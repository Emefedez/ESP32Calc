#include "ui/menu.h"

#include <algorithm>
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

constexpr int kGraphFooterY = 116;
constexpr int kGraphX = 5;
constexpr int kGraphY = 29;
constexpr float kRangeEpsilon = 0.0001f;
constexpr float kGraphCachePadding = 1.0f;

bool same_range(float left, float right) {
  return std::fabs(left - right) <= kRangeEpsilon;
}

}  // namespace

void MenuUi::apply_graph_key(const KeyEvent& key) {
  const KeyDef& def = key_at(key.row, key.col);

  if (def.role == KeyRole::Clear || def.role == KeyRole::Mode) {
    open_mode(ModeKind::Standard);
    return;
  }

  switch (def.role) {
    case KeyRole::Left:
      pan_graph(-0.25f, 0.0f);
      return;
    case KeyRole::Right:
      pan_graph(0.25f, 0.0f);
      return;
    case KeyRole::Up:
      pan_graph(0.0f, 0.25f);
      return;
    case KeyRole::Down:
      pan_graph(0.0f, -0.25f);
      return;
    case KeyRole::FractionToggle:
      graph_show_numbers_ = !graph_show_numbers_;
      status_ = graph_show_numbers_ ? "NUMBERS ON" : "NUMBERS OFF";
      return;
    default:
      break;
  }

  const char* token = key_input(def, key.shift, key.alpha);
  if (token != nullptr && std::strcmp(token, "+") == 0) {
    zoom_graph(0.5f);
    return;
  }
  if (token != nullptr && std::strcmp(token, "-") == 0) {
    zoom_graph(2.0f);
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
  graph_x_min_ = constants::kGraphXMin;
  graph_x_max_ = constants::kGraphXMax;
  graph_y_min_ = constants::kGraphYMin;
  graph_y_max_ = constants::kGraphYMax;
  graph_auto_y_ = true;
  graph_count_ = 0;
  graph_has_result_ = false;
  graph_has_error_ = false;
  open_mode(ModeKind::Graph);
  queue_graph_sample();
}
void MenuUi::queue_graph_sample() {
  graph_count_ = 0;
  graph_has_result_ = false;
  graph_has_error_ = false;
  MathRequest request {};
  request.kind = MathJobKind::Graph;
  const float x_span = graph_x_max_ - graph_x_min_;
  request.graph_x_min = graph_x_min_ - x_span * kGraphCachePadding;
  request.graph_x_max = graph_x_max_ + x_span * kGraphCachePadding;
  char expanded_expression[sizeof(request.expression)] {};
  if (!menu_detail::expand_for_math(graph_expression_,
                                    expanded_expression,
                                    sizeof(expanded_expression))) {
    status_ = "GRAPH EXPR TOO LONG";
    graph_has_error_ = true;
    return;
  }
  if (restore_graph_cache(graph_expression_)) {
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
bool MenuUi::restore_graph_cache(const char* expression) {
  if (!menu_detail::has_text(expression) || graph_cache_ == nullptr) {
    return false;
  }

  for (size_t i = 0; i < graph_cache_capacity_; ++i) {
    GraphCacheEntry& entry = graph_cache_[i];
    if (!entry.used ||
        std::strcmp(entry.expression, expression) != 0 ||
        !graph_view_inside_sample(entry.x_min, entry.x_max)) {
      continue;
    }
    copy_graph_view_from_series(entry.y, entry.valid, entry.count, entry.x_min, entry.x_max);
    if (graph_auto_y_) {
      fit_graph_y_to_visible_values();
    }
    graph_has_result_ = true;
    graph_has_error_ = false;
    entry.age = ++graph_cache_age_;
    status_ = "GRAPH CACHE";
    return true;
  }
  return false;
}
void MenuUi::store_graph_cache(const MathResult& result) {
  if (!result.ok || graph_cache_ == nullptr || graph_cache_capacity_ == 0) {
    return;
  }

  GraphCacheEntry* target = nullptr;
  for (size_t i = 0; i < graph_cache_capacity_; ++i) {
    GraphCacheEntry& entry = graph_cache_[i];
    if (entry.used &&
        std::strcmp(entry.expression, graph_expression_) == 0 &&
        same_range(entry.x_min, result.graph_x_min) &&
        same_range(entry.x_max, result.graph_x_max)) {
      target = &entry;
      break;
    }
    if (!entry.used && target == nullptr) {
      target = &entry;
    }
  }

  if (target == nullptr) {
    target = &graph_cache_[0];
    for (size_t i = 1; i < graph_cache_capacity_; ++i) {
      if (graph_cache_[i].age < target->age) {
        target = &graph_cache_[i];
      }
    }
  }

  target->used = true;
  target->age = ++graph_cache_age_;
  target->x_min = result.graph_x_min;
  target->x_max = result.graph_x_max;
  target->count = result.graph_count > kGraphSampleCount ? kGraphSampleCount : result.graph_count;
  std::snprintf(target->expression, sizeof(target->expression), "%s", graph_expression_);
  for (size_t i = 0; i < target->count; ++i) {
    target->y[i] = result.graph_y[i];
    target->valid[i] = result.graph_valid[i];
  }
}
bool MenuUi::graph_view_inside_sample(float sample_min, float sample_max) const {
  return sample_min <= graph_x_min_ + kRangeEpsilon &&
         sample_max >= graph_x_max_ - kRangeEpsilon;
}
bool MenuUi::copy_graph_view_from_series(const float* y,
                                         const bool* valid,
                                         size_t count,
                                         float sample_min,
                                         float sample_max) {
  graph_count_ = kGraphSampleCount;
  if (y == nullptr || valid == nullptr || count == 0 ||
      sample_max <= sample_min ||
      !graph_view_inside_sample(sample_min, sample_max)) {
    for (size_t i = 0; i < graph_count_; ++i) {
      graph_y_[i] = 0.0f;
      graph_valid_[i] = false;
    }
    return false;
  }

  const float view_span = graph_x_max_ - graph_x_min_;
  const float sample_span = sample_max - sample_min;
  for (size_t i = 0; i < graph_count_; ++i) {
    const float t = graph_count_ <= 1
                        ? 0.0f
                        : static_cast<float>(i) / static_cast<float>(graph_count_ - 1);
    const float x_value = graph_x_min_ + view_span * t;
    const float sample_pos = std::clamp(
        (x_value - sample_min) / sample_span * static_cast<float>(count - 1),
        0.0f,
        static_cast<float>(count - 1));
    const size_t left = static_cast<size_t>(std::floor(sample_pos));
    const size_t right = std::min(left + 1, count - 1);
    const float mix = sample_pos - static_cast<float>(left);

    if (left >= count || !valid[left] || !valid[right] ||
        !std::isfinite(y[left]) || !std::isfinite(y[right])) {
      graph_y_[i] = 0.0f;
      graph_valid_[i] = false;
      continue;
    }

    graph_y_[i] = y[left] + (y[right] - y[left]) * mix;
    graph_valid_[i] = true;
  }
  return true;
}
void MenuUi::fit_graph_y_to_visible_values() {
  bool have_range = false;
  float y_min = 0.0f;
  float y_max = 0.0f;
  for (size_t i = 0; i < graph_count_; ++i) {
    if (!graph_valid_[i] || !std::isfinite(graph_y_[i])) {
      continue;
    }
    if (!have_range) {
      y_min = graph_y_[i];
      y_max = graph_y_[i];
      have_range = true;
    } else {
      y_min = std::min(y_min, graph_y_[i]);
      y_max = std::max(y_max, graph_y_[i]);
    }
  }
  if (!have_range) {
    return;
  }
  const float span = y_max - y_min;
  const float pad = span > 0.0f ? span * 0.08f : 1.0f;
  graph_y_min_ = y_min - pad;
  graph_y_max_ = y_max + pad;
}
void MenuUi::pan_graph(float dx_fraction, float dy_fraction) {
  const float x_span = graph_x_max_ - graph_x_min_;
  const float y_span = graph_y_max_ - graph_y_min_;
  const float dx = x_span * dx_fraction;
  const float dy = y_span * dy_fraction;
  graph_x_min_ += dx;
  graph_x_max_ += dx;
  graph_y_min_ += dy;
  graph_y_max_ += dy;
  if (dy_fraction != 0.0f) {
    graph_auto_y_ = false;
  }
  status_ = "PAN";
  queue_graph_sample();
}
void MenuUi::zoom_graph(float factor) {
  if (factor <= 0.0f) {
    return;
  }
  const float x_mid = 0.5f * (graph_x_min_ + graph_x_max_);
  const float y_mid = 0.5f * (graph_y_min_ + graph_y_max_);
  const float x_half = 0.5f * (graph_x_max_ - graph_x_min_) * factor;
  const float y_half = 0.5f * (graph_y_max_ - graph_y_min_) * factor;
  graph_x_min_ = x_mid - x_half;
  graph_x_max_ = x_mid + x_half;
  graph_y_min_ = y_mid - y_half;
  graph_y_max_ = y_mid + y_half;
  graph_auto_y_ = false;
  status_ = factor < 1.0f ? "ZOOM IN" : "ZOOM OUT";
  queue_graph_sample();
}
void MenuUi::apply_graph_result(const MathResult& result) {
  if (result.kind != MathJobKind::Graph) {
    return;
  }
  store_graph_cache(result);
  if (!result.ok) {
    graph_has_result_ = false;
    graph_has_error_ = true;
    std::snprintf(result_text_, sizeof(result_text_), "%s", result.text);
    status_ = result_text_;
    return;
  }
  if (!graph_view_inside_sample(result.graph_x_min, result.graph_x_max)) {
    return;
  }
  copy_graph_view_from_series(result.graph_y,
                              result.graph_valid,
                              result.graph_count,
                              result.graph_x_min,
                              result.graph_x_max);
  if (result.ok && graph_auto_y_) {
    fit_graph_y_to_visible_values();
  }
  graph_has_result_ = result.ok;
  graph_has_error_ = !result.ok;
  std::snprintf(result_text_, sizeof(result_text_), "%s", result.text);
  status_ = result.ok ? "GRAPH READY" : result_text_;
}
void MenuUi::render_graph() {
  const size_t len = std::strlen(graph_expression_);
  const char* expression_label = graph_expression_;
  if (len > constants::kGraphLabelChars) {
    expression_label = graph_expression_ + len - constants::kGraphLabelChars;
  }
  menu_detail::draw_math_text(canvas_, 5, 22, expression_label);

  constexpr int gx = kGraphX;
  constexpr int gy = kGraphY;
  const int gw = MonoCanvas::kWidth - gx * 2;
  const int gh = MonoCanvas::kHeight - gy - 18;
  const int y_axis = gx + gw / 2;
  bool has_visible_value = false;
  auto map_y = [&](float y_value) {
    const float top = static_cast<float>(gy + 1);
    const float bottom = static_cast<float>(gy + gh - 2);
    const float normalized =
        (y_value - graph_y_min_) / (graph_y_max_ - graph_y_min_);
    return static_cast<int>(bottom - normalized * (bottom - top));
  };

  const int x_axis =
      graph_y_min_ <= 0.0f && graph_y_max_ >= 0.0f ? map_y(0.0f) : gy + gh / 2;
  const int y_axis_display =
      graph_x_min_ <= 0.0f && graph_x_max_ >= 0.0f
          ? gx + 1 + static_cast<int>((0.0f - graph_x_min_) / (graph_x_max_ - graph_x_min_) *
                                      static_cast<float>(gw - 3))
          : y_axis;
  canvas_.plot_graph(gx, gy, gw, gh, x_axis, y_axis_display, nullptr, 0, true);

  if (graph_has_error_) {
    canvas_.draw_text(gx + 8, gy + gh / 2 - 4, status_, 1, true);
    canvas_.draw_text(6, kGraphFooterY, "AC/MODE BACK", 1, true);
    return;
  }
  if (!graph_has_result_) {
    canvas_.draw_text(gx + 8, gy + gh / 2 - 4, "GRAPH LOADING", 1, true);
    canvas_.draw_text(6, kGraphFooterY, status_, 1, true);
    return;
  }

  std::array<MonoPoint, kGraphSampleCount> points {};
  std::array<bool, kGraphSampleCount> visible {};
  for (size_t i = 0; i < graph_count_; ++i) {
    if (!graph_valid_[i] ||
        graph_y_[i] < graph_y_min_ ||
        graph_y_[i] > graph_y_max_) {
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
    canvas_.draw_text(6, kGraphFooterY, "ARROWS PAN  +/- ZOOM", 1, true);
    return;
  }

  for (size_t i = 1; i < graph_count_; ++i) {
    if (visible[i - 1] && visible[i] &&
        std::abs(points[i].y - points[i - 1].y) < gh * 2) {
      canvas_.line(points[i - 1].x, points[i - 1].y, points[i].x, points[i].y, true);
    }
  }

  if (graph_show_numbers_) {
    char xr[24] {};
    char yr[24] {};
    std::snprintf(xr,
                  sizeof(xr),
                  "X %.2g..%.2g",
                  static_cast<double>(graph_x_min_),
                  static_cast<double>(graph_x_max_));
    std::snprintf(yr,
                  sizeof(yr),
                  "Y %.2g..%.2g",
                  static_cast<double>(graph_y_min_),
                  static_cast<double>(graph_y_max_));
    canvas_.draw_text(gx + 3, gy + 3, xr, 1, true);
    canvas_.draw_text(gx + 3, gy + gh - 10, yr, 1, true);
  }

  canvas_.draw_text(6,
                    kGraphFooterY,
                    graph_show_numbers_ ? "ARROWS PAN  +/- ZOOM  S<>D HIDE"
                                        : "ARROWS PAN  +/- ZOOM  S<>D NUM",
                    1,
                    true);
}

}  // namespace esp32calc_alt
