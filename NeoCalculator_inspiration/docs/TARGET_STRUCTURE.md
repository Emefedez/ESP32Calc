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
    display_backend.*      e-paper now, possible TFT/LVGL later
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

    core/
      numeric_eval.*
      rational.*
      constants.*
      formatting.*

    solve/
      linear_system.*
      polynomial.*
      cas_bridge.*         interface first, Giac implementation later
      solve_steps.*

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
| `Firmware/src/calc/calc_engine.cpp` | `math/input`, `math/core`, `math/solve`, `system/math_service` | Split orchestration from parsing and solving. |
| `Firmware/src/calc/symbolic_engine.cpp` | `math/core`, `math/solve`, `math/ast` | Keep behavior, replace unclear names and isolated helper duplication. |
| `Firmware/src/ui/modes/graph_mode.cpp` local evaluator | `math/graph` plus shared `math/input` | Graph and standard mode should not parse expressions differently. |
| `Firmware/src/ui/modes/standard_mode.cpp` result drawing | `math/render` plus `ui/apps/calculation_app` | Fractions/powers should be renderer features, not mode special cases. |
| `Firmware/src/app_events.h` calc result structs | `system/event_bus` and typed result payloads | Keep queue payloads bounded and ownership obvious. |

## Reimplementation Rules

- Add a small contract first: structs, ownership, error model, and memory budget.
- Reimplement behavior locally before importing external source.
- Keep the current firmware behavior as the regression baseline unless we
  explicitly choose a better behavior.
- CAS should be replaceable behind an interface. A lightweight local fallback
  should handle basic numeric, rational, linear, polynomial, and matrix cases.
- Graphing must share parser/classifier code with calculator mode.
- Lua and Python runtimes must talk to the same math service instead of reaching
  into UI internals.

## First Milestone

Create a minimal, testable engine path:

```text
MathRequest
  -> tokenizer
  -> parser/classifier
  -> numeric_eval or solve fallback
  -> MathResult
```

Then migrate the existing numeric parser and rational formatting into that
shape. Once this is stable, add AST/rendering and CAS bridge work.

## Current Implementation Slice

The first slice is now wired into `src/math/math_service.*`:

- `math/input/expression_classifier.*`
- `math/core/numeric_eval.*`
- `math/core/rational_eval.*`
- `math/solve/linear_solver.*`
- `math/math_engine.*`

This intentionally covers a narrow but real path from the current firmware:
numeric evaluation, exact rational display for division, symbolic linear
simplification, and linear systems. Polynomial CAS, matrices, graph sampling,
math AST rendering, and scripting should be added as separate slices.

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
