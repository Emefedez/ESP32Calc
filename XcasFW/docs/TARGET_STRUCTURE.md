# Target Structure

This workspace should be a guided reimplementation, not a mirror of
NeoCalculator. We use NeoCalculator to ask better questions, then keep or change
each idea deliberately.

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
| `Firmware/src/calc/symbolic_engine.cpp` | `math/giac`, `math/ast`, targeted regression tests | Keep behavior as test cases, but do not use this as the new CAS design. |
| `Firmware/src/ui/modes/graph_mode.cpp` local evaluator | `math/graph` plus shared `math/input` | Graph and standard mode should not parse expressions differently. |
| `Firmware/src/ui/modes/standard_mode.cpp` result drawing | `math/render` plus `ui/apps/calculation_app` | Fractions/powers should be renderer features, not mode special cases. |
| `Firmware/src/app_events.h` calc result structs | `system/event_bus` and typed result payloads | Keep queue payloads bounded and ownership obvious. |

## Reimplementation Rules

- Add a small contract first: structs, ownership, error model, and memory budget.
- Keep imported external source behind narrow boundaries.
- Keep the current firmware behavior as the regression baseline unless we
  explicitly choose a better behavior.
- CAS should be centralized behind the Giac/Xcas bridge. The bridge exposes
  domain methods (`evaluate`, `simplify`, `solve`, `matrix`, `determinant`,
  `inverse`, `graph_expression`) instead of a generic backend abstraction.
- Lightweight local math should only shape requests. Giac/KhiCAS is the
  evaluator for calculation, exact arithmetic, solving, matrices, graph
  normalization, and symbolic operations.
- Graphing must share parser/classifier code with calculator mode.
- Lua and Python runtimes must talk to the same math service instead of reaching
  into UI internals.

## First Milestone

Create a minimal, testable engine path:

```text
MathRequest
  -> tokenizer
  -> parser/classifier
  -> Giac/Xcas bridge
  -> MathResult
```

Then add AST/rendering, graph sampling, and matrix editing around the same
request/result contract.

## Current Implementation Slice

The first slice is now wired into `src/math/math_service.*`:

- `math/input/expression_classifier.*`
- `math/math_engine.*`
- `math/giac/giac_bridge.*`

This path now treats Giac/KhiCAS as the real evaluator. The old native numeric
and rational evaluator files have been removed so XcasFW does not grow a second
math engine. Graph sampling, math AST rendering, a proper matrix editor, and
scripting should be added as separate slices around this bridge.

## Giac/Xcas Bridge Slice

The alternative firmware now has `src/math/giac/giac_bridge.*` as the intended
CAS boundary. It is deliberately an object, not a UART command helper:

- `begin()` owns context setup.
- `evaluate()` handles direct calculation commands.
- `simplify()` handles symbolic simplification.
- `solve()` shapes `solve(...)` commands, including semicolon-separated systems.
- `matrix()`, `determinant()`, and `inverse()` reserve matrix entry points.
- `graph_expression()` reserves graph normalization and validation.

The current implementation links the vendored Giac/KhiCAS source from
`components/giac` plus `components/libtommath`. `MathService` owns one bridge
instance in its worker task, lazy-initializes a Giac context, and routes
math requests through this object. `ESP32CALC_USE_GIAC=1` and
`ESP32CALC_GIAC_COMPILED=1` are enabled for the alternative firmware targets.

It is supossed to be the main CAS method and not depend on manually implemented, so it will be used for everything like Matrix and equation systems except for integral transformations and derivative transformations as those are not implemented.

## Current UI/Wokwi Slice

The alternative firmware now has enough UI to test the engine on the simulated
keypad/display path:

- `diagram.json`, `wokwi.toml`, and the `weact_213_bw` Wokwi chip are present at
  the workspace root.
- `hardware/keymap.*` and `hardware/keypad_matrix.*` keep the existing 9x6
  keyboard matrix/pinout.
- `graphics/mono_canvas.*` and `display/weact_213_bw.*` keep the e-paper render
  path.
- `ui/menu.*` is intentionally small: Standard expression entry, Graph screen,
  and variable selector only.
- `ui/input_behavior.h` makes the calculator-specific modifier rules explicit:
  `SHIFT+=` inserts `=`, `ALPHA+=` opens Graph, and `SHIFT+xyz` opens the
  variable selector.
- squared numbers should not be shown on display like x^2 but rather with a small 2 up there, and automatic parenthesis should be included so that it accidentally does not take the following numbers into the elevation by mistake.
- Divisions should be shown like the natural casio way by default, and the UI should be able to enter/get out of these sub-operations like the sqrt(), (/), x^n logab, etc...
- If possible, make moving words also skip "tokens", as in, Ans is a single term, not actually 3 letters.
