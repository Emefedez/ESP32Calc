#include "graphics/mono_canvas.h"

#include "esp_log.h"
#include "esp_timer.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace esp32calc {
namespace {

void glyph_for(char in, uint8_t (&out)[5]) {
  const char raw = in;
  char c = raw;
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
  static constexpr uint8_t lower_letters[26][5] = {
      {0x20, 0x54, 0x54, 0x54, 0x78},
      {0x7F, 0x48, 0x44, 0x44, 0x38},
      {0x38, 0x44, 0x44, 0x44, 0x20},
      {0x38, 0x44, 0x44, 0x48, 0x7F},
      {0x38, 0x54, 0x54, 0x54, 0x18},
      {0x08, 0x7E, 0x09, 0x01, 0x02},
      {0x0C, 0x52, 0x52, 0x52, 0x3E},
      {0x7F, 0x08, 0x04, 0x04, 0x78},
      {0x00, 0x44, 0x7D, 0x40, 0x00},
      {0x20, 0x40, 0x44, 0x3D, 0x00},
      {0x7F, 0x10, 0x28, 0x44, 0x00},
      {0x00, 0x41, 0x7F, 0x40, 0x00},
      {0x7C, 0x04, 0x18, 0x04, 0x78},
      {0x7C, 0x08, 0x04, 0x04, 0x78},
      {0x38, 0x44, 0x44, 0x44, 0x38},
      {0x7C, 0x14, 0x14, 0x14, 0x08},
      {0x08, 0x14, 0x14, 0x18, 0x7C},
      {0x7C, 0x08, 0x04, 0x04, 0x08},
      {0x48, 0x54, 0x54, 0x54, 0x20},
      {0x04, 0x3F, 0x44, 0x40, 0x20},
      {0x3C, 0x40, 0x40, 0x20, 0x7C},
      {0x1C, 0x20, 0x40, 0x20, 0x1C},
      {0x3C, 0x40, 0x30, 0x40, 0x3C},
      {0x44, 0x28, 0x10, 0x28, 0x44},
      {0x0C, 0x50, 0x50, 0x50, 0x3C},
      {0x44, 0x64, 0x54, 0x4C, 0x44},
  };

  static constexpr uint8_t colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
  static constexpr uint8_t semicolon[5] = {0x00, 0x56, 0x36, 0x00, 0x00};
  static constexpr uint8_t comma[5] = {0x00, 0x50, 0x30, 0x00, 0x00};
  static constexpr uint8_t dot[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
  static constexpr uint8_t dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
  static constexpr uint8_t plus[5] = {0x08, 0x08, 0x3E, 0x08, 0x08};
  static constexpr uint8_t asterisk[5] = {0x14, 0x08, 0x3E, 0x08, 0x14};
  static constexpr uint8_t slash[5] = {0x20, 0x10, 0x08, 0x04, 0x02};
  static constexpr uint8_t equals[5] = {0x14, 0x14, 0x14, 0x14, 0x14};
  static constexpr uint8_t percent[5] = {0x23, 0x13, 0x08, 0x64, 0x62};
  static constexpr uint8_t caret[5] = {0x04, 0x02, 0x01, 0x02, 0x04};
  static constexpr uint8_t left_paren[5] = {0x00, 0x1C, 0x22, 0x41, 0x00};
  static constexpr uint8_t right_paren[5] = {0x00, 0x41, 0x22, 0x1C, 0x00};
  static constexpr uint8_t less_than[5] = {0x08, 0x14, 0x22, 0x41, 0x00};
  static constexpr uint8_t greater_than[5] = {0x00, 0x41, 0x22, 0x14, 0x08};
  static constexpr uint8_t apostrophe[5] = {0x00, 0x05, 0x03, 0x00, 0x00};

  if (c >= 'a' && c <= 'z') {
    glyph = lower_letters[c - 'a'];
  } else {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
  }

  if (glyph != nullptr) {
    // Lowercase glyph already selected.
  } else if (c == ' ') {
    glyph = space;
  } else if (c >= '0' && c <= '9') {
    glyph = digits[c - '0'];
  } else if (c >= 'A' && c <= 'Z') {
    glyph = letters[c - 'A'];
  } else {
    switch (c) {
      case ':': glyph = colon; break;
      case ';': glyph = semicolon; break;
      case ',': glyph = comma; break;
      case '.': glyph = dot; break;
      case '-': glyph = dash; break;
      case '+': glyph = plus; break;
      case '*': glyph = asterisk; break;
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

void MonoCanvas::begin_frame(CanvasRefreshKind kind) {
  update_hint_ = {};
  update_hint_.kind = kind;
  if (kind == CanvasRefreshKind::Full) {
    request_full_refresh();
  }
}

void MonoCanvas::request_full_refresh() {
  update_hint_.kind = CanvasRefreshKind::Full;
  update_hint_.x = 0;
  update_hint_.y = 0;
  update_hint_.width = kWidth;
  update_hint_.height = kHeight;
}

void MonoCanvas::request_partial_refresh(int x, int y, int width, int height) {
  mark_dirty(x, y, width, height);
}

void MonoCanvas::mark_dirty(int x, int y, int width, int height) {
  if (width <= 0 || height <= 0 || update_hint_.kind == CanvasRefreshKind::Full) {
    return;
  }

  const int x0 = std::max(0, x);
  const int y0 = std::max(0, y);
  const int x1 = std::min<int>(kWidth, x + width);
  const int y1 = std::min<int>(kHeight, y + height);
  if (x0 >= x1 || y0 >= y1) {
    return;
  }

  if (!update_hint_.has_region()) {
    update_hint_.x = x0;
    update_hint_.y = y0;
    update_hint_.width = x1 - x0;
    update_hint_.height = y1 - y0;
    return;
  }

  const int ux0 = std::min(update_hint_.x, x0);
  const int uy0 = std::min(update_hint_.y, y0);
  const int ux1 = std::max(update_hint_.x + update_hint_.width, x1);
  const int uy1 = std::max(update_hint_.y + update_hint_.height, y1);
  update_hint_.x = ux0;
  update_hint_.y = uy0;
  update_hint_.width = ux1 - ux0;
  update_hint_.height = uy1 - uy0;
}

void MonoCanvas::clear(bool white) {
  std::fill(buffer_.begin(), buffer_.end(), white ? 0xFF : 0x00);
  mark_dirty(0, 0, kWidth, kHeight);
}

void MonoCanvas::set_pixel_raw(int x, int y, bool black) {
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

void MonoCanvas::set_pixel(int x, int y, bool black) {
  set_pixel_raw(x, y, black);
  mark_dirty(x, y, 1, 1);
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
  if (width <= 0) {
    return;
  }
  for (int i = 0; i < width; ++i) {
    set_pixel_raw(x + i, y, black);
  }
  mark_dirty(x, y, width, 1);
}

void MonoCanvas::vline(int x, int y, int height, bool black) {
  if (height <= 0) {
    return;
  }
  for (int i = 0; i < height; ++i) {
    set_pixel_raw(x, y + i, black);
  }
  mark_dirty(x, y, 1, height);
}

void MonoCanvas::line(int x0, int y0, int x1, int y1, bool black) {
  const int dirty_x = std::min(x0, x1);
  const int dirty_y = std::min(y0, y1);
  const int dirty_w = std::abs(x1 - x0) + 1;
  const int dirty_h = std::abs(y1 - y0) + 1;

  const int dx = std::abs(x1 - x0);
  const int sx = x0 < x1 ? 1 : -1;
  const int dy = -std::abs(y1 - y0);
  const int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;

  while (true) {
    set_pixel_raw(x0, y0, black);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    const int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
  mark_dirty(dirty_x, dirty_y, dirty_w, dirty_h);
}

void MonoCanvas::draw_line_strip(const MonoPoint* points, size_t count, bool black) {
  if (points == nullptr || count < 2) {
    return;
  }
  for (size_t i = 1; i < count; ++i) {
    line(points[i - 1].x, points[i - 1].y, points[i].x, points[i].y, black);
  }
}

void MonoCanvas::rect(int x, int y, int width, int height, bool black) {
  if (width <= 0 || height <= 0) {
    return;
  }
  hline(x, y, width, black);
  hline(x, y + height - 1, width, black);
  vline(x, y, height, black);
  vline(x + width - 1, y, height, black);
}

void MonoCanvas::fill_rect(int x, int y, int width, int height, bool black) {
  if (width <= 0 || height <= 0) {
    return;
  }
  for (int yy = 0; yy < height; ++yy) {
    for (int xx = 0; xx < width; ++xx) {
      set_pixel_raw(x + xx, y + yy, black);
    }
  }
  mark_dirty(x, y, width, height);
}

void MonoCanvas::circle(int center_x, int center_y, int radius, bool black) {
  if (radius < 0) {
    return;
  }

  int x = radius;
  int y = 0;
  int err = 0;

  while (x >= y) {
    set_pixel_raw(center_x + x, center_y + y, black);
    set_pixel_raw(center_x + y, center_y + x, black);
    set_pixel_raw(center_x - y, center_y + x, black);
    set_pixel_raw(center_x - x, center_y + y, black);
    set_pixel_raw(center_x - x, center_y - y, black);
    set_pixel_raw(center_x - y, center_y - x, black);
    set_pixel_raw(center_x + y, center_y - x, black);
    set_pixel_raw(center_x + x, center_y - y, black);

    if (err <= 0) {
      ++y;
      err += 2 * y + 1;
    }
    if (err > 0) {
      --x;
      err -= 2 * x + 1;
    }
  }
  mark_dirty(center_x - radius, center_y - radius, radius * 2 + 1, radius * 2 + 1);
}

void MonoCanvas::fill_circle(int center_x, int center_y, int radius, bool black) {
  if (radius < 0) {
    return;
  }

  int x = radius;
  int y = 0;
  int err = 0;

  while (x >= y) {
    hline(center_x - x, center_y + y, x * 2 + 1, black);
    hline(center_x - x, center_y - y, x * 2 + 1, black);
    hline(center_x - y, center_y + x, y * 2 + 1, black);
    hline(center_x - y, center_y - x, y * 2 + 1, black);

    if (err <= 0) {
      ++y;
      err += 2 * y + 1;
    }
    if (err > 0) {
      --x;
      err -= 2 * x + 1;
    }
  }
}

void MonoCanvas::triangle(MonoPoint a, MonoPoint b, MonoPoint c, bool black) {
  line(a.x, a.y, b.x, b.y, black);
  line(b.x, b.y, c.x, c.y, black);
  line(c.x, c.y, a.x, a.y, black);
}

void MonoCanvas::fill_triangle(MonoPoint a, MonoPoint b, MonoPoint c, bool black) {
  if (b.y < a.y) std::swap(a, b);
  if (c.y < a.y) std::swap(a, c);
  if (c.y < b.y) std::swap(b, c);

  auto edge_x = [](MonoPoint p0, MonoPoint p1, int y) {
    if (p0.y == p1.y) {
      return p0.x;
    }
    return p0.x + static_cast<int>((static_cast<int64_t>(p1.x - p0.x) * (y - p0.y)) /
                                   (p1.y - p0.y));
  };

  for (int y = a.y; y <= c.y; ++y) {
    const int x0 = edge_x(a, c, y);
    const int x1 = y < b.y ? edge_x(a, b, y) : edge_x(b, c, y);
    hline(std::min(x0, x1), y, std::abs(x1 - x0) + 1, black);
  }
}

void MonoCanvas::plot_graph(int x,
                            int y,
                            int width,
                            int height,
                            int x_axis,
                            int y_axis,
                            const MonoPoint* points,
                            size_t count,
                            bool black) {
  if (width <= 0 || height <= 0) {
    return;
  }

  rect(x, y, width, height, black);
  if (x_axis >= y && x_axis < y + height) {
    hline(x, x_axis, width, black);
  }
  if (y_axis >= x && y_axis < x + width) {
    vline(y_axis, y, height, black);
  }
  draw_line_strip(points, count, black);
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
