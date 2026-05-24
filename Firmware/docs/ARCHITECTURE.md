# Firmware Architecture

## Language Choice

The base is ESP-IDF C++ rather than MicroPython or Rust. Rust would let us use
`weact-studio-epd` directly, but the rest of the requested stack is easier to
stage in ESP-IDF today: dual-core FreeRTOS pinning, SDSPI, ADC, Wi-Fi/BLE, and
the ESP Component Registry Raylib component.

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

## Raylib Bridge

`georgik/raylib` 5.6.0 is designed for ESP-IDF LCD panels with RGB565 buffers.
For e-paper, the firmware keeps the known WeAct command sequence and presents a
small fake `esp_lcd` panel to the raylib port:

- `Weact213BwDisplay` remains the only hardware display driver.
- Raylib draws 250x122 RGB565 chunks through `esp_raylib_port`.
- `RaylibEpaperPort` converts those chunks to `MonoCanvas`.
- The completed frame is sent as a full e-paper refresh while panel mapping is
  being validated.

This gives the UI raylib drawing APIs without replacing the display driver with
a generic LCD path. Partial refresh should stay disabled until full-frame output
is visibly correct on hardware.

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
