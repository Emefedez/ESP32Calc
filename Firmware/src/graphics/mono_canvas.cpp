#include "graphics/mono_canvas.h"

#include "esp_log.h"
#include "esp_timer.h"
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

void MonoCanvas::blit_rgb565(const uint16_t* pixels, int src_w, int src_h,
                              int dx, int dy, int w, int h) {
  if (!pixels) return;
  if (w < 0) w = src_w;
  if (h < 0) h = src_h;
  constexpr int kBpr = (kWidth + 7) / 8;

  for (int row = 0; row < h; ++row) {
    const int cy = dy + row;
    if (cy < 0 || cy >= kHeight) continue;
    const uint16_t* src = pixels + row * src_w;

    for (int bx = 0; bx < w; bx += 8) {
      const int cx = dx + bx;
      if (cx >= kWidth) break;
      const int bits = std::min(8, w - bx);
      int byte_col = cx / 8;
      int bit_shift = cx % 8;

      uint8_t byte = 0xFF;
      for (int b = 0; b < bits; ++b) {
        if ((cx + b) < 0) continue;
        const uint16_t p = src[bx + b];
        if (p == 0xFFFF) continue;  // pure white
        const uint32_t r5 = (p >> 11) & 0x1F;
        const uint32_t g6 = (p >> 5) & 0x3F;
        const uint32_t b5 = p & 0x1F;
        if (r5 * 77 + g6 * 150 + b5 * 29 < 2400) {  // ~50% luminance threshold
          byte &= static_cast<uint8_t>(~(0x80 >> b));
        }
      }

      size_t idx = static_cast<size_t>(cy) * kBpr + byte_col;
      if (bit_shift == 0) {
        buffer_[idx] = byte;
      } else {
        buffer_[idx] = (buffer_[idx] & (0xFFu >> (8 - bit_shift))) | (byte << bit_shift);
        int next_bits = bits - (8 - bit_shift);
        if (next_bits > 0 && byte_col + 1 < kBpr) {
          buffer_[idx + 1] = (buffer_[idx + 1] & (0xFFu << next_bits)) | (byte >> (8 - bit_shift));
        }
      }
    }
  }
}

void MonoCanvas::to_epd_native(std::array<uint8_t, kNativeBufferSize>& out) const {
  const int64_t t0 = esp_timer_get_time();
  std::fill(out.begin(), out.end(), 0xFF);

  constexpr uint16_t kBytesPerRow = (kWidth + 7) / 8;
  constexpr uint16_t kNativeBytesPerRow = config::kDisplayNativeWidth / 8;

  uint32_t black_pixels = 0;
  uint32_t skipped_bytes = 0;

  for (uint16_t ly = 0; ly < kHeight; ++ly) {
    const uint16_t native_x = static_cast<uint16_t>(config::kDisplayNativeWidth - 1 - ly);
    const uint16_t byte_col = native_x / 8;
    const uint8_t mask = static_cast<uint8_t>(~(0x80 >> (native_x % 8)));
    const uint8_t* row = &buffer_[ly * kBytesPerRow];

    for (uint16_t bx = 0; bx < kBytesPerRow; ++bx) {
      const uint8_t byte = row[bx];
      if (byte == 0xFF) {
        ++skipped_bytes;
        continue;
      }
      for (int b = 0; b < 8; ++b) {
        if ((byte & (0x80 >> b)) == 0) {
          ++black_pixels;
          const uint16_t native_y = bx * 8 + b;
          out[static_cast<size_t>(native_y) * kNativeBytesPerRow + byte_col] &= mask;
        }
      }
    }
  }

  const int64_t t1 = esp_timer_get_time();
  ESP_LOGD("canvas",
           "to_epd_native: black=%u skipped_bytes=%u loop=%lldus",
           black_pixels, skipped_bytes, t1 - t0);
}

}  // namespace esp32calc
