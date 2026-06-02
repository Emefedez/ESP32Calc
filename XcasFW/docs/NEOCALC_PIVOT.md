# NeoCalculator Pivot Notes

Reference inspected: `El-EnderJ/NeoCalculator`, branch `main`, commit `7a1a415a1405368e780077a11fc06fb6c3f4fe5d` on 2026-05-31.

## What Looks Valuable

NeoCalculator/NumOS ideas worth adapting:

- App lifecycle: lazy create/tear down apps; avoid every view active forever.
- Math AST first: edit/render formulas as structure, not flat string.
- CAS boundary: Giac/KhiCAS canonical symbolic backend through bridge; UI does not call CAS directly.
- PSRAM policy: symbolic objects, AST nodes, graph buffers, step logs pushed toward PSRAM.
- Staged solving: parse, solve, render step, finalize; UI stays alive.
- Visual renderer: natural display, fractions, powers, roots, cursor layout first-class.

## What Should Not Be Copied Blindly

- Targets Arduino + LVGL + TFT ILI9341; this project targets ESP-IDF + 2.13 inch black/white e-paper.
- Uses much `String`, `std::string`, `std::vector`, dynamic app objects, broad UI state. OK with 16 MB PSRAM, but needs ownership rules + caps.
- Many large/experimental apps; importing all muddies firmware direction.
- Parser/evaluator layers mixed maturity; comments still describe future work. Architecture more useful than direct drop-in.
- License GPLv3. Current ESP32Calc repo AGPLv3. Combining possible only with compatible copyleft + attribution. Clean-room interface first safer.

## Pivot Recommendation

Do not replace `Firmware/` immediately. Build `XcasFW/` parallel lab with new internal contract:

```text
keys/storage/wireless
        |
        v
app shell and active workspace
        |
        v
math request queue
        |
        v
math worker
  - Giac CAS bridge
  - tightly-scoped request shaping helpers
  - optional scripting runtime
        |
        v
bounded result model
        |
        v
display-neutral math renderer
        |
        v
e-paper or future display backend
```

First migration target not "all NeoCalculator". Target:

1. Stable math request/result API.
2. Display-neutral expression tree + renderer contract.
3. CAS bridge object backed by Giac/KhiCAS.
4. Graph engine sharing parser/evaluator with calculation mode.
5. Lazy app runtime with only active app loaded.

## Lua And Python

16 MB flash + PSRAM makes scripting realistic, but isolated:

- Lua first: smaller, easy sandbox, enough for calculator programs.
- Python means MicroPython or compatible tiny runtime, not CPython.
- Scripting = one app/service with fixed heap budget.
- Scripts communicate through same math/result API, not UI/CAS internals.

## Proposed Migration Phases

1. Phase 0: create inspiration folder + bounded service skeleton.
2. Phase 1: extract current numeric, rational, linear, polynomial, graph logic behind interfaces without behavior change.
3. Phase 2: introduce display-neutral math AST + renderer model for e-paper canvas.
4. Phase 3: expand real Giac bridge methods; avoid parallel symbolic engine.
5. Phase 4: measure Giac on hardware: license, flash size, PSRAM use, exceptions, stack size, long-operation behavior.
6. Phase 5: add Lua scripting. Consider MicroPython only after app runtime + memory guardrails proven.
