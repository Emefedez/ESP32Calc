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

The Wokwi diagram uses `board-epaper-2in9` as a simulator stand-in. The firmware
itself targets the real WeAct 2.13 inch B/W module with the Rust driver's
128x250 controller buffer, exposed as a 250x128 landscape framebuffer with
250x122 physically visible pixels.

## Notes

- The current firmware forces full e-paper refreshes
  (`ESP32CALC_FORCE_FULL_REFRESH=1`) and shows a boot self-test pattern before
  the menu. This is intentional while the panel mapping is being validated.
- Display SPI is set to 100 kHz for bring-up, matching the conservative speed
  used by the Rust examples more closely than the earlier 4 MHz setting.
- Raylib is enabled with `georgik/raylib` 5.6.0. The UI renders through raylib's
  software path into RGB565 chunks, then `graphics/raylib_epaper_port.cpp`
  downconverts the frame to 1-bit and sends it through the WeAct e-paper driver.
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
