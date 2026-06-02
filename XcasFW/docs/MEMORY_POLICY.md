# XcasFW Memory Policy

Target: ESP32-S3 with 16 MB flash and 16 MB PSRAM.

Goal not "avoid dynamic memory". Goal: dynamic memory explicit, bounded where needed, easy release when app closes.

## Internal RAM

Prefer internal RAM for:

- FreeRTOS kernel objects and hot queues.
- Task stacks.
- Display transfer buffers and DMA-sensitive buffers.
- Small event/result envelopes between services.
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

Prefer app-owned arenas/pools reset on app close.

## Runtime Rules

- Only one heavy app active at a time.
- Heavy apps need `load()` and `unload()` or equivalent lifecycle hooks.
- Math worker owns long CAS/scripting ops.
- UI receives bounded result objects, not raw CAS trees.
- Large textual output paged or streamed.
- Failed allocation shows user-visible memory error and leaves system responsive.

## Build Flags To Consider Later

```ini
-DALT_USE_GIAC=1
-DALT_USE_LUA=1
-DALT_USE_MICROPYTHON=0
-DALT_CAS_STACK_BYTES=65536
-DALT_SCRIPT_HEAP_BYTES=1048576
```

## Heap Checks

Log around heavy ops:

- free internal heap
- largest internal free block
- free PSRAM
- largest PSRAM free block
- active app name
- active worker job kind
