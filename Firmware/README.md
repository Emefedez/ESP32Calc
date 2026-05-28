# ESP32Calc Firmware

This firmware is designed to run on my custom ESP32-S3 based calculator, this is the model with 16MB of PSRAM and 16MB of flash memory, it is designed to be a replacement/solid alternative to a scientific calculator, hopefully with good enough graphing, integration and maybe matrix solving.

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
- Within `/tools`, a MacOS utility for pixel color detection exists, it was used to debug the screen, as it took quite a bit of work to start displaying proper text/graphics instead of random lines.

## Wokwi

```bash
cd Firmware
pio run -e esp32-s3-wokwi
wokwi-cli .
```

- `wokwi.toml` points at the Wokwi PlatformIO firmware output so Wokwi can be
launched after `pio run -e esp32-s3-wokwi`.

- **On real hardware, the FTDI connector should be used to flash the software.**

- The firmware itself targets the real WeAct 2.13' B/W module with partial refresh, this screen has a different resolution from the 2.9' display that is a default in wokwi and, as such, the display uses a version of [this driver](https://github.com/avsaase/weact-studio-epd) that has been adapted for C++.

## Notes

- The battery sense input is modeled with a slide potentiometer on GPIO3. In the Wokwi build, the full slider travel maps to the configured empty-to-full battery range and updates once per second.

- The UI is rendered through `MonoCanvas` with methods like `DrawLine`, `DrawLineStrip`, `DrawRectangleLines`,  and `DrawText`.

- Drawing calls update a `CanvasUpdateHint` with either `Partial` or `Full` refresh intent plus a logical dirty rectangle; we keep a cache, to force `Full` refreshes after a certain amount of partial `render()` calls, this helps us keep solid performance while we mantain a clean look for the display.

- I intended to use `Raylib` for this project, but rendering was extremely slow, even with the new `rlsb` renderer, most likely my fault, but the input lag was simply too much, about a second of delay was the usual amount. Franquly, this may mostly be the fault of trying to do the boilerplate with AI, but MonoCanvas did not give me this problem so I just decided to pivot.

- `config::kBatteryDividerRatio` in `src/app_config.h` exists since real battery voltages are a much smaller range than what the slider in the simulator provides.
