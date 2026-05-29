# ESP32Calc Firmware

Firmware for the custom ESP32-S3 calculator: 16 MB flash, 16 MB PSRAM on target
hardware, WeAct 2.13 inch black/white e-paper, SDSPI storage, matrix keyboard,
and a small C++ math engine for numeric, symbolic, graph, system, and matrix
work.

## Build

```bash
cd Firmware
pio run -e esp32-s3-n16r16
```

## Wokwi

```bash
cd Firmware
pio run -e esp32-s3-wokwi
wokwi-cli .
```

`wokwi.toml` points to the PlatformIO Wokwi firmware output. The simulator uses
the same UI and calculation code as hardware, with simulator-specific battery
poll timing.

## Flash / Monitor

```bash
cd Firmware
pio run -e esp32-s3-n16r16 -t upload
pio device monitor -e esp32-s3-n16r16
```

Real hardware should be flashed through the FTDI connector.

## Current Math Surface

- Numeric expressions: `+`, `-`, `*`, `/`, `^`, parentheses, unary signs, and
  scientific notation such as `1E-3`.
- Fractions: exact rational division results display as stacked fractions in
  Standard mode. `S<>D` toggles fraction and decimal views.
- Symbolic polynomial work: simplify polynomial expressions in `x`, derive with
  `d/dx(...)`, integrate with `int(...)`, and solve real polynomial equations up
  to degree 2.
- Linear systems: enter equations separated by `;`, for example
  `x+y=2;x-y=0`. The solver uses RREF and supports `x,y,z,a,b,c`.
- Graphing: explicit `y=...`, reversed `...=y`, linear implicit equations, and
  nonlinear implicit equations with one displayed real branch.
- Matrix mode: enter matrices as rows separated by `;` and columns separated by
  `,`, for example `1,2;3,4`. `DET` and `INVERSE` call the engine directly.

See [docs/MATH_ENGINE.md](docs/MATH_ENGINE.md) for syntax and limits.

## Architecture Docs

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): tasks, queues, display path, UI
  modes, and calculation ownership.
- [docs/WEACT_RUST_DRIVER_COMPARISON.md](docs/WEACT_RUST_DRIVER_COMPARISON.md):
  display driver notes against the Rust reference driver.

## Notes

- UI rendering goes through `MonoCanvas`, a 250x128 logical 1-bit framebuffer.
- Drawing calls update `CanvasUpdateHint`; the display driver uses that dirty
  region for partial refresh and forces a full refresh after the configured
  interval.
- The battery sense input is modeled with a slide potentiometer on GPIO3 in
  Wokwi. Real hardware uses `config::kBatteryDividerRatio` from
  `src/app_config.h`.
- `tools/screen_color_probe.py` is a macOS helper used while debugging Wokwi
  screen colors and panel mapping.
