# Target Structure

Workspace = guided reimplementation, not NeoCalculator mirror. Use NeoCalculator to ask better questions; keep/change each idea deliberately.

## Proposed Source Layout

```text
src/
  system/
    app_runtime.*          active app lifecycle, lazy load/unload
    memory_budget.*        internal/PSRAM checks and per-app budgets
    event_bus.*            bounded events between services

  hardware/
    keypad.*
    display_backend.*      e-paper now, different from NeoCalc
    storage.*

  math/
    input/
      tokenizer.*
      parser.*
      classifier.*

    ast/
      expression_node.*
      expression_arena.*
      serializer.*

    solve/
      command_shape.*      request shaping and targeted regression helpers
      solve_steps.*

    giac/
      giac_bridge.*        canonical Xcas/Giac object API

    graph/
      graph_expression.*
      sampler.*
      viewport.*

    render/
      math_layout.*
      mono_math_renderer.* e-paper renderer, independent from engine

  ui/
    app_shell.*
    apps/
      calculation_app.*
      graph_app.*
      matrix_app.*
      settings_app.*

  scripting/
    script_service.*
    lua_runtime.*          first scripting target
    micropython_runtime.*  optional later
```

## Current Firmware Migration Map

| Current Area | New Area | Notes |
| --- | --- | --- |
| `Firmware/src/calc/calc_engine.cpp` | `math/input`, `math/giac`, `math/solve`, `system/math_service` | Split orchestration from parsing and solving. |
| `Firmware/src/calc/symbolic_engine.cpp` | `math/giac`, `math/ast`, targeted regression tests | Keep behavior as test cases; do not use as new CAS design. |
| `Firmware/src/ui/modes/graph_mode.cpp` local evaluator | `math/graph` plus shared `math/input` | Graph and standard mode should parse expressions same way. |
| `Firmware/src/ui/modes/standard_mode.cpp` result drawing | `math/render` plus `ui/apps/calculation_app` | Fractions/powers renderer features, not mode special cases. |
| `Firmware/src/app_events.h` calc result structs | `system/event_bus` and typed result payloads | Keep queue payloads bounded and ownership obvious. |

## Reimplementation Rules

- Add small contract first: structs, ownership, error model, memory budget.
- Keep imported external source behind narrow boundaries.
- Keep current firmware behavior as regression baseline unless explicitly choosing better behavior.
- CAS centralized behind Giac/Xcas bridge. Bridge exposes domain methods (`evaluate`, `simplify`, `solve`, `matrix`, `determinant`, `inverse`, `graph_expression`) instead of generic backend abstraction.
- Lightweight local math only shapes requests. Giac/KhiCAS evaluates calculation, exact arithmetic, solving, matrices, graph normalization, symbolic ops.
- Graphing must share parser/classifier with calculator mode. Current local graph evaluator temporary; replace with Giac-backed normalization/sampling helpers in bridge.
- Lua/Python runtimes talk to same math service; no reaching into UI internals.

## First Milestone

Create minimal, testable engine path:

```text
MathRequest
  -> tokenizer
  -> parser/classifier
  -> Giac/Xcas bridge
  -> MathResult
```

Then add AST/rendering, graph sampling, matrix editing around same request/result contract.

## Current Implementation Slice

First slice wired into `src/math/math_service.*`:

- `math/input/expression_classifier.*`
- `math/math_engine.*`
- `math/giac/giac_bridge.*`

Giac/KhiCAS now real evaluator. Old native numeric/rational evaluator files removed so XcasFW does not grow second math engine. Explicit graph sampling runs through `GiacBridge::sample_graph()`. Old UI-local graph parser only under `src/deprecated/`. Math AST rendering, proper matrix editor, scripting should be separate slices around bridge.

Current UX priorities:

- Use Wokwi target (`esp32-s3-wokwi`) for testing until physical hardware available.
- Keep menus staged and active-only: mode selector with arrows/index, `MENU` returns to selector, heavy submenu data built/destroyed by active mode path.
- Constants and Integrals same interaction model: group selection, in-group search/numeric filtering, then `=` copies selected item.
- Natural display must become structural for powers, roots, fractions so cursor edits target child slot directly.

## Giac/Xcas Bridge Slice

Alt firmware has `src/math/giac/giac_bridge.*` as intended CAS boundary. Object, not UART command helper:

- `begin()` owns context setup.
- `evaluate()` handles direct calculation commands.
- `simplify()` handles symbolic simplification.
- `solve()` shapes `solve(...)` commands, including semicolon-separated systems.
- `matrix()`, `determinant()`, and `inverse()` reserve matrix entry points.
- `graph_expression()` reserves graph normalization and validation.

Implementation links vendored Giac/KhiCAS from `components/giac` plus `components/libtommath`. `MathService` owns one bridge in worker task, lazy-initializes Giac context, routes math requests through object. `ESP32CALC_USE_GIAC=1` and `ESP32CALC_GIAC_COMPILED=1` enabled for alt firmware targets.

Giac/Xcas should be main CAS method, used for Matrix and equation systems. Avoid manual implementations except bounded request shaping and integral/derivative transformations until bridge supports them.

## Current UI/Wokwi Slice

Alt firmware has enough UI to test engine on simulated keypad/display:

- `diagram.json`, `wokwi.toml`, and `weact_213_bw` Wokwi chip at workspace root.
- `hardware/keymap.*` and `hardware/keypad_matrix.*` keep existing 9x6 keyboard matrix/pinout.
- `graphics/mono_canvas.*` and `display/weact_213_bw.*` keep e-paper render path.
- `ui/menu.*` intentionally small: Standard expression entry, Graph screen, variable selector.
- `ui/input_behavior.h` makes calculator modifier rules explicit: `SHIFT+=` inserts `=`, `ALPHA+=` opens Graph, `SHIFT+xyz` opens variable selector.
- Squared numbers should display natural (`x` with small raised `2`), with automatic parentheses so following numbers do not accidentally join exponent.
- Divisions should display natural Casio-style by default; UI must enter/exit sub-operations like `sqrt()`, `(/)`, `x^n`, `logab`, etc.
- Movement should skip tokens when possible: `Ans` one term, not 3 letters.
