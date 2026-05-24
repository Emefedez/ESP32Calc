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

  void to_epd_native(std::array<uint8_t, kNativeBufferSize>& out) const;

  // Blit an RGB565 pixel buffer into this 1bpp canvas at position (dx, dy).
  // Processes 8 pixels per iteration for speed. src_w/h are the source
  // dimensions; w/h are the region to copy (defaults to full source).
  void blit_rgb565(const uint16_t* pixels, int src_w, int src_h,
                   int dx, int dy, int w = -1, int h = -1);

 private:
  std::array<uint8_t, kBufferSize> buffer_ {};
};

}  // namespace esp32calc
