#include "graphics/mono_canvas.h"

#include "app_log.h"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace esp32calc {
namespace {

void glyph_for(char in, uint8_t (&out)[5]) {
  char c = static_cast<char>(std::toupper(static_cast<unsigned char>(in)));
  const uint8_t* glyph = nullptr;

  static constexpr uint8_t space[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
  static constexpr uint8_t unknown[5] = {0x7F, 0x41, 0x5D, 0x41, 0x7F};
  static constexpr uint8_t digits[10][5] = {
      {0x3E, 0x51, 0x49, 0x45, 0x3E},
      {0x00, 0x42, 0x7F, 0x40, 0x00},
      {0x42, 0x61, 0x51, 0x49, 0x46},
      {0x21, 0x41, 0x45, 0x4B, 0x31},
      {0x18, 0x14, 0x12, 0x7F, 0x10},
      {0x27, 0x45, 0x45, 0x45, 0x39},
      {0x3C, 0x4A, 0x49, 0x49, 0x30},
      {0x01, 0x71, 0x09, 0x05, 0x03},
      {0x36, 0x49, 0x49, 0x49, 0x36},
      {0x06, 0x49, 0x49, 0x29, 0x1E},
  };
  static constexpr uint8_t letters[26][5] = {
      {0x7E, 0x11, 0x11, 0x11, 0x7E},
      {0x7F, 0x49, 0x49, 0x49, 0x36},
      {0x3E, 0x41, 0x41, 0x41, 0x22},
      {0x7F, 0x41, 0x41, 0x22, 0x1C},
      {0x7F, 0x49, 0x49, 0x49, 0x41},
      {0x7F, 0x09, 0x09, 0x09, 0x01},
      {0x3E, 0x41, 0x49, 0x49, 0x7A},
      {0x7F, 0x08, 0x08, 0x08, 0x7F},
      {0x00, 0x41, 0x7F, 0x41, 0x00},
      {0x20, 0x40, 0x41, 0x3F, 0x01},
      {0x7F, 0x08, 0x14, 0x22, 0x41},
      {0x7F, 0x40, 0x40, 0x40, 0x40},
      {0x7F, 0x02, 0x0C, 0x02, 0x7F},
      {0x7F, 0x04, 0x08, 0x10, 0x7F},
      {0x3E, 0x41, 0x41, 0x41, 0x3E},
      {0x7F, 0x09, 0x09, 0x09, 0x06},
      {0x3E, 0x41, 0x51, 0x21, 0x5E},
      {0x7F, 0x09, 0x19, 0x29, 0x46},
      {0x46, 0x49, 0x49, 0x49, 0x31},
      {0x01, 0x01, 0x7F, 0x01, 0x01},
      {0x3F, 0x40, 0x40, 0x40, 0x3F},
      {0x1F, 0x20, 0x40, 0x20, 0x1F},
      {0x7F, 0x20, 0x18, 0x20, 0x7F},
      {0x63, 0x14, 0x08, 0x14, 0x63},
      {0x03, 0x04, 0x78, 0x04, 0x03},
      {0x61, 0x51, 0x49, 0x45, 0x43},
  };

  static constexpr uint8_t colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
  static constexpr uint8_t dot[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
  static constexpr uint8_t dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
  static constexpr uint8_t plus[5] = {0x08, 0x08, 0x3E, 0x08, 0x08};
  static constexpr uint8_t slash[5] = {0x20, 0x10, 0x08, 0x04, 0x02};
  static constexpr uint8_t equals[5] = {0x14, 0x14, 0x14, 0x14, 0x14};
  static constexpr uint8_t percent[5] = {0x23, 0x13, 0x08, 0x64, 0x62};
  static constexpr uint8_t caret[5] = {0x04, 0x02, 0x01, 0x02, 0x04};
  static constexpr uint8_t left_paren[5] = {0x00, 0x1C, 0x22, 0x41, 0x00};
  static constexpr uint8_t right_paren[5] = {0x00, 0x41, 0x22, 0x1C, 0x00};
  static constexpr uint8_t less_than[5] = {0x08, 0x14, 0x22, 0x41, 0x00};
  static constexpr uint8_t greater_than[5] = {0x00, 0x41, 0x22, 0x14, 0x08};
  static constexpr uint8_t apostrophe[5] = {0x00, 0x05, 0x03, 0x00, 0x00};

  if (c == ' ') {
    glyph = space;
  } else if (c >= '0' && c <= '9') {
    glyph = digits[c - '0'];
  } else if (c >= 'A' && c <= 'Z') {
    glyph = letters[c - 'A'];
  } else {
    switch (c) {
      case ':': glyph = colon; break;
      case '.': glyph = dot; break;
      case '-': glyph = dash; break;
      case '+': glyph = plus; break;
      case '/': glyph = slash; break;
      case '=': glyph = equals; break;
      case '%': glyph = percent; break;
      case '^': glyph = caret; break;
      case '(': glyph = left_paren; break;
      case ')': glyph = right_paren; break;
      case '<': glyph = less_than; break;
      case '>': glyph = greater_than; break;
      case '\'': glyph = apostrophe; break;
      default: glyph = unknown; break;
    }
  }

  std::memcpy(out, glyph, sizeof(out));
}

}  // namespace

void MonoCanvas::clear(bool white) {
  std::fill(buffer_.begin(), buffer_.end(), white ? 0xFF : 0x00);
}

void MonoCanvas::set_pixel(int x, int y, bool black) {
  if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) {
    return;
  }
  const size_t index = static_cast<size_t>(y) * ((kWidth + 7) / 8) + x / 8;
  const uint8_t mask = 0x80 >> (x % 8);
  if (black) {
    buffer_[index] &= static_cast<uint8_t>(~mask);
  } else {
    buffer_[index] |= mask;
  }
}

bool MonoCanvas::pixel(int x, int y) const {
  if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) {
    return false;
  }
  const size_t index = static_cast<size_t>(y) * ((kWidth + 7) / 8) + x / 8;
  const uint8_t mask = 0x80 >> (x % 8);
  return (buffer_[index] & mask) == 0;
}

