# NeoCalculator Pivot Notes

Reference inspected: `El-EnderJ/NeoCalculator`, branch `main`, commit
`7a1a415a1405368e780077a11fc06fb6c3f4fe5d` on 2026-05-31.

## What Looks Valuable

NeoCalculator/NumOS has several ideas worth adapting:

- App lifecycle: apps are lazily created and torn down, instead of keeping every
  view active forever.
- Math AST first: formulas are edited and rendered as structure, not just as a
  flat string.
- CAS boundary: Giac/KhiCAS is treated as the canonical symbolic backend through
  a bridge instead of being called directly from UI code.
- PSRAM policy: symbolic objects, AST nodes, graph buffers, and step logs are
  pushed toward PSRAM.
- Staged solving: equation solving can be split into parse, solve, render step,
  and finalize phases so the UI stays alive.
- Visual renderer: natural display, fractions, powers, roots, and cursor layout
  are first-class instead of special cases in each mode.

## What Should Not Be Copied Blindly

- It targets Arduino + LVGL + TFT ILI9341, while this project currently targets
  ESP-IDF + 2.13 inch black/white e-paper.
- It uses a lot of `String`, `std::string`, `std::vector`, dynamic app objects,
  and broad UI state. That is acceptable with 16 MB PSRAM, but still needs
  ownership rules and caps.
- It includes many large or experimental apps. Importing all of them would make
  firmware direction less clear.
- The parser/evaluator layers are not uniformly mature; some comments still
  describe future work. The architecture is more valuable than a direct drop-in.
- Its software license is GPLv3. The current ESP32Calc repository is AGPLv3.
  Combining code is possible only if we preserve compatible copyleft terms and
  attribution. A clean-room interface first is safer.

## Pivot Recommendation

Do not replace `Firmware/` immediately. Build `NeoCalculator_inspiration/` as a
parallel lab with a new internal contract:

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

The first migration target should not be "all of NeoCalculator". It should be:

1. A stable math request/result API.
2. A display-neutral expression tree and renderer contract.
3. A CAS bridge object backed by Giac/KhiCAS.
4. A graph engine that shares the same parser/evaluator as calculation mode.
5. A lazy app runtime that keeps only the active app loaded.

## Lua And Python

With 16 MB flash and PSRAM, scripting is realistic, but it should be isolated:

- Lua is the better first embedded scripting runtime because it is smaller,
  easy to sandbox, and enough for calculator programs.
- Python should mean MicroPython or a compatible tiny runtime, not CPython.
- Scripting should be one app or one service with a fixed heap budget.
- Scripts should communicate through the same math/result API, not poke UI or
  CAS internals directly.

## Proposed Migration Phases

1. Phase 0: create this inspiration folder and boot a bounded service
   skeleton.
2. Phase 1: extract current numeric, rational, linear, polynomial, and graph
   logic behind separate interfaces without changing behavior.
3. Phase 2: introduce a display-neutral math AST and renderer model for the
   e-paper canvas.
4. Phase 3: keep expanding the real Giac bridge methods instead of building a
   parallel symbolic engine.
5. Phase 4: measure Giac integration on hardware, including license, flash
   size, PSRAM use, exceptions, stack size, and long-operation behavior.
6. Phase 5: add Lua scripting. Consider MicroPython only after the app runtime
   and memory guardrails are proven.
