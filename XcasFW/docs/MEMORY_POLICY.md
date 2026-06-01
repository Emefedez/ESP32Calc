# XcasFW Memory Policy

Target: ESP32-S3 with 16 MB flash and 16 MB PSRAM.

The goal is not to avoid dynamic memory completely. The goal is to make dynamic
memory explicit, bounded where needed, and easy to release when an app closes.

## Internal RAM

Prefer internal RAM for:

- FreeRTOS kernel objects and hot queues.
- Task stacks.
- Display transfer buffers and any DMA-sensitive buffers.
- Small event/result envelopes sent between services.
- ISR-facing data.

Avoid large `std::vector` or unbounded strings in internal RAM.

## PSRAM

Use PSRAM for:

- CAS objects and Giac/KhiCAS heap.
- Math AST nodes and render layout caches.
- Step-by-step logs.
- Graph sample buffers and tables.
- History.
- Lua or MicroPython heaps.
- File import/export staging buffers.

Prefer app-owned arenas or pools that can be reset on app close.

## Runtime Rules

- Only one heavy app is active at a time.
- Heavy apps must have `load()` and `unload()` or equivalent lifecycle hooks.
- The math worker owns long CAS/scripting operations.
- The UI receives bounded result objects, not raw CAS trees.
- Large textual output must be paged or streamed.
- A failed allocation should produce a user-visible memory error and leave the
  system responsive.

## Build Flags To Consider Later

```ini
-DALT_USE_GIAC=1
-DALT_USE_LUA=1
-DALT_USE_MICROPYTHON=0
-DALT_CAS_STACK_BYTES=65536
-DALT_SCRIPT_HEAP_BYTES=1048576
```

## Heap Checks

Add logging around heavy operations:

- free internal heap
- largest internal free block
- free PSRAM
- largest PSRAM free block
- active app name
- active worker job kind
