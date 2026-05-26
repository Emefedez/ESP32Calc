# ESP32Calc Firmware Blueprint

This firmware is a first working architecture for the ESP32-S3 N16R16 calculator:

- ESP-IDF C++ under PlatformIO for FreeRTOS, SPI, ADC, SDSPI, and later Wi-Fi/BLE.
- WeAct 2.13 inch B/W e-paper driver adapted from `avsaase/weact-studio-epd`.
- 9x6 matrix keyboard from `.ignoredList.md`, with `SHIFT`, `ALPHA`, and always-available `MODE`.
- Boot-to-menu UI with separate FreeRTOS services for UI/input, battery, SD, calculation, and wireless.
- Debug logs with Unix-style second timestamps from `gettimeofday()`.

## Build

```bash
cd Firmware
pio run -e esp32-s3-n16r16
```

## Flash / Monitor

```bash
cd Firmware
pio run -e esp32-s3-n16r16 -t upload
pio device monitor -e esp32-s3-n16r16
```

## Wokwi

```bash
cd Firmware
pio run -e esp32-s3-wokwi
wokwi-cli .
```

`wokwi.toml` points at the Wokwi PlatformIO firmware output so Wokwi can be
launched after `pio run -e esp32-s3-wokwi`.

The battery sense input is modeled with a slide potentiometer on GPIO3. In the
Wokwi build, the full slider travel maps to the configured empty-to-full battery
range and updates once per second.

The Wokwi diagram uses `board-epaper-2in9` as a simulator stand-in. The firmware
itself targets the real WeAct 2.13 inch B/W module with the Rust driver's
128x250 controller buffer, exposed as a 250x128 landscape framebuffer with
250x122 physically visible pixels.

## Notes

- The UI is rendered through `MonoCanvas`, a 1-bit framebuffer with native
  drawing primitives and familiar convenience methods such as `DrawLine`,
  `DrawLineStrip`, `DrawRectangleLines`, and `DrawText`.
- Drawing calls update a `CanvasUpdateHint` with either `Partial` or `Full`
  refresh intent plus a logical dirty rectangle. `Weact213BwDisplay` consumes
  that hint, patches its cached native controller buffer, and sends only the
  changed e-paper window when possible.
- Full e-paper refreshes are still requested for boot and screen transitions.
  During normal partial operation the driver forces one full refresh every
  `ESP32CALC_EPD_FULL_REFRESH_INTERVAL` partial updates to limit ghosting.
- Display SPI is set to 4 MHz for both hardware and Wokwi builds. The e-paper
  controller latency dominates after removing the old RGB565 renderer path.
- Raylib was removed from the firmware. The ESP32-S3 software renderer path was
  measured at roughly 0.9 s per simple graph frame before the e-paper update,
  so interactive graph/UI drawing uses `MonoCanvas` primitives instead.
- `tools/screen_color_probe.py` can capture a small macOS screen region and list
  dominant colors for Wokwi/camera-preview debugging.
- `GPIO0`, `GPIO45`, and `GPIO8` are involved in boot/serial edge cases on this
  board. Avoid holding matrix keys during reset/flashing until the hardware is
  validated.
- FTDI flashing is left to the ESP32-S3 ROM bootloader path on UART0. The
  firmware records the shared RTS/VOUT GPIOs in `pins.h` but does not drive them.
- `GPIO17` is treated as the e-paper power-enable/VCC pin because that is how it
  is listed in `.ignoredList.md`.
- `config::kBatteryDividerRatio` in `src/app_config.h` must match the real VBAT
  divider before battery percentage can be trusted.
