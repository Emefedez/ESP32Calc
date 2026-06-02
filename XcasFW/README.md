# XcasFW

Experimental firmware workspace for the ESP32Calc engine/CAS pivot.

This folder is intentionally separate from `Firmware/`. The current firmware is
not replaced, deleted, or reorganized here. `XcasFW/` is the alternative
firmware workspace for the Xcas/Giac-based path, using its own contracts,
naming, memory policy, UI, and hardware assumptions.

## Status

Current lab slice:

- ESP-IDF + PlatformIO target for ESP32-S3 N16R16.
- Wokwi target copied from the current firmware, including the e-paper custom
  chip and keypad matrix wiring.
- Static FreeRTOS math service with bounded queues.
- Minimal test UI for Standard and Graph flows.
- Hardware/display/keypad canvas path adapted from the current firmware.
- Real Giac/Xcas bridge linked as an ESP-IDF component, using explicit
  math-domain methods instead of a generic backend factory.
- NeoCalculator's Giac/KhiCAS and libtommath sources are vendored under
  `components/giac` and `components/libtommath`.
- Memory and pivot notes in `docs/`.

## Initial Direction

- Keep ESP-IDF as the base runtime for predictable memory, tasks, and hardware
  control.
- Use NeoCalculator as an architectural reference, especially for app lifecycle,
  Giac/Xcas isolation, math AST rendering, staged solving, and PSRAM policy.
- Avoid copying GPLv3 code until we explicitly decide what the license and
  attribution boundaries should be. The main repository is AGPLv3, which can be
  compatible with GPLv3 components, but the obligations should be deliberate.
- Prefer reimplementing small pieces behind our own tests and interfaces before
  replacing the current firmware paths.
- Treat Giac/Xcas as the intended source of truth for symbolic math, solving,
  matrices, and calculus. Local math code should only shape requests, expose
  small tests, or provide tightly-scoped helpers; it should not become a second
  CAS.
- Split the old monolithic `calc_engine` into smaller layers:
  - `math/input`: tokenize, parse, expression classification.
  - `math/giac`: canonical Xcas/Giac bridge and only evaluator.
  - `math/solve`: command shaping and optional targeted regression helpers.
  - `math/graph`: sampling and expression evaluation independent of UI.
  - `math/render`: display-neutral math layout.
  - `ui/apps`: app shells that load one active workspace at a time.
  - `scripting`: Lua first, Python/MicroPython only behind a feature flag.

## Memory Assumption

Target hardware has 16 MB flash and 16 MB PSRAM. This workspace assumes:

- Display-critical buffers and FreeRTOS queues stay bounded and preferably
  internal.
- Giac/KhiCAS state, CAS objects, AST nodes, graph buffers, history, and
  scripting heaps can use PSRAM.
- Only one heavy app should be live at once.
- Long math operations should run in a worker task and report progress or a
  bounded result object back to the UI.

## Build

Current testing target is Wokwi only unless physical hardware is available.
Scripts and quick checks should build only:

```sh
./build_wokwi.sh
```

From this folder:

```sh
pio run -e esp32-s3-n16r16
```

From the repository root:

```sh
pio run -d XcasFW -e esp32-s3-n16r16
```

Wokwi build from the repository root:

```sh
pio run -d XcasFW -e esp32-s3-wokwi
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

## Current Priority Notes

- The active firmware folder is `XcasFW/`.
- Graphing should move toward Giac-backed normalization/sampling instead of
  local expression parsing. Non-linear graph behavior is a current priority.
- Constants should keep the same staged UX as Integrals: group selector, search
  within group, numeric filtering, and `=` to copy selected value.
- Natural display/editing needs a real expression layout model for fractions,
  powers, roots, and nested cursor movement. Editing an exponent/root/fraction
  child should replace that child directly, not force deletion of the whole
  parent expression.
- Menus need denser layouts: Standard has unused space, and selector rows must
  avoid overlapping arrows/hints.
- Matrix support should go through Giac bridge methods first, with UI/editor
  code only shaping bounded requests.
