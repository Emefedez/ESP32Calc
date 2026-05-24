# Firmware Tools

## Screen Color Probe

`screen_color_probe.py` captures a small macOS screen region repeatedly and shows
the dominant colors. It is useful for checking whether Wokwi or a camera preview
of the e-paper panel is actually producing black/white pixels, noise, or only a
corner line.

```bash
cd Firmware
python3 tools/screen_color_probe.py
```

Use **Select From Screenshot** for the reliable crop path. It takes a full
screen capture, shows a scaled preview inside the app, and maps your dragged
rectangle back to the real screenshot pixels. Use **Fit** to see the whole
desktop, **1:1** for exact edges, and **Zoom In/Out** when the display is hard
to isolate. The selected crop is immediately logged and reused by **One Shot**
and **Start**.

Use **Overlay Select** only as a faster fallback. If that repeated region is
offset on a Retina display, adjust the `scale` field, usually between `1` and
`2`.

Every capture appends a text report to `tools/screen_color_probe.log`, including
the average color, luma range, dark/mid/light percentages, top quantized colors,
and a small luma map of the selected crop only. It also overwrites
`tools/screen_color_probe_latest_crop.png` with the exact pixels that were
analyzed, so you can verify the crop before trusting the numbers. **Clear Log**
removes both generated files.

The app uses the built-in macOS `screencapture` command, so macOS may require
Screen Recording permission for the terminal/Codex app running the script.
