#include "ui/menu.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "graphics/mono_canvas.h"
#include "ui/graph_expression.h"
#include "ui/menu_constants.h"
#include "ui/menu_detail.h"

namespace esp32calc_alt {
namespace {

namespace constants = menu_constants;

}  // namespace

void MenuUi::apply_graph_key(const KeyEvent& key) {
  const KeyDef& def = key_at(key.row, key.col);

  if (def.role == KeyRole::Clear || def.role == KeyRole::Mode) {
    open_mode(ModeKind::Standard);
    return;
  }

  if (def.role == KeyRole::Enter) {
    open_mode(ModeKind::Standard);
    status_ = "EDIT GRAPH EXPR";
    return;
  }
}
void MenuUi::open_graph_from_expression() {
  if (expression_len_ == 0) {
    status_ = "EMPTY GRAPH";
    return;
  }
  open_graph_expression(expression_);
}
void MenuUi::open_graph_expression(const char* expression) {
  if (!menu_detail::has_text(expression)) {
    status_ = "EMPTY GRAPH";
    return;
  }
  std::snprintf(graph_expression_, sizeof(graph_expression_), "%s", expression);
  open_mode(ModeKind::Graph);
}
void MenuUi::render_graph() {
  canvas_.draw_text(4, 17, "GRAPH", 2, true);

  const size_t len = std::strlen(graph_expression_);
  const char* expression_label = graph_expression_;
  if (len > constants::kGraphLabelChars) {
    expression_label = graph_expression_ + len - constants::kGraphLabelChars;
  }
  menu_detail::draw_math_text(canvas_, 88, 24, expression_label);

  constexpr int gx = constants::kGraphX;
  constexpr int gy = constants::kGraphY;
  const int gw = MonoCanvas::kWidth - gx * 2;
  const int gh = MonoCanvas::kHeight - gy - 12;
  const int y_axis = gx + gw / 2;
  const bool implicit_graph = graph_expression::should_render_implicit(graph_expression_);

  if (implicit_graph) {
    const int x_axis = gy + gh / 2;
    canvas_.plot_graph(gx, gy, gw, gh, x_axis, y_axis, nullptr, 0, true);

    bool has_value = false;
    std::array<float, MonoCanvas::kWidth> previous_row {};
    std::array<bool, MonoCanvas::kWidth> previous_ok {};

    for (int py = 1; py < gh - 1; ++py) {
      float left_value = 0.0f;
      bool left_ok = false;
      for (int px = 1; px < gw - 1 && px < static_cast<int>(previous_row.size()); ++px) {
        const float x_value =
            constants::kGraphXMin + (constants::kGraphXMax - constants::kGraphXMin) *
                                       (static_cast<float>(px) / static_cast<float>(gw - 2));
        const float y_value =
            constants::kGraphYMax - (constants::kGraphYMax - constants::kGraphYMin) *
                                       (static_cast<float>(py) / static_cast<float>(gh - 2));

        bool ok = false;
        const float value = graph_expression::evaluate_relation(graph_expression_, x_value, y_value, ok);
        if (!ok || !std::isfinite(value)) {
          left_ok = false;
          previous_ok[px] = false;
          continue;
        }

        const bool near_zero = std::fabs(value) < constants::kImplicitNearZero;
        const bool crosses_left =
            left_ok && ((left_value <= 0.0f && value >= 0.0f) ||
                        (left_value >= 0.0f && value <= 0.0f));
        const bool crosses_up =
            previous_ok[px] && ((previous_row[px] <= 0.0f && value >= 0.0f) ||
                                (previous_row[px] >= 0.0f && value <= 0.0f));
        if (near_zero || crosses_left || crosses_up) {
          canvas_.set_pixel(gx + px, gy + py, true);
          has_value = true;
        }

        left_value = value;
        left_ok = true;
        previous_row[px] = value;
        previous_ok[px] = true;
      }
    }

    if (!has_value) {
      canvas_.draw_text(gx + 8, gy + gh / 2 - 4, "GRAPH ERR", 1, true);
    }
    canvas_.draw_text(6, 108, status_, 1, true);
    return;
  }

  std::array<float, constants::kGraphPoints> y_values {};
  std::array<bool, constants::kGraphPoints> y_valid {};
  float y_min = 0.0f;
  float y_max = 0.0f;
  bool has_value = false;
  for (int i = 0; i < constants::kGraphPoints; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(constants::kGraphPoints - 1);
    const float x_value = constants::kGraphXMin + (constants::kGraphXMax - constants::kGraphXMin) * t;
    bool ok = false;
    const float y_value = graph_expression::evaluate(graph_expression_, x_value, ok);
    if (!ok || !std::isfinite(y_value)) {
      continue;
    }

    y_valid[i] = true;
    y_values[i] = y_value;
    if (!has_value) {
      y_min = y_value;
      y_max = y_value;
      has_value = true;
    } else {
      y_min = std::fmin(y_min, y_value);
      y_max = std::fmax(y_max, y_value);
    }
  }

  if (has_value && std::fabs(y_max - y_min) < 0.001f) {
    y_min -= 1.0f;
    y_max += 1.0f;
  }

  auto map_y = [&](float y_value) {
    const float top = static_cast<float>(gy + 1);
    const float bottom = static_cast<float>(gy + gh - 2);
    const float normalized = (y_value - y_min) / (y_max - y_min);
    return static_cast<int>(bottom - normalized * (bottom - top));
  };

  const int x_axis = has_value && y_min <= 0.0f && y_max >= 0.0f ? map_y(0.0f) : gy + gh / 2;
  canvas_.plot_graph(gx, gy, gw, gh, x_axis, y_axis, nullptr, 0, true);

  if (!has_value) {
    canvas_.draw_text(gx + 8, gy + gh / 2 - 4, "GRAPH ERR", 1, true);
    canvas_.draw_text(6, 108, "AC/MODE RETURNS", 1, true);
    return;
  }

  std::array<MonoPoint, constants::kGraphPoints> points {};
  for (int i = 0; i < constants::kGraphPoints; ++i) {
    if (!y_valid[i]) {
      continue;
    }
    points[i] = MonoPoint {
        gx + 1 + i * (gw - 3) / (constants::kGraphPoints - 1),
        map_y(y_values[i]),
    };
  }

  for (int i = 1; i < constants::kGraphPoints; ++i) {
    if (y_valid[i - 1] && y_valid[i]) {
      canvas_.line(points[i - 1].x, points[i - 1].y, points[i].x, points[i].y, true);
    }
  }

  canvas_.draw_text(6, 108, status_, 1, true);
}

}  // namespace esp32calc_alt
