#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "app_config.h"

namespace esp32calc {

class MonoCanvas {
 public:
  static constexpr uint16_t kWidth = config::kDisplayLogicalWidth;
  static constexpr uint16_t kHeight = config::kDisplayLogicalHeight;
  static constexpr size_t kBufferSize = ((kWidth + 7) / 8) * kHeight;
  static constexpr size_t kNativeBufferSize = config::kDisplayNativeBufferSize;

  void clear(bool white = true);
  void set_pixel(int x, int y, bool black);
  bool pixel(int x, int y) const;
  void hline(int x, int y, int width, bool black = true);
  void vline(int x, int y, int height, bool black = true);
  void rect(int x, int y, int width, int height, bool black = true);
  void fill_rect(int x, int y, int width, int height, bool black = true);
  void draw_char(int x, int y, char c, uint8_t scale = 1, bool black = true);
  void draw_text(int x, int y, const char* text, uint8_t scale = 1, bool black = true);

  const uint8_t* data() const { return buffer_.data(); }
  uint8_t* data() { return buffer_.data(); }
  size_t size() const { return buffer_.size(); }

  // Converts the 250x128 Rust-style landscape canvas into the SSD1680 native
  // 128x250 memory layout used by the WeAct 2.13 inch panel. The panel only
  // exposes 122 of those 128 rows physically.
  void to_epd_native(std::array<uint8_t, kNativeBufferSize>& out) const;

  // Converts only the region [x..x+w-1, y..y+h-1] from logical canvas to
  // native packed format. Only touches native bytes affected by that region.
  void to_epd_native_region(std::array<uint8_t, kNativeBufferSize>& out,
                            int16_t x, int16_t y, int16_t w, int16_t h) const;

 private:
  std::array<uint8_t, kBufferSize> buffer_ {};
};

}  // namespace esp32calc