void MonoCanvas::hline(int x, int y, int width, bool black) {
  for (int i = 0; i < width; ++i) {
    set_pixel(x + i, y, black);
  }
}

void MonoCanvas::vline(int x, int y, int height, bool black) {
  for (int i = 0; i < height; ++i) {
    set_pixel(x, y + i, black);
  }
}

void MonoCanvas::rect(int x, int y, int width, int height, bool black) {
  hline(x, y, width, black);
  hline(x, y + height - 1, width, black);
  vline(x, y, height, black);
  vline(x + width - 1, y, height, black);
}

void MonoCanvas::fill_rect(int x, int y, int width, int height, bool black) {
  for (int yy = 0; yy < height; ++yy) {
    hline(x, y + yy, width, black);
  }
}

void MonoCanvas::draw_char(int x, int y, char c, uint8_t scale, bool black) {
  uint8_t glyph[5] {};
  glyph_for(c, glyph);
  const uint8_t s = std::max<uint8_t>(scale, 1);

  for (int col = 0; col < 5; ++col) {
    for (int row = 0; row < 7; ++row) {
      if ((glyph[col] >> row) & 0x01) {
        fill_rect(x + col * s, y + row * s, s, s, black);
      }
    }
  }
}

void MonoCanvas::draw_text(int x, int y, const char* text, uint8_t scale, bool black) {
  if (text == nullptr) {
    return;
  }
  const uint8_t s = std::max<uint8_t>(scale, 1);
  int cursor = x;
  while (*text != '\0') {
    draw_char(cursor, y, *text, s, black);
    cursor += 6 * s;
    ++text;
  }
}

void MonoCanvas::to_epd_native(std::array<uint8_t, kNativeBufferSize>& out) const {
  std::fill(out.begin(), out.end(), 0xFF);

  uint32_t black_pixels = 0;
  uint32_t skipped_white = 0;
  int min_reported_y = kHeight;
  int max_reported_y = 0;

  for (uint16_t y = 0; y < kHeight; ++y) {
    for (uint16_t x = 0; x < kWidth; ++x) {
      if (!pixel(x, y)) {
        ++skipped_white;
        continue;
      }

      ++black_pixels;
      if (y < (uint16_t)min_reported_y) min_reported_y = y;
      if (y > (uint16_t)max_reported_y) max_reported_y = y;

      const uint16_t native_x =
          static_cast<uint16_t>(config::kDisplayNativeWidth - 1 - y);
      const uint16_t native_y = x;
      const size_t index = static_cast<size_t>(native_y) * (config::kDisplayNativeWidth / 8) +
                           native_x / 8;
      const uint8_t mask = 0x80 >> (native_x % 8);
      out[index] &= static_cast<uint8_t>(~mask);
    }
  }

  // Log sample pixel mappings for diagnostics
  constexpr int kSamplePixels = 5;
  int sampled = 0;
  for (uint16_t y = 0; y < kHeight && sampled < kSamplePixels; ++y) {
    for (uint16_t x = 0; x < kWidth && sampled < kSamplePixels; ++x) {
      if (!pixel(x, y)) continue;
      ++sampled;
      const uint16_t nx = static_cast<uint16_t>(config::kDisplayNativeWidth - 1 - y);
      const uint16_t ny = x;
      const size_t idx = static_cast<size_t>(ny) * (config::kDisplayNativeWidth / 8) + nx / 8;
      const uint8_t m = 0x80 >> (nx % 8);
      ESP32CALC_LOGD("canvas", "px[%d] logical(%3d,%3d) -> native(%3d,%3d) byte[%4zu] mask=0x%02x",
                     sampled, x, y, nx, ny, idx, m);
    }
  }

  ESP32CALC_LOGI("canvas",
                 "to_epd_native: black=%u white=%u total=%u rows=[%d..%d]",
                 black_pixels, skipped_white,
                 black_pixels + skipped_white,
                 min_reported_y, max_reported_y);

  // Report first and last non-zero bytes in native buffer
  size_t first_non_ff = kNativeBufferSize;
  size_t last_non_ff = 0;
  for (size_t i = 0; i < kNativeBufferSize; ++i) {
    if (out[i] != 0xFF) {
      if (i < first_non_ff) first_non_ff = i;
      if (i > last_non_ff) last_non_ff = i;
    }
  }
  if (first_non_ff < kNativeBufferSize) {
    ESP32CALC_LOGI("canvas",
                   "native buffer non-0xFF range: bytes [%zu..%zu] (of %zu)",
                   first_non_ff, last_non_ff, kNativeBufferSize);
  } else {
    ESP32CALC_LOGI("canvas", "native buffer: ALL 0xFF (all white)");
  }
}

}  // namespace esp32calc
