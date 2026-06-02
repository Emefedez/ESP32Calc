#include "ui/menu.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "graphics/mono_canvas.h"
#include "ui/menu_constants.h"
#include "ui/menu_detail.h"

namespace esp32calc_alt {
namespace {

namespace constants = menu_constants;

bool expand_graph_expression(const char* input, char* output, size_t output_size) {
  return constants::expand_scientific_constants(input, output, output_size);
}

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
  graph_count_ = 0;
  graph_has_result_ = false;
  graph_has_error_ = false;
  open_mode(ModeKind::Graph);
  MathRequest request {};
  request.kind = MathJobKind::Graph;
  char expanded_expression[sizeof(request.expression)] {};
  if (!expand_graph_expression(graph_expression_,
                               expanded_expression,
                               sizeof(expanded_expression))) {
    status_ = "GRAPH EXPR TOO LONG";
    graph_has_error_ = true;
    return;
  }
  std::snprintf(request.expression, sizeof(request.expression), "%s", expanded_expression);
  if (math_.submit(request)) {
    status_ = "GRAPH QUEUED";
  } else {
    status_ = "QUEUE FULL";
    graph_has_error_ = true;
  }
}
void MenuUi::apply_graph_result(const MathResult& result) {
  if (result.kind != MathJobKind::Graph) {
    return;
  }
  graph_count_ = result.graph_count > kGraphSampleCount ? kGraphSampleCount : result.graph_count;
  for (size_t i = 0; i < graph_count_; ++i) {
    graph_y_[i] = result.graph_y[i];
    graph_valid_[i] = result.graph_valid[i];
  }
  graph_has_result_ = result.ok;
  graph_has_error_ = !result.ok;
  std::snprintf(result_text_, sizeof(result_text_), "%s", result.text);
  status_ = result.ok ? "GRAPH READY" : result_text_;
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
  bool has_visible_value = false;
  auto map_y = [&](float y_value) {
    const float top = static_cast<float>(gy + 1);
    const float bottom = static_cast<float>(gy + gh - 2);
    const float normalized =
        (y_value - constants::kGraphYMin) / (constants::kGraphYMax - constants::kGraphYMin);
    return static_cast<int>(bottom - normalized * (bottom - top));
  };

  const int x_axis =
      constants::kGraphYMin <= 0.0f && constants::kGraphYMax >= 0.0f ? map_y(0.0f) : gy + gh / 2;
  canvas_.plot_graph(gx, gy, gw, gh, x_axis, y_axis, nullptr, 0, true);

  if (graph_has_error_) {
    canvas_.draw_text(gx + 8, gy + gh / 2 - 4, status_, 1, true);
    canvas_.draw_text(6, 108, "AC/MODE RETURNS", 1, true);
    return;
  }
  if (!graph_has_result_) {
    canvas_.draw_text(gx + 8, gy + gh / 2 - 4, "GRAPH LOADING", 1, true);
    canvas_.draw_text(6, 108, status_, 1, true);
    return;
  }

  std::array<MonoPoint, kGraphSampleCount> points {};
  std::array<bool, kGraphSampleCount> visible {};
  for (size_t i = 0; i < graph_count_; ++i) {
    if (!graph_valid_[i] ||
        graph_y_[i] < constants::kGraphYMin ||
        graph_y_[i] > constants::kGraphYMax) {
      continue;
    }
    const int px = gx + 1 +
                   static_cast<int>(i) * (gw - 3) /
                       static_cast<int>(graph_count_ > 1 ? graph_count_ - 1 : 1);
    points[i] = MonoPoint {px, map_y(graph_y_[i])};
    visible[i] = true;
    has_visible_value = true;
  }

  if (!has_visible_value) {
    canvas_.draw_text(gx + 8, gy + gh / 2 - 4, "OUT OF VIEW", 1, true);
    canvas_.draw_text(6, 108, status_, 1, true);
    return;
  }

  for (size_t i = 1; i < graph_count_; ++i) {
    if (visible[i - 1] && visible[i] &&
        std::abs(points[i].y - points[i - 1].y) < gh * 2) {
      canvas_.line(points[i - 1].x, points[i - 1].y, points[i].x, points[i].y, true);
    }
  }

  canvas_.draw_text(6, 108, status_, 1, true);
}

}  // namespace esp32calc_alt
