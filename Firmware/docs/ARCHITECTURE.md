# Firmware Architecture

## Language Choice

The base is ESP-IDF C++ rather than MicroPython or Rust. Rust would let us use
`weact-studio-epd` directly, but the rest of the requested stack is easier to
stage in ESP-IDF today: dual-core FreeRTOS pinning, SDSPI, ADC, Wi-Fi/BLE, and
the existing PlatformIO/ESP-IDF toolchain.

The display layer keeps the key part of the Rust driver: SSD1680 init, 128x250
RAM layout, full refresh, fast refresh, and the partial LUT. That avoids the
known-bad deprecated display code while keeping a path that already worked on
the panel.

## Task Split

- Core 1: keyboard scan and UI/display updates.
- Core 0: battery monitor, SD/program service, calculation engine, and future
  wireless service.
- `AppEvent` queue: input, battery, storage, calculation, and wireless events
  flow into the UI.

This is intentionally small. It is not an OS yet; it is the skeleton that lets
the calculator boot, scan keys, show the menu, and grow each mode independently.

## Display

The logical UI canvas is 250x128 landscape, matching the Rust driver's 128x250
buffer after `Rotate90`. The WeAct panel physically exposes only 122 of those
128 rows. `MonoCanvas::to_epd_native` rotates and packs the logical framebuffer
into the controller buffer.

The driver writes both the B/W buffer and the controller's second buffer on full
updates, matching the Rust driver behavior. Fast refresh writes the partial LUT
before using the fast update control value.

## Keyboard

`pins.h` is the GPIO list. `keymap.cpp` is the key list. The matrix scanner uses
open-drain columns and pulled-up rows, debounces transitions, then publishes
press/release events. `SHIFT` and `ALPHA` are UI booleans. `MODE` always returns
to the menu.

## Drawing And Refresh

`MonoCanvas` is the primary drawing API. It stores a 250x128 logical 1-bit
framebuffer and provides both low-level primitives (`set_pixel`, `line`,
`rect`, `circle`, `triangle`, `draw_text`) and familiar convenience methods
(`DrawLine`, `DrawLineStrip`, `DrawRectangleLines`, `DrawText`, and related
helpers). The graph screen uses this path directly; it does not initialize
an intermediate RGB565 renderer.

Every drawing operation updates a `CanvasUpdateHint`. The hint has two refresh
levels:

- `Partial`: the normal interactive mode. Drawing operations merge their touched
  logical rectangle into the hint.
- `Full`: used by callers before whole-screen transitions or boot screens.

`Weact213BwDisplay::update_canvas(canvas)` reads the hint, compares only the
hinted canvas bytes when possible, patches the cached native 128x250 controller
buffer, and sends a byte-aligned partial e-paper window. If the hint requests
`Full`, or if the partial update counter reaches
`ESP32CALC_EPD_FULL_REFRESH_INTERVAL`, the driver performs a full e-paper
refresh and resets the counter. This keeps the UI simple while still giving the
driver enough information to avoid unnecessary full-screen transfers.

## Removed RGB565 Renderer

Raylib and the previous e-paper bridge were removed from the firmware. On the
ESP32-S3 software-rendered RGB565 path, even a simple graph frame measured about
0.9 s before the e-paper transfer. That made the bridge slower than the display
refresh itself, so interactive calculator screens now draw directly into the
1-bit `MonoCanvas`.

## Calculation / Programs

`CalcEngine` is a placeholder task for symbolic and numeric work. The intended
next step is to add a request queue with expression parse/evaluate jobs and
later a CAS layer. `ProgramLoader` already assumes SD programs live in
`/sdcard/programs`.

Recommended custom program model:

- Start with a restricted bytecode or tiny interpreted language.
- Give programs explicit APIs for screen, keys, files, and math.
- Run user code in the calculation task with watchdog-friendly yields.
- Keep Wi-Fi/BLE APIs behind capabilities so programs cannot silently enable
  radios.
