#!/usr/bin/env python3
"""Tiny macOS screen-region color probe for e-paper/Wokwi debugging.

It uses only the Python standard library plus the macOS `screencapture` tool.
That keeps it usable from the existing firmware repo without installing Pillow,
mss, OpenCV, or NumPy.

CLI:

python3 screen_color_probe.py --cli (--clear if you want to clean the log)
"""

from __future__ import annotations

import argparse
import collections
import datetime as _datetime
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import time
import tkinter as tk
from tkinter import messagebox, ttk
import zlib


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
DEFAULT_LOG_PATH = Path(__file__).with_name("screen_color_probe.log")
DEFAULT_LATEST_CROP_PATH = Path(__file__).with_name("screen_color_probe_latest_crop.png")
DEFAULT_CONFIG_PATH = Path(__file__).with_name("screen_color_probe_config.json")

def png_chunk(kind: bytes, data: bytes) -> bytes:
    return (
        struct.pack(">I", len(data))
        + kind
        + data
        + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    )


def write_png_rgb(
    path: Path,
    width: int,
    height: int,
    pixels: list[tuple[int, int, int]],
):
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        start = y * width
        for red, green, blue in pixels[start : start + width]:
            raw.extend((red, green, blue))

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as handle:
        handle.write(PNG_SIGNATURE)
        handle.write(png_chunk(b"IHDR", ihdr))
        handle.write(png_chunk(b"IDAT", zlib.compress(bytes(raw), level=6)))
        handle.write(png_chunk(b"IEND", b""))


