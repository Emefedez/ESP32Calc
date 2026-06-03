# XcasFW

Experimental firmware workspace for ESP32Calc engine/CAS pivot.

Separate from `Firmware/`. Current firmware not replaced/deleted/reorganized here. 
`XcasFW/` = alt Xcas/Giac firmware workspace with own contracts, naming, memory policy, UI, hardware assumptions.

## Status

Current lab slice:

- ESP-IDF + PlatformIO target for ESP32-S3 N16R16.
- Wokwi target copied from current firmware: e-paper custom chip + keypad matrix wiring.
- Static FreeRTOS math service, bounded queues.
- Minimal Standard + Solver + Graph-view UI test flows.
- SD SPI storage foundation: `/sdcard/config`, `/sdcard/programs`, internal FAT mirror at `/internal/programs`.
- Hardware/display/keypad canvas path adapted from current firmware.
- Real Giac/Xcas bridge linked as ESP-IDF component; explicit math-domain methods, no generic backend factory.
- NeoCalculator Giac/KhiCAS + libtommath sources vendored under `components/giac` and `components/libtommath`.
- Memory + pivot notes in `docs/`.

## Initial Direction

- Keep ESP-IDF runtime for predictable memory, tasks, hardware control.
- Use NeoCalculator as architecture reference: app lifecycle, Giac/Xcas isolation, math AST rendering, staged solving, PSRAM policy.
- Avoid GPLv3 code copy until license/attribution boundaries explicit. Main repo AGPLv3 can be compatible with GPLv3, but obligations deliberate.
- Prefer small reimplementations behind own tests/interfaces before replacing current firmware paths.
- Giac/Xcas = source of truth for symbolic math, solving, matrices, calculus. Local math only shape requests, expose small tests, or tight helpers; no second CAS.
- SD card programs are external app manifests plus payload files. Internal flash can mirror flat program folders for offline launch later.
- WiFi/chatbot secrets are read from SD config files, not compiled into firmware.
- Split old monolithic `calc_engine` into smaller layers:
  - `math/input`: tokenize, parse, expression classification.
  - `math/giac`: canonical Xcas/Giac bridge and only evaluator.
  - `math/solve`: command shaping and optional targeted regression helpers.
  - `math/graph`: sampling and expression evaluation independent of UI.
  - `math/render`: display-neutral math layout.
  - `ui/apps`: app shells that load one active workspace at a time.
  - `scripting`: Lua first, Python/MicroPython only behind a feature flag.

## Memory Assumption

Target hardware: 16 MB flash + 16 MB PSRAM.

- Display-critical buffers + FreeRTOS queues bounded, preferably internal.
- Giac/KhiCAS state, CAS objects, AST nodes, graph buffers, history, scripting heaps can use PSRAM.
- Only one heavy app live at once.
- Long math ops run in worker task; report progress or bounded result object to UI.

## Build

Current test target Wokwi only unless physical hardware available. Scripts/quick checks build only:

```sh
./build_wokwi.sh
```

From this folder:

```sh
pio run -e esp32-s3-n16r16
```

From repository root:

```sh
pio run -d XcasFW -e esp32-s3-n16r16
```

Wokwi build from repository root:

```sh
pio run -d XcasFW -e esp32-s3-wokwi
```

Wokwi files in this folder:

- `diagram.json`
- `wokwi.toml`
- `weact_213_bw.chip.json`
- `weact_213_bw.chip.wasm`

## Test UI Behaviors

Alt UI only covers behavior needed to exercise migrated math path:

- `SHIFT` then `=` inserts `=` instead of evaluate.
- `ALPHA` then `=` opens Graph screen with current expression.
- `SHIFT` then `xyz` opens variable selector for `x y z a b c`.
- `SHIFT` then `xyz^2` opens same selector and inserts chosen variable squared.
- `MODE` then `SOLVER` opens equation and systems-of-equations templates.
- `ALPHA` then `=` opens Graph view from the Standard expression; Graph is no
  longer a standalone mode selector entry.
- `ENTER`/`CALC` evaluates through `MathService`.

## SD Card Layout

Templates live in `sdcard_template/`.

```text
/sdcard/
  config/
    wifi.ini       ssid/password/hostname
    chatbot.ini    provider/endpoint/model/api_key/system_prompt
    keymap.ini     optional boot key overrides
  programs/
    chatbot/
      app.ini      external app manifest
      keymap.ini   per-app key overrides
      chatbot.app  app payload placeholder
```

Key remaps use `key.<row>.<col>.<field>=value`. Fields: `label`, `role`,
`normal`, `shift`, `alpha`. External apps can remap keys only when manifest has
`allow_keymap=true`.

## Current Priority Notes

- Active firmware folder: `XcasFW/`.
- Constants should match Integrals staged UX: group selector, in-group search, numeric filtering, `=` copies selected value.
