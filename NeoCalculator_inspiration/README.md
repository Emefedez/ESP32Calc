# NeoCalculator Inspiration

Experimental firmware workspace for the ESP32Calc engine/CAS pivot.

This folder is intentionally separate from `Firmware/`. The current firmware is
not replaced, deleted, or reorganized here. `NeoCalculator_inspiration/` is a
place to study the NeoCalculator/NumOS shape, then reimplement the parts we want
with our own contracts, naming, memory policy, UI, and hardware assumptions.

## Status

Current lab slice:

- ESP-IDF + PlatformIO target for ESP32-S3 N16R16.
- Wokwi target copied from the current firmware, including the e-paper custom
  chip and keypad matrix wiring.
- Static FreeRTOS math service with bounded queues.
- Minimal test UI for Standard and Graph flows.
- Hardware/display/keypad canvas path adapted from the current firmware.
- Memory and pivot notes in `docs/`.
- No NeoCalculator source is vendored yet.

## Initial Direction

- Keep ESP-IDF as the base runtime for predictable memory, tasks, and hardware
  control.
- Use NeoCalculator as an architectural reference, especially for app lifecycle,
  CAS isolation, math AST rendering, staged solving, and PSRAM policy.
- Avoid copying GPLv3 code until we explicitly decide what the license and
  attribution boundaries should be. The main repository is AGPLv3, which can be
  compatible with GPLv3 components, but the obligations should be deliberate.
- Prefer reimplementing small pieces behind our own tests and interfaces before
  replacing the current firmware paths.
- Split the old monolithic `calc_engine` into smaller layers:
  - `math/input`: tokenize, parse, expression classification.
  - `math/core`: numeric and exact arithmetic.
  - `math/solve`: linear, polynomial, and CAS-backed solving.
  - `math/graph`: sampling and expression evaluation independent of UI.
  - `math/render`: display-neutral math layout.
  - `ui/apps`: app shells that load one active workspace at a time.
  - `scripting`: Lua first, Python/MicroPython only behind a feature flag.

## Memory Assumption

Target hardware has 16 MB flash and 16 MB PSRAM. This workspace assumes:

- Display-critical buffers and FreeRTOS queues stay bounded and preferably
  internal.
- CAS, AST nodes, graph buffers, history, and scripting heaps can use PSRAM.
- Only one heavy app should be live at once.
- Long math operations should run in a worker task and report progress or a
  bounded result object back to the UI.

## Build

From this folder:

```sh
pio run -e esp32-s3-n16r16
```

From the repository root:

```sh
pio run -d NeoCalculator_inspiration -e esp32-s3-n16r16
```

Wokwi build from the repository root:

```sh
pio run -d NeoCalculator_inspiration -e esp32-s3-wokwi
```

The Wokwi files live directly in this folder:

- `diagram.json`
- `wokwi.toml`
- `weact_213_bw.chip.json`
- `weact_213_bw.chip.wasm`

## Test UI Behaviors

The current alternative UI intentionally implements only the behavior needed to
exercise the migrated math path:

- `SHIFT` then `=` inserts `=` into the expression instead of evaluating.
- `ALPHA` then `=` opens the Graph screen with the current expression.
- `SHIFT` then `xyz` opens the variable selector for `x y z a b c`.
- `SHIFT` then `xyz^2` opens the same selector and inserts the chosen variable
  squared.
- `ENTER`/`CALC` evaluates through `MathService`.
