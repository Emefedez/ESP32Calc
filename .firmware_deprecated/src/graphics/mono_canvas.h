#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "app_config.h"

namespace esp32calc {

struct MonoPoint {
  int x;
  int y;
};

enum class CanvasRefreshKind : uint8_t {
  Partial,
  Full,
};

struct CanvasUpdateHint {
  CanvasRefreshKind kind = CanvasRefreshKind::Partial;
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;

  bool has_region() const { return width > 0 && height > 0; }
};

class MonoCanvas {
 public:
  static constexpr uint16_t kWidth = config::kDisplayLogicalWidth;
  static constexpr uint16_t kHeight = config::kDisplayLogicalHeight;
  static constexpr size_t kBufferSize = ((kWidth + 7) / 8) * kHeight;
  static constexpr size_t kNativeBufferSize = config::kDisplayNativeBufferSize;

  void begin_frame(CanvasRefreshKind kind = CanvasRefreshKind::Partial);
  const CanvasUpdateHint& update_hint() const { return update_hint_; }
  void request_full_refresh();
  void request_partial_refresh(int x, int y, int width, int height);

  void clear(bool white = true);
  void set_pixel(int x, int y, bool black);
  bool pixel(int x, int y) const;
  void hline(int x, int y, int width, bool black = true);
  void vline(int x, int y, int height, bool black = true);
  void line(int x0, int y0, int x1, int y1, bool black = true);
  void draw_line_strip(const MonoPoint* points, size_t count, bool black = true);
  void rect(int x, int y, int width, int height, bool black = true);
  void fill_rect(int x, int y, int width, int height, bool black = true);
  void circle(int center_x, int center_y, int radius, bool black = true);
  void fill_circle(int center_x, int center_y, int radius, bool black = true);
  void triangle(MonoPoint a, MonoPoint b, MonoPoint c, bool black = true);
  void fill_triangle(MonoPoint a, MonoPoint b, MonoPoint c, bool black = true);
  void plot_graph(int x,
                  int y,
                  int width,
                  int height,
                  int x_axis,
                  int y_axis,
                  const MonoPoint* points,
                  size_t count,
                  bool black = true);
  void draw_char(int x, int y, char c, uint8_t scale = 1, bool black = true);
  void draw_text(int x, int y, const char* text, uint8_t scale = 1, bool black = true);

  void ClearBackground(bool white = true) { clear(white); }
  void DrawPixel(int x, int y, bool black = true) { set_pixel(x, y, black); }
  void DrawLine(int x0, int y0, int x1, int y1, bool black = true) {
    line(x0, y0, x1, y1, black);
  }
  void DrawLineStrip(const MonoPoint* points, size_t count, bool black = true) {
    draw_line_strip(points, count, black);
  }
  void DrawRectangle(int x, int y, int width, int height, bool black = true) {
    fill_rect(x, y, width, height, black);
  }
  void DrawRectangleLines(int x, int y, int width, int height, bool black = true) {
    rect(x, y, width, height, black);
  }
  void DrawCircle(int center_x, int center_y, int radius, bool black = true) {
    fill_circle(center_x, center_y, radius, black);
  }
  void DrawCircleLines(int center_x, int center_y, int radius, bool black = true) {
    circle(center_x, center_y, radius, black);
  }
  void DrawTriangle(MonoPoint a, MonoPoint b, MonoPoint c, bool black = true) {
    fill_triangle(a, b, c, black);
  }
  void DrawTriangleLines(MonoPoint a, MonoPoint b, MonoPoint c, bool black = true) {
    triangle(a, b, c, black);
  }
  void PlotGraph(int x,
                 int y,
                 int width,
                 int height,
                 int x_axis,
                 int y_axis,
                 const MonoPoint* points,
                 size_t count,
                 bool black = true) {
    plot_graph(x, y, width, height, x_axis, y_axis, points, count, black);
  }
  void DrawText(const char* text, int x, int y, uint8_t scale = 1, bool black = true) {
    draw_text(x, y, text, scale, black);
  }
  void RequestFullRefresh() { request_full_refresh(); }
  void RequestPartialRefresh(int x, int y, int width, int height) {
    request_partial_refresh(x, y, width, height);
  }

  const uint8_t* data() const { return buffer_.data(); }
  uint8_t* data() { return buffer_.data(); }
  size_t size() const { return buffer_.size(); }

  void to_epd_native(std::array<uint8_t, kNativeBufferSize>& out) const;

 private:
  void mark_dirty(int x, int y, int width, int height);
  void set_pixel_raw(int x, int y, bool black);

  std::array<uint8_t, kBufferSize> buffer_ {};
  CanvasUpdateHint update_hint_ {};
};

}  // namespace esp32calc
