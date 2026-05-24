"""
Host-side smoke test for MicroPython firmware imports and startup wiring.

Run from the repo root:
    python3 Firmware/tests/host_smoke.py
"""
import importlib
import pathlib
import struct
import sys
import time
import types

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))


def install_micropython_shims():
    dummy = types.ModuleType("_dummy")
    dummy.const = lambda value: value  # type: ignore[attr-defined]

    micropython = types.ModuleType("micropython")
    micropython.const = lambda value: value  # type: ignore[attr-defined]
    sys.modules["micropython"] = micropython

    machine = types.ModuleType("machine")

    class Pin:  # type: ignore[no-redef]
        IN = 0
        OUT = 1
        PULL_UP = 2

        def __init__(self, pin, *args, **kwargs):
            if pin == -1:
                raise ValueError("invalid pin")
            self.pin = pin
            self._value = kwargs.get("value", 0)

        def init(self, mode=None, pull=None, value=None):
            if value is not None:
                self._value = value

        def value(self, value=None):
            if value is not None:
                self._value = value
            return self._value

        def __call__(self, value=None):
            return self.value(value)

    class SPI:  # type: ignore[no-redef]
        def __init__(self, bus, **kwargs):
            self.bus = bus
            self.kwargs = kwargs
            if kwargs.get("miso") is not None and getattr(kwargs["miso"], "pin", None) == -1:
                raise ValueError("invalid pin")

        def write(self, data):
            pass

    machine.Pin = Pin  # type: ignore[attr-defined]
    machine.SPI = SPI  # type: ignore[attr-defined]
    sys.modules["machine"] = machine

    framebuf = types.ModuleType("framebuf")
    framebuf.MONO_VLSB = 0  # type: ignore[attr-defined]
    framebuf.MONO_HLSB = 3  # type: ignore[attr-defined]

    class FrameBuffer:  # type: ignore[no-redef]
        def __init__(self, buf, width, height, fmt):
            if fmt == framebuf.MONO_HLSB:
                expected = ((width + 7) // 8) * height
            elif fmt == framebuf.MONO_VLSB:
                # MONO_VLSB is column-major: ceil(H/8) bytes per column
                expected = width * ((height + 7) // 8)
            else:
                raise ValueError("unsupported framebuf format")
            if len(buf) < expected:
                raise ValueError("framebuf too small")

        def fill(self, *args):
            pass

        def text(self, *args):
            pass

        def hline(self, *args):
            pass

        def vline(self, *args):
            pass

        def rect(self, *args):
            pass

        def fill_rect(self, *args):
            pass

        def pixel(self, *args):
            pass

        def blit(self, *args):
            pass

    framebuf.FrameBuffer = FrameBuffer  # type: ignore[attr-defined]
    sys.modules["framebuf"] = framebuf

    ustruct = types.ModuleType("ustruct")
    ustruct.pack = struct.pack  # type: ignore[attr-defined]
    sys.modules["ustruct"] = ustruct

    thread = types.ModuleType("_thread")

    class _Lock:  # type: ignore[no-redef]
        def acquire(self, wait=1):
            return True

        def release(self):
            pass

    thread.allocate_lock = lambda: _Lock()  # type: ignore[attr-defined]

    def _start_new_thread(fn, args):
        pass  # no-op on host

    thread.start_new_thread = _start_new_thread  # type: ignore[attr-defined]
    sys.modules["_thread"] = thread

    time.sleep_ms = lambda ms: None  # type: ignore[attr-defined]
    time.sleep_us = lambda us: None  # type: ignore[attr-defined]
    time.ticks_ms = lambda: 0  # type: ignore[attr-defined]
    time.ticks_diff = lambda now, start: now - start  # type: ignore[attr-defined]


def smoke_imports():
    failures = []
    for path in ROOT.rglob("*.py"):
        parts = path.parts
        if "build" in parts or "tools" in parts or "tests" in parts or path.name == "main.py":
            continue
        mod = ".".join(path.with_suffix("").relative_to(ROOT).parts)
        try:
            importlib.import_module(mod)
        except Exception as exc:
            failures.append((mod, type(exc).__name__, str(exc)))

    if failures:
        for mod, name, message in failures:
            print("FAIL", mod, name, message)
        raise SystemExit(1)


def smoke_startup_objects():
    from machine import SPI, Pin
    from config.pins import (
        PIN_EPD_BUSY,
        PIN_EPD_CS,
        PIN_EPD_DC,
        PIN_EPD_MOSI,
        PIN_EPD_PWR,
        PIN_EPD_RST,
        PIN_EPD_SCLK,
        EPD_SPI_BAUD,
    )
    from drivers.epaper import EPD
    from drivers.framebuf import Display
    from drivers.keypad import Keypad
    from screens.calc_screen import CalcScreen

    Pin(PIN_EPD_PWR, Pin.OUT, value=1)
    spi = SPI(2, baudrate=EPD_SPI_BAUD, sck=Pin(PIN_EPD_SCLK), mosi=Pin(PIN_EPD_MOSI))
    epd = EPD(
        spi,
        Pin(PIN_EPD_CS, Pin.OUT, value=1),
        Pin(PIN_EPD_DC, Pin.OUT, value=0),
        Pin(PIN_EPD_RST, Pin.OUT, value=0),
        Pin(PIN_EPD_BUSY, Pin.IN),
    )
    epd.init()
    disp = Display(epd)
    kpd = Keypad()
    screen = CalcScreen(disp, kpd)
    screen.draw()


if __name__ == "__main__":
    install_micropython_shims()
    smoke_imports()
    smoke_startup_objects()
    print("host smoke ok")
