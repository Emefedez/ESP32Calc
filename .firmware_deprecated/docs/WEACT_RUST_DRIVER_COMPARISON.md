# WeAct Rust Driver Comparison

Reference driver: <https://github.com/avsaase/weact-studio-epd>

Compared against the current `master` branch on 2026-05-24.

## What Matches

- 2.13 inch B/W type: Rust uses `DisplayDriver<..., 128, 122, 250, Color>`.
- Init sequence: hardware reset, `0x12` software reset, 10 ms delay, wait for
  BUSY low, `0x01`, `0x11`, `0x3C`, `0x21`, `0x18`, full RAM window, wait.
- BUSY polarity: Rust waits while BUSY is high, so idle is low.
- Full update sequence: write old/red buffer, write B/W buffer, full refresh,
  then sync old/red and B/W buffers again.
- Full refresh command: `0x22` with `0xF7`, then `0x20`.
- Fast refresh command: partial LUT, `0x22` with `0xCC`, then `0x20`.
- Color packing: black clears a bit, white sets a bit.

## Differences Fixed

- Rust's framebuffer for the 2.13 inch display is 128x250 even though only 122
  controller columns are visible. The firmware now uses a 250x128 landscape
  framebuffer and accepts that 250x122 pixels are physically visible.
- Rust examples use conservative SPI speeds during bring-up. The firmware uses
  4 MHz now that the panel mapping and partial refresh path are working.
- The Rust driver assumes panel VCC is already stable before reset. Because this
  hardware list routes display VCC through GPIO17, the firmware now waits 250 ms
  after enabling GPIO17 before reset and sets that GPIO to maximum drive.

## Still Worth Checking On Hardware

- If the panel VCC is connected directly to GPIO17 instead of through a MOSFET
  or load switch, the GPIO may not be able to supply e-paper refresh current
  reliably. That can look like random flashing or a small partial line even when
  the command sequence is correct.
- The firmware can render a raw native-buffer diagnostic before the normal
  canvas path. If that still appears only as a bottom-left line, the remaining
  suspicion shifts below graphics conversion: display power/current, wiring,
  SPI transaction timing, or a pin mismatch.