def paeth_predictor(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def read_png_rgb(path: str) -> tuple[int, int, list[tuple[int, int, int]]]:
    with open(path, "rb") as handle:
        blob = handle.read()

    if not blob.startswith(PNG_SIGNATURE):
        raise ValueError("capture is not a PNG")

    offset = len(PNG_SIGNATURE)
    width = height = bit_depth = color_type = None
    palette: list[tuple[int, int, int]] = []
    idat = bytearray()

    while offset < len(blob):
        if offset + 8 > len(blob):
            raise ValueError("truncated PNG chunk")
        length = struct.unpack(">I", blob[offset : offset + 4])[0]
        kind = blob[offset + 4 : offset + 8]
        data_start = offset + 8
        data_end = data_start + length
        data = blob[data_start:data_end]
        offset = data_end + 4

        if kind == b"IHDR":
            width, height, bit_depth, color_type, _compression, _filter, _interlace = struct.unpack(
                ">IIBBBBB", data
            )
        elif kind == b"PLTE":
            palette = [
                tuple(data[i : i + 3])  # type: ignore[arg-type]
                for i in range(0, len(data), 3)
                if i + 3 <= len(data)
            ]
        elif kind == b"IDAT":
            idat.extend(data)
        elif kind == b"IEND":
            break

    if width is None or height is None or bit_depth is None or color_type is None:
        raise ValueError("PNG is missing IHDR")
    if bit_depth != 8:
        raise ValueError(f"unsupported PNG bit depth {bit_depth}; expected 8")

    channels_by_type = {
        0: 1,  # grayscale
        2: 3,  # RGB
        3: 1,  # indexed color
        4: 2,  # grayscale + alpha
        6: 4,  # RGBA
    }
    if color_type not in channels_by_type:
        raise ValueError(f"unsupported PNG color type {color_type}")

    raw = zlib.decompress(bytes(idat))
    channels = channels_by_type[color_type]
    stride = width * channels
    previous = bytearray(stride)
    position = 0
    pixels: list[tuple[int, int, int]] = []

    for _y in range(height):
        filter_type = raw[position]
        position += 1
        row = bytearray(raw[position : position + stride])
        position += stride

        for i in range(stride):
            left = row[i - channels] if i >= channels else 0
            up = previous[i]
            up_left = previous[i - channels] if i >= channels else 0
            if filter_type == 1:
                row[i] = (row[i] + left) & 0xFF
            elif filter_type == 2:
                row[i] = (row[i] + up) & 0xFF
            elif filter_type == 3:
                row[i] = (row[i] + ((left + up) // 2)) & 0xFF
            elif filter_type == 4:
                row[i] = (row[i] + paeth_predictor(left, up, up_left)) & 0xFF
            elif filter_type != 0:
                raise ValueError(f"unsupported PNG filter {filter_type}")

        for x in range(width):
            base = x * channels
            if color_type == 0:
                value = row[base]
                pixels.append((value, value, value))
            elif color_type == 2:
                pixels.append((row[base], row[base + 1], row[base + 2]))
            elif color_type == 3:
                pixels.append(palette[row[base]])
            elif color_type == 4:
                value = row[base]
                alpha = row[base + 1]
                blended = (value * alpha + 255 * (255 - alpha)) // 255
                pixels.append((blended, blended, blended))
            elif color_type == 6:
                red, green, blue, alpha = row[base : base + 4]
                pixels.append(
                    (
                        (red * alpha + 255 * (255 - alpha)) // 255,
                        (green * alpha + 255 * (255 - alpha)) // 255,
                        (blue * alpha + 255 * (255 - alpha)) // 255,
                    )
                )

        previous = row

    return width, height, pixels


def luminance(color: tuple[int, int, int]) -> int:
    red, green, blue = color
    return (red * 299 + green * 587 + blue * 114) // 1000


def quantize(color: tuple[int, int, int], step: int = 16) -> tuple[int, int, int]:
    r, g, b = color
    def q(c: int) -> int:
        return min(255, ((c + step // 2) // step) * step)

    return (q(r), q(g), q(b))


def hex_color(color: tuple[int, int, int]) -> str:
    return "#{:02X}{:02X}{:02X}".format(*color)


def crop_pixels(
    width: int,
    height: int,
    pixels: list[tuple[int, int, int]],
    x: int,
    y: int,
    crop_width: int,
    crop_height: int,
) -> list[tuple[int, int, int]]:
    _, _, out = crop_pixels_with_size(width, height, pixels, x, y, crop_width, crop_height)
    return out


def crop_pixels_with_size(
    width: int,
    height: int,
    pixels: list[tuple[int, int, int]],
    x: int,
    y: int,
    crop_width: int,
    crop_height: int,
) -> tuple[int, int, list[tuple[int, int, int]]]:
    x0 = max(0, min(width - 1, x))
    y0 = max(0, min(height - 1, y))
    x1 = max(x0 + 1, min(width, x0 + crop_width))
    y1 = max(y0 + 1, min(height, y0 + crop_height))
    actual_width = x1 - x0
    actual_height = y1 - y0
    out: list[tuple[int, int, int]] = []
    for yy in range(y0, y1):
        start = yy * width + x0
        out.extend(pixels[start : start + actual_width])
    return actual_width, actual_height, out


def build_luma_map(
    width: int,
    height: int,
    pixels: list[tuple[int, int, int]],
    max_width: int = 80,
    max_height: int = 32,
) -> list[str]:
    if width <= 0 or height <= 0 or not pixels:
        return []

    out_width = min(width, max_width)
    out_height = min(height, max_height)
    lines: list[str] = []
    for out_y in range(out_height):
        y0 = out_y * height // out_height
        y1 = max(y0 + 1, (out_y + 1) * height // out_height)
        chars: list[str] = []
        for out_x in range(out_width):
            x0 = out_x * width // out_width
            x1 = max(x0 + 1, (out_x + 1) * width // out_width)
            total = 0
            count = 0
            has_red = False
            has_black = False
            for yy in range(y0, y1):
                row = yy * width
                for xx in range(x0, x1):
                    r, g, b = pixels[row + xx]
                    total += luminance((r, g, b))
                    count += 1
                    if r > 150 and g < 100 and b < 100:
                        has_red = True
                    if r < 40 and g < 40 and b < 40:
                        has_black = True
                        
            if has_red:
                chars.append("R")
            elif has_black:
                chars.append("M")
            else:
                avg = total // max(1, count)
                if avg < 80:
                    chars.append("#")
                elif avg < 176:
                    chars.append("+")
                else:
                    chars.append(".")
        lines.append("".join(chars))
    return lines


class RegionSelector(tk.Toplevel):
    def __init__(self, master: tk.Tk, callback):
        super().__init__(master)
        self.callback = callback
        self.title("Drag & Resize to Target")
        self.attributes("-topmost", True)
        self.attributes("-alpha", 0.6)
        self.geometry("300x200")
        
        self.canvas = tk.Canvas(self, bg="cyan", cursor="crosshair")
        self.canvas.pack(fill="both", expand=True)
        
        # Un botón en el centro para confirmar
        btn = ttk.Button(self.canvas, text="Set Region Here", command=self.confirm)
        btn.place(relx=0.5, rely=0.5, anchor="center")
        
        self.bind("<Escape>", lambda _event: self.destroy())

    def confirm(self):
        # Tomamos las coordenadas globales del Canvas interior
        x = self.canvas.winfo_rootx()
        y = self.canvas.winfo_rooty()
        width = self.canvas.winfo_width()
        height = self.canvas.winfo_height()
        self.destroy()
        if width >= 2 and height >= 2:
            self.callback(x, y, width, height)


class ScreenshotRegionSelector(tk.Toplevel):
    def __init__(self, master: tk.Tk, image_path: str, callback):
        super().__init__(master)
        self.image_path = image_path
        self.callback = callback
        self.start: tuple[int, int] | None = None
        self.rect: int | None = None
        self.zoom_divisor = 1

        self.title("Select Screen Block")
        self.window_width = min(master.winfo_screenwidth() - 80, 1400)
        self.window_height = min(master.winfo_screenheight() - 120, 900)
        self.geometry(f"{self.window_width}x{self.window_height}")
        self.minsize(640, 420)

        self.source_image = tk.PhotoImage(file=image_path)
        self.image = self.source_image

        toolbar = ttk.Frame(self, padding=(8, 8, 8, 0))
        toolbar.pack(fill="x")
        self.zoom_text = tk.StringVar(value="")
        ttk.Button(toolbar, text="Fit", command=self.zoom_fit).pack(side="left")
        ttk.Button(toolbar, text="1:1", command=lambda: self.set_zoom(1)).pack(
            side="left", padx=(6, 0)
        )
        ttk.Button(toolbar, text="Zoom In", command=self.zoom_in).pack(side="left", padx=(6, 0))
        ttk.Button(toolbar, text="Zoom Out", command=self.zoom_out).pack(side="left", padx=(6, 0))
        ttk.Label(toolbar, textvariable=self.zoom_text).pack(side="left", padx=(12, 0))

        frame = ttk.Frame(self)
        frame.pack(fill="both", expand=True)
        self.canvas = tk.Canvas(frame, bg="#202020", cursor="crosshair", highlightthickness=0)
        x_scroll = ttk.Scrollbar(frame, orient="horizontal", command=self.canvas.xview)
        y_scroll = ttk.Scrollbar(frame, orient="vertical", command=self.canvas.yview)
        self.canvas.configure(xscrollcommand=x_scroll.set, yscrollcommand=y_scroll.set)

        self.canvas.grid(row=0, column=0, sticky="nsew")
        y_scroll.grid(row=0, column=1, sticky="ns")
        x_scroll.grid(row=1, column=0, sticky="ew")
        frame.rowconfigure(0, weight=1)
        frame.columnconfigure(0, weight=1)

        self.image_item = self.canvas.create_image(0, 0, anchor="nw", image=self.image)
        self.canvas.bind("<ButtonPress-1>", self.on_press)
        self.canvas.bind("<B1-Motion>", self.on_drag)
        self.canvas.bind("<ButtonRelease-1>", self.on_release)
        self.canvas.bind("<Motion>", self.on_motion)
        self.bind("<Escape>", lambda _event: self.cancel())
        self.protocol("WM_DELETE_WINDOW", self.cancel)

        self.info_text = tk.StringVar(value="")
        ttk.Label(
            self,
            textvariable=self.info_text,
            padding=8,
        ).pack(anchor="w")
        self.zoom_fit()

    def zoom_fit(self):
        divisor = max(
            1,
            math.ceil(
                max(
                    self.source_image.width() / max(1, self.window_width - 80),
                    self.source_image.height() / max(1, self.window_height - 150),
                )
            ),
        )
        self.set_zoom(divisor)

    def zoom_in(self):
        self.set_zoom(max(1, self.zoom_divisor - 1))

    def zoom_out(self):
        self.set_zoom(self.zoom_divisor + 1)

    def set_zoom(self, divisor: int):
        self.zoom_divisor = max(1, divisor)
        if self.zoom_divisor == 1:
            self.image = self.source_image
        else:
            self.image = self.source_image.subsample(self.zoom_divisor, self.zoom_divisor)
        self.canvas.itemconfigure(self.image_item, image=self.image)
        self.canvas.configure(scrollregion=(0, 0, self.image.width(), self.image.height()))
        if self.rect is not None:
            self.canvas.delete(self.rect)
            self.rect = None
        self.start = None
        self.zoom_text.set(
            f"view 1:{self.zoom_divisor}  "
            f"source {self.source_image.width()}x{self.source_image.height()} px"
        )
        self.info_text.set(
            "Drag the display block. Use Fit to see the whole screen or 1:1 for exact edges. "
            "The saved crop is real screenshot pixels, not the scaled preview."
        )

    def display_xy(self, event) -> tuple[int, int]:
        x = int(self.canvas.canvasx(event.x))
        y = int(self.canvas.canvasy(event.y))
        x = max(0, min(self.image.width() - 1, x))
        y = max(0, min(self.image.height() - 1, y))
        return x, y

    def source_rect_from_display(
        self,
        x0: int,
        y0: int,
        x1: int,
        y1: int,
    ) -> tuple[int, int, int, int]:
        left = min(x0, x1) * self.zoom_divisor
        top = min(y0, y1) * self.zoom_divisor
        right = min(self.source_image.width(), (max(x0, x1) + 1) * self.zoom_divisor)
        bottom = min(self.source_image.height(), (max(y0, y1) + 1) * self.zoom_divisor)
        return left, top, max(1, right - left), max(1, bottom - top)

    def on_motion(self, event):
        x, y = self.display_xy(event)
        source_x = min(self.source_image.width() - 1, x * self.zoom_divisor)
        source_y = min(self.source_image.height() - 1, y * self.zoom_divisor)
        self.info_text.set(
            f"source x={source_x} y={source_y}. Drag the display block; Esc cancels."
        )

    def on_press(self, event):
        self.start = self.display_xy(event)
        x, y = self.start
        self.rect = self.canvas.create_rectangle(x, y, x, y, outline="#00FFFF", width=2)

    def on_drag(self, event):
        if self.start is None or self.rect is None:
            return
        x, y = self.display_xy(event)
        self.canvas.coords(self.rect, self.start[0], self.start[1], x, y)
        left, top, width, height = self.source_rect_from_display(
            self.start[0], self.start[1], x, y
        )
        self.info_text.set(f"crop x={left} y={top} w={width} h={height}")

    def on_release(self, event):
        if self.start is None:
            self.cancel()
            return
        x0, y0 = self.start
        x1, y1 = self.display_xy(event)
        x, y, width, height = self.source_rect_from_display(x0, y0, x1, y1)
        if width >= 2 and height >= 2:
            self.callback(self.image_path, x, y, width, height)
        self.destroy()

    def cancel(self):
        try:
            os.unlink(self.image_path)
        except OSError:
            pass
        self.destroy()


class ColorProbeApp:
    def __init__(self, root: tk.Tk, headless: bool = False):
        self.root = root
        self.headless = headless
        self.running = False
        self.last_capture_seconds = 0.0
        self.last_pixels: list[tuple[int, int, int]] | None = None
        self.capture_count = 0
        self.log_path = DEFAULT_LOG_PATH
        self.latest_crop_path = DEFAULT_LATEST_CROP_PATH

        self.x_var = tk.IntVar(value=0)
        self.y_var = tk.IntVar(value=0)
        self.w_var = tk.IntVar(value=250)
        self.h_var = tk.IntVar(value=122)
        self.interval_var = tk.IntVar(value=10)
        self.scale_var = tk.DoubleVar(value=self.detect_screen_scale())
        self.status = tk.StringVar(value="")
        self.summary = tk.StringVar(value="")

        self.load_config()
        for var in (self.x_var, self.y_var, self.w_var, self.h_var, self.interval_var, self.scale_var):
            var.trace_add("write", lambda *a: self.save_config())

        if self.headless:
            return

        root.title("ESP32Calc Screen Color Probe")
        root.geometry("980x680")
        root.minsize(860, 560)

        controls = ttk.Frame(root, padding=10)
        controls.pack(fill="x")

        for label, variable, column in (
            ("x", self.x_var, 0),
            ("y", self.y_var, 2),
            ("w", self.w_var, 4),
            ("h", self.h_var, 6),
            ("ms", self.interval_var, 8),
        ):
            ttk.Label(controls, text=label).grid(row=0, column=column, sticky="e")
            ttk.Spinbox(controls, from_=-10000, to=10000, textvariable=variable, width=7).grid(
                row=0, column=column + 1, padx=(2, 8)
            )

        ttk.Label(controls, text="scale").grid(row=0, column=10, sticky="e")
        ttk.Spinbox(
            controls,
            from_=0.25,
            to=4.0,
            increment=0.25,
            textvariable=self.scale_var,
            width=6,
        ).grid(row=0, column=11, padx=(2, 0))

        ttk.Button(controls, text="Overlay Select", command=self.select_region).grid(
            row=1, column=0, columnspan=2, pady=(8, 0), sticky="ew"
        )
        ttk.Button(controls, text="Select From Screenshot", command=self.select_from_screenshot).grid(
            row=1, column=2, columnspan=2, pady=(8, 0), sticky="ew"
        )
        ttk.Button(controls, text="One Shot", command=self.capture_once).grid(
            row=1, column=4, columnspan=2, pady=(8, 0), sticky="ew"
        )
        ttk.Button(controls, text="Start", command=self.start).grid(
            row=1, column=6, columnspan=2, pady=(8, 0), sticky="ew"
        )
        ttk.Button(controls, text="Stop", command=self.stop).grid(
            row=1, column=8, columnspan=2, pady=(8, 0), sticky="ew"
        )
        ttk.Button(controls, text="Clear Log", command=self.clear_log).grid(
            row=1, column=10, columnspan=2, pady=(8, 0), sticky="ew"
        )

        self.status = tk.StringVar(
            value=(
                "Select a Wokwi/display region, then capture. "
                f"Log: {self.log_path}; latest crop: {self.latest_crop_path}."
            )
        )
        ttk.Label(root, textvariable=self.status, padding=(10, 0)).pack(anchor="w")

        self.summary = tk.StringVar(value="")
        ttk.Label(root, textvariable=self.summary, padding=(10, 6), justify="left").pack(
            anchor="w"
        )

        self.canvas = tk.Canvas(root, height=320, bg="white", highlightthickness=1)
        self.canvas.pack(fill="both", expand=True, padx=10, pady=(0, 10))

    def load_config(self):
        if DEFAULT_CONFIG_PATH.exists():
            try:
                with DEFAULT_CONFIG_PATH.open("r") as f:
                    cfg = json.load(f)
                self.x_var.set(cfg.get("x", self.x_var.get()))
                self.y_var.set(cfg.get("y", self.y_var.get()))
                self.w_var.set(cfg.get("w", self.w_var.get()))
                self.h_var.set(cfg.get("h", self.h_var.get()))
                self.interval_var.set(cfg.get("interval", self.interval_var.get()))
                self.scale_var.set(cfg.get("scale", self.scale_var.get()))
            except Exception:
                pass

    def save_config(self):
        try:
            cfg = {
                "x": self.x_var.get(),
                "y": self.y_var.get(),
                "w": self.w_var.get(),
                "h": self.h_var.get(),
                "interval": self.interval_var.get(),
                "scale": self.scale_var.get(),
            }
            with DEFAULT_CONFIG_PATH.open("w") as f:
                json.dump(cfg, f)
        except Exception:
            pass

    def detect_screen_scale(self) -> float:
        try:
            return max(0.25, round(self.root.winfo_fpixels("1i") / 72.0, 2))
        except tk.TclError:
            return 1.0

    def select_region(self):
        RegionSelector(self.root, self.set_region)

    def set_region(self, x: int, y: int, width: int, height: int):
        self.x_var.set(x)
        self.y_var.set(y)
        self.w_var.set(width)
        self.h_var.set(height)
        self.status.set(
            f"Overlay region set to x={x}, y={y}, w={width}, h={height}; "
            "use scale if repeated capture is offset."
        )

    def select_from_screenshot(self):
        path = self.capture_fullscreen_png()
        ScreenshotRegionSelector(self.root, path, self.set_region_from_screenshot)

    def set_region_from_screenshot(
        self,
        image_path: str,
        x: int,
        y: int,
        width: int,
        height: int,
    ):
        try:
            scale = self.detect_screen_scale()
            self.scale_var.set(scale)
            self.x_var.set(round(x / scale))
            self.y_var.set(round(y / scale))
            self.w_var.set(round(width / scale))
            self.h_var.set(round(height / scale))
            screen_width, screen_height, screen_pixels = read_png_rgb(image_path)
            crop_width, crop_height, crop = crop_pixels_with_size(
                screen_width,
                screen_height,
                screen_pixels,
                x,
                y,
                width,
                height,
            )
            self.render_stats(crop_width, crop_height, crop, "screenshot crop")
            self.status.set(
                f"Screenshot crop set to x={x}, y={y}, w={crop_width}, h={crop_height}. "
                f"Logged to {self.log_path}; PNG saved to {self.latest_crop_path}"
            )
        finally:
            try:
                os.unlink(image_path)
            except OSError:
                pass

    def start(self):
        self.running = True
        self.last_pixels = None
        self.capture_once()

    def stop(self):
        self.running = False
        self.status.set("Stopped")

    def clear_log(self):
        self.capture_count = 0
        for path in (self.log_path, self.latest_crop_path):
            try:
                path.unlink()
            except FileNotFoundError:
                pass
        self.summary.set("")
        if hasattr(self, 'canvas'):
            self.canvas.delete("all")
        self.status.set(f"Cleared {self.log_path} and {self.latest_crop_path}")

    def schedule_next(self):
        if not self.running:
            return
        # Use a very short interval for "real time" fast capture
        self.root.after(max(10, self.interval_var.get()), self.capture_once)

    def capture_once(self):
        try:
            width, height, pixels = self.capture_region()
            
            # Fast check: skip logging if nothing changed (avoids disk and UI overhead)
            if self.last_pixels == pixels:
                self.last_capture_seconds = time.time()
                return
                
            self.last_pixels = pixels
            self.render_stats(width, height, pixels, "region")
            self.last_capture_seconds = time.time()
        except Exception as exc:  # noqa: BLE001 - GUI should show recoverable capture failures.
            self.running = False
            messagebox.showerror(
                "Capture failed",
                f"{exc}\n\nIf macOS blocks capture, grant Screen Recording permission "
                "to the app running this script, then restart it.",
            )
        finally:
            self.schedule_next()

    def pick_block_once(self):
        try:
            width, height, pixels = self.capture_interactive_block()
            self.render_stats(width, height, pixels, "interactive block")
            self.last_capture_seconds = time.time()
        except Exception as exc:  # noqa: BLE001 - GUI should show recoverable capture failures.
            messagebox.showerror(
                "Capture failed",
                f"{exc}\n\nIf macOS blocks capture, grant Screen Recording permission "
                "to the app running this script, then restart it.",
            )

    def capture_interactive_block(self) -> tuple[int, int, list[tuple[int, int, int]]]:
        fd, path = tempfile.mkstemp(prefix="esp32calc_probe_pick_", suffix=".png")
        os.close(fd)
        try:
            self.root.withdraw()
            self.root.update()
            time.sleep(0.15)
            subprocess.run(
                ["screencapture", "-i", "-s", path],
                check=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                text=True,
            )
            if not os.path.exists(path) or os.path.getsize(path) == 0:
                raise RuntimeError("interactive capture was cancelled or empty")
            return read_png_rgb(path)
        except subprocess.CalledProcessError as exc:
            raise RuntimeError(exc.stderr.strip() or "interactive screencapture failed") from exc
        finally:
            self.root.deiconify()
            try:
                os.unlink(path)
            except OSError:
                pass

    def capture_fullscreen_png(self) -> str:
        fd, path = tempfile.mkstemp(prefix="esp32calc_fullscreen_", suffix=".png")
        os.close(fd)
        try:
            subprocess.run(
                ["screencapture", "-x", path],
                check=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                text=True,
            )
            if not os.path.exists(path) or os.path.getsize(path) == 0:
                raise RuntimeError("fullscreen capture produced an empty file")
            return path
        except subprocess.CalledProcessError as exc:
            try:
                os.unlink(path)
            except OSError:
                pass
            raise RuntimeError(exc.stderr.strip() or "fullscreen screencapture failed") from exc

    def capture_region(self) -> tuple[int, int, list[tuple[int, int, int]]]:
        # 'screencapture -R' en macOS espera las coordenadas en "Puntos" (coordenadas lógicas)
        # por lo que no hace falta multiplicarlas por el factor de escala de pantallas Retina.
        x = self.x_var.get()
        y = self.y_var.get()
        width = max(1, self.w_var.get())
        height = max(1, self.h_var.get())

        fd, path = tempfile.mkstemp(prefix="esp32calc_region_", suffix=".png")
        os.close(fd)
        try:
            subprocess.run(
                ["screencapture", "-R", f"{x},{y},{width},{height}", "-x", path],
                check=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                text=True,
            )
            return read_png_rgb(path)
        finally:
            try:
                os.unlink(path)
            except OSError:
                pass

    def render_stats(
        self,
        width: int,
        height: int,
        pixels: list[tuple[int, int, int]],
        source: str,
    ):
        total = max(1, len(pixels))
        quantized = collections.Counter(quantize(pixel) for pixel in pixels)
        top = quantized.most_common(14)

        lumas = [luminance(pixel) for pixel in pixels]
        dark = sum(1 for value in lumas if value < 64)
        mid = sum(1 for value in lumas if 64 <= value <= 192)
        light = total - dark - mid
        avg: tuple[int, int, int] = (
            sum(pixel[0] for pixel in pixels) // total,
            sum(pixel[1] for pixel in pixels) // total,
            sum(pixel[2] for pixel in pixels) // total,
        )
        raw_unique = len(set(pixels))
        self.capture_count += 1
        captured_at = _datetime.datetime.now().isoformat(timespec="seconds")
        write_png_rgb(self.latest_crop_path, width, height, pixels)
        luma_map = build_luma_map(width, height, pixels)

        self.status.set(
            f"Captured {width}x{height} from {source} at {time.strftime('%H:%M:%S')} "
            f"({raw_unique} raw colors, {len(quantized)} quantized). "
            f"Logged to {self.log_path}; PNG {self.latest_crop_path}"
        )
        self.summary.set(
            f"avg {hex_color(avg)}  dark {dark / total:.1%}  mid {mid / total:.1%}  "
            f"light {light / total:.1%}  luma {min(lumas)}..{max(lumas)}"
        )
        self.append_log(
            captured_at=captured_at,
            source=source,
            capture_size=(width, height),
            average=avg,
            dark=dark / total,
            mid=mid / total,
            light=light / total,
            luma_range=(min(lumas), max(lumas)),
            raw_unique=raw_unique,
            quantized_unique=len(quantized),
            top=top,
            luma_map=luma_map,
            total=total,
        )
        
        if self.headless:
            print(self.status.get())
            return

        self.canvas.delete("all")
        pad = 14
        bar_width = max(1, self.canvas.winfo_width() - pad * 2)
        y = pad
        for color, count in top:
            percentage = count / total
            fill = hex_color(color)
            text_fill = "white" if luminance(color) < 128 else "black"
            swatch_width = max(24, int(bar_width * percentage))
            self.canvas.create_rectangle(pad, y, pad + swatch_width, y + 24, fill=fill, outline="")
            self.canvas.create_rectangle(pad, y, pad + 58, y + 24, outline="#777777")
            self.canvas.create_text(
                pad + 70,
                y + 12,
                anchor="w",
                text=f"{fill}  {count:>7} px  {percentage:>6.2%}",
                fill="black",
                font=("Menlo", 12),
            )
            self.canvas.create_text(
                pad + 29,
                y + 12,
                text=fill,
                fill=text_fill,
                font=("Menlo", 9),
            )
            y += 30

    def append_log(
        self,
        *,
        captured_at: str,
        source: str,
        capture_size: tuple[int, int],
        average: tuple[int, int, int],
        dark: float,
        mid: float,
        light: float,
        luma_range: tuple[int, int],
        raw_unique: int,
        quantized_unique: int,
        top: list[tuple[tuple[int, int, int], int]],
        luma_map: list[str],
        total: int,
    ):
        if source in {"region", "screenshot crop"}:
            scale = max(0.25, float(self.scale_var.get()))
            source_detail = (
                f"x={self.x_var.get()} y={self.y_var.get()} "
                f"w={self.w_var.get()} h={self.h_var.get()} scale={scale:g}"
            )
        else:
            source_detail = "native macOS interactive block"

        lines = [
            f"[{captured_at}] capture #{self.capture_count}",
            f"source: {source} ({source_detail})",
            f"captured_size: {capture_size[0]}x{capture_size[1]} pixels",
            (
                f"avg: {hex_color(average)}  dark: {dark:.3%}  mid: {mid:.3%}  "
                f"light: {light:.3%}  luma: {luma_range[0]}..{luma_range[1]}"
            ),
            f"colors: raw_unique={raw_unique} quantized_unique={quantized_unique}",
            f"latest_crop_png: {self.latest_crop_path}",
            "top_quantized:",
        ]
        for color, count in top:
            lines.append(f"  {hex_color(color)}  {count} px  {count / total:.3%}")
        if luma_map:
            lines.append("luma_map (# dark, + mid, . light):")
            lines.extend(f"  {line}" for line in luma_map)
        lines.append("")

        self.log_path.parent.mkdir(parents=True, exist_ok=True)
        with self.log_path.open("a", encoding="utf-8") as handle:
            handle.write("\n".join(lines))
            handle.write("\n")


def main():
    parser = argparse.ArgumentParser(description="ESP32Calc Screen Color Probe")
    parser.add_argument("--cli", action="store_true", help="Run without GUI using last saved region")
    parser.add_argument("--clear", action="store_true", help="Clear log files before starting")
    args = parser.parse_args()

    root = tk.Tk()
    if args.cli:
        root.withdraw()
        app = ColorProbeApp(root, headless=True)
        if args.clear:
            app.clear_log()
        print("Starting headless CLI capture mode using last coordinates... (Press Ctrl+C to stop)")
        app.start()
        try:
            root.mainloop()
        except KeyboardInterrupt:
            print("\nStopping capture...")
            root.destroy()
    else:
        app = ColorProbeApp(root)
        if args.clear:
            app.clear_log()
        root.mainloop()

if __name__ == "__main__":
    main()
