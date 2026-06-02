# Firmware Architecture

ESP32Calc is an ESP-IDF C++ firmware split around small FreeRTOS services and a
single UI event queue. The code avoids copying owned data through queues:
expressions and calculation results travel as heap-owned pointers, then the
receiver frees them after copying what it needs.

## Boot And Tasks

- `main.cpp` owns global services: display, keypad, battery monitor, SD storage,
  calculation engine, wireless service, and boot canvas.
- Core 1 runs the UI task. `MenuUi::run()` receives events, mutates current mode
  state, then renders one frame.
- Core 0 runs lower-level services: keypad scanning, battery polling, SD setup,
  wireless placeholder service, and `CalcEngine`.
- `AppEvent` is the only UI ingress path. Event variants currently cover keys,
  battery snapshots, storage, calculation results, and wireless updates.

## Input

`pins.h` defines the keyboard matrix GPIOs. `keypad_matrix.cpp` drives columns
open-drain, reads pulled-up rows, debounces changes, and publishes key events.
`keymap.cpp` maps physical keys to roles and text tokens.

Important roles:

- `SHIFT` and `ALPHA` are one-shot UI modifiers managed by `MenuUi`.
- `MODE` closes the active mode and returns to the main menu.
- `S<>D` is `KeyRole::FractionToggle` in Standard mode.
- In Matrix mode, `ALPHA /` inserts `,` and `ALPHA =` inserts `;`.

## Display

`MonoCanvas` is the drawing layer. It stores a 250x128 logical black/white
framebuffer and exposes primitive drawing plus Raylib-like aliases such as
`DrawLine`, `DrawRectangleLines`, and `DrawText`.

`Weact213BwDisplay` converts that logical landscape buffer into the SSD1680
128x250 native layout, keeping in mind the real screen resolution is actually 122*250.
Full updates write both controller buffers. Partial updates (which are much faster and as such, essential for e-ink) use the dirty rectangle from `CanvasUpdateHint`, patch the cached native buffer, and send a byte-aligned e-paper window. After `ESP32CALC_EPD_FULL_REFRESH_INTERVAL` partial updates, the driver forces a full refresh to control ghosting.

## UI Modes

Modes are registered in `mode_registry.cpp` and constructed in preallocated
storage owned by `MenuUi`. Only one mode instance is live at once.

- `STANDARD`: expression entry, graph launch with `ALPHA ENTER`, result display,
  symbolic calculus, and `S<>D`. [NOTE] `SHIFT ENTER` inserts `=`.
- `MATRIX`: linear systems, matrix determinant, matrix inverse.
- `PROGRAMS`, `FILES`, `WIRELESS`, `SETTINGS`: menu shells for upcoming work.
- `GRAPH`: opened by calculation result action, not shown in the main mode list.

Modes render into the shared `MonoCanvas` and return `ModeResult` to request
redraws, full refreshes, or menu exit.

## Calculation Engine

`CalcEngine` owns a FreeRTOS request queue. Public submission methods duplicate
the input expression into a heap `char*`, enqueue it, and return immediately.
The calculation task classifies the expression, evaluates it, allocates a
`CalcResult`, and sends that pointer through `AppEvent`.

`calc_engine.cpp` is the queue/orchestration layer plus the numeric and linear
paths. `symbolic_engine.cpp` contains the heavier dynamic math paths: exact
rational display, polynomial symbolic work, polynomial equations, and matrices.

Expression paths:

- Numeric parser: recursive descent for `+ - * / ^`, unary signs, parentheses,
  and `strtod` numbers. Exponentiation binds tighter than unary signs, so
  `-x^2` is parsed as `-(x^2)`.
- Rational parser: exact(ish) integer/decimal rational arithmetic for fraction
  display when division is present.
- Linear parser: expressions over `x,y,z,a,b,c` normalized into coefficient rows.
- RREF solver: handles linear systems, inconsistent systems, free variables, and
  known-variable options.
- Polynomial parser: dynamic `std::vector<PolyTerm>` representation for `x`
  expressions, including implicit multiplication and integer exponents.
- Polynomial calculus: `d/dx(...)` and `int(...)`. `int(...)` is the integration
  operator exposed by the calculator UI.
- Polynomial equation solver: real degree 1 and degree 2 equations.
- Matrix helpers: dynamic matrix parse, determinant, and inverse.

The engine still keeps small fixed caps where the UI or solver needs bounded
embedded behavior: UI entry buffers, maximum linear equation count, and maximum
linear variables. Symbolic polynomial and matrix internals use dynamic storage.

## Graphing

Graph mode has a local expression evaluator for `x`, `y`, constants `pi/e`, and
functions `sin`, `cos`, `tan`, `sqrt`, `ln`, and `log`.

Supported graph forms:

- `sin(x)` and other explicit `y=f(x)` bodies.
- `y=...` and `...=y`.
- Linear implicit equations with `y` on both sides.
- Nonlinear implicit equations through numeric root scanning in `y`. If multiple
  real branches exist, the displayed branch prefers non-negative `y`, then the
  root closest to zero.

## Storage / Programs / Wireless

- `SdStorage` should mount SDSPI at `/sdcard`.
- `ProgramLoader` expects programs under /sdcard/programs`.
- Wireless service should eventually be able to help us connect to the device and manage it.

## Build Targets

- `esp32-s3-n16r16`: real hardware target with 16 MB flash and OPI PSRAM config.
- `esp32-s3-wokwi`: Wokwi target with simulator defines and slower battery poll.
