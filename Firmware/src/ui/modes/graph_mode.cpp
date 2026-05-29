#include "ui/modes/mode_registry.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ui/modes/mode_common.h"

namespace {

constexpr esp32calc::ModeMenuItem kItems[] = {
    {"PLOT", "DRAW"},
    {"TABLE", "DATA"},
    {"WINDOW", "AXIS"},
    {"TRACE", "CURSOR"},
};
constexpr size_t kItemCount = sizeof(kItems) / sizeof(kItems[0]);
constexpr size_t kGraphExpressionCapacity = 96;
constexpr int kGraphPoints = 96;
constexpr size_t kGraphLabelChars = 24;
constexpr float kGraphXMin = -5.0f;
constexpr float kGraphXMax = 5.0f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kEuler = 2.71828182845904523536f;

const char* graph_expression_body(const char* expression) {
  if (expression == nullptr) {
    return "";
  }

  const char* equals = std::strchr(expression, '=');
  return equals == nullptr ? expression : equals + 1;
}

class GraphExpressionEvaluator {
 public:
  GraphExpressionEvaluator(const char* expression, float x, float y, bool allow_y)
      : input_(expression), x_(x), y_(y), allow_y_(allow_y) {}

  float evaluate(bool& ok) {
    const float value = parse_expression();
    skip_spaces();
    ok = ok_ && *input_ == '\0' && std::isfinite(value);
    return value;
  }

 private:
  const char* input_;
  float x_;
  float y_;
  bool allow_y_;
  bool ok_ = true;

  void skip_spaces() {
    while (*input_ == ' ') {
      ++input_;
    }
  }

  bool match(char expected) {
    skip_spaces();
    if (*input_ != expected) {
      return false;
    }
    ++input_;
    return true;
  }

  bool starts_primary() {
    skip_spaces();
    const unsigned char c = static_cast<unsigned char>(*input_);
    return *input_ == '(' || *input_ == '.' || std::isdigit(c) || std::isalpha(c);
  }

  float parse_expression() {
    float value = parse_term();
    while (ok_) {
      if (match('+')) {
        value += parse_term();
      } else if (match('-')) {
        value -= parse_term();
      } else {
        break;
      }
    }
    return value;
  }

  float parse_term() {
    float value = parse_unary();
    while (ok_) {
      if (match('*')) {
        value *= parse_unary();
      } else if (match('/')) {
        const float denominator = parse_unary();
        if (denominator == 0.0f) {
          ok_ = false;
          return 0.0f;
        }
        value /= denominator;
      } else if (starts_primary()) {
        value *= parse_unary();
      } else {
        break;
      }
    }
    return value;
  }

  float parse_power() {
    float value = parse_primary();
    if (ok_ && match('^')) {
      value = std::pow(value, parse_unary());
      if (!std::isfinite(value)) {
        ok_ = false;
      }
    }
    return value;
  }

  float parse_unary() {
    if (match('+')) {
      return parse_unary();
    }
    if (match('-')) {
      return -parse_unary();
    }
    return parse_power();
  }

  float parse_number() {
    char* end = nullptr;
    const float value = std::strtof(input_, &end);
    if (end == input_) {
      ok_ = false;
      return 0.0f;
    }
    input_ = end;
    return value;
  }

  bool parse_identifier(char (&identifier)[8]) {
    skip_spaces();
    size_t used = 0;
    bool has_identifier = false;
    while (std::isalpha(static_cast<unsigned char>(*input_))) {
      has_identifier = true;
      if (used + 1 < sizeof(identifier)) {
        identifier[used++] =
            static_cast<char>(std::tolower(static_cast<unsigned char>(*input_)));
      }
      ++input_;
    }
    identifier[used] = '\0';
    return has_identifier;
  }

  float parse_function_argument() {
    if (!match('(')) {
      ok_ = false;
      return 0.0f;
    }
    const float argument = parse_expression();
    if (!match(')')) {
      ok_ = false;
    }
    return argument;
  }

  float parse_identifier_value() {
    char identifier[8] {};
    if (!parse_identifier(identifier)) {
      ok_ = false;
      return 0.0f;
    }

    if (std::strcmp(identifier, "x") == 0) {
      return x_;
    }
    if (std::strcmp(identifier, "y") == 0) {
      if (!allow_y_) {
        ok_ = false;
        return 0.0f;
      }
      return y_;
    }
    if (std::strcmp(identifier, "pi") == 0) {
      return kPi;
    }
    if (std::strcmp(identifier, "e") == 0) {
      return kEuler;
    }

    const float argument = parse_function_argument();
    if (!ok_) {
      return 0.0f;
    }

    if (std::strcmp(identifier, "sin") == 0) {
      return std::sin(argument);
    }
    if (std::strcmp(identifier, "cos") == 0) {
      return std::cos(argument);
    }
    if (std::strcmp(identifier, "tan") == 0) {
      return std::tan(argument);
    }
    if (std::strcmp(identifier, "sqrt") == 0) {
      return argument < 0.0f ? (ok_ = false, 0.0f) : std::sqrt(argument);
    }
    if (std::strcmp(identifier, "ln") == 0) {
      return argument <= 0.0f ? (ok_ = false, 0.0f) : std::log(argument);
    }
    if (std::strcmp(identifier, "log") == 0) {
      return argument <= 0.0f ? (ok_ = false, 0.0f) : std::log10(argument);
    }

    ok_ = false;
    return 0.0f;
  }

  float parse_primary() {
    skip_spaces();
    if (match('(')) {
      const float value = parse_expression();
      if (!match(')')) {
        ok_ = false;
      }
      return value;
    }

    const unsigned char c = static_cast<unsigned char>(*input_);
    if (*input_ == '.' || std::isdigit(c)) {
      return parse_number();
    }
    if (std::isalpha(c)) {
      return parse_identifier_value();
    }

    ok_ = false;
    return 0.0f;
  }
};

bool expression_references_y(const char* expression) {
  if (expression == nullptr) {
    return false;
  }

  while (*expression != '\0') {
    if (!std::isalpha(static_cast<unsigned char>(*expression))) {
      ++expression;
      continue;
    }

    char identifier[8] {};
    size_t used = 0;
    while (std::isalpha(static_cast<unsigned char>(*expression))) {
      if (used + 1 < sizeof(identifier)) {
        identifier[used++] =
            static_cast<char>(std::tolower(static_cast<unsigned char>(*expression)));
      }
      ++expression;
    }
    identifier[used] = '\0';
    if (std::strcmp(identifier, "y") == 0) {
      return true;
    }
  }
  return false;
}

bool expression_is_plain_y(const char* expression) {
  if (expression == nullptr) {
    return false;
  }

  while (*expression == ' ') {
    ++expression;
  }
  if (std::tolower(static_cast<unsigned char>(*expression)) != 'y') {
    return false;
  }
  ++expression;

  while (*expression == ' ') {
    ++expression;
  }
  return *expression == '\0';
}

void copy_expression_segment(char (&output)[kGraphExpressionCapacity],
                             const char* begin,
                             size_t length) {
  const size_t copy_len = std::min(length, sizeof(output) - 1);
  std::memcpy(output, begin, copy_len);
  output[copy_len] = '\0';
}

float evaluate_expression_segment(const char* expression,
                                  float x,
                                  float y,
                                  bool allow_y,
                                  bool& ok) {
  GraphExpressionEvaluator evaluator(expression, x, y, allow_y);
  return evaluator.evaluate(ok);
}

float evaluate_graph_relation(const char* expression, float x, float y, bool& ok) {
  if (expression == nullptr) {
    ok = false;
    return 0.0f;
  }

  const char* equals = std::strchr(expression, '=');
  if (equals == nullptr) {
    return evaluate_expression_segment(expression, x, y, true, ok);
  }

  char left[kGraphExpressionCapacity] {};
  char right[kGraphExpressionCapacity] {};
  copy_expression_segment(left, expression, static_cast<size_t>(equals - expression));
  copy_expression_segment(right, equals + 1, std::strlen(equals + 1));

  bool left_ok = false;
  bool right_ok = false;
  const float left_value = evaluate_expression_segment(left, x, y, true, left_ok);
  const float right_value = evaluate_expression_segment(right, x, y, true, right_ok);
  ok = left_ok && right_ok;
  return left_value - right_value;
}

bool prefer_graph_root(float candidate, float best, bool has_best) {
  if (!has_best) {
    return true;
  }
  if (candidate >= 0.0f && best < 0.0f) {
    return true;
  }
  if (candidate < 0.0f && best >= 0.0f) {
    return false;
  }
  return std::fabs(candidate) < std::fabs(best);
}

bool solve_graph_relation_y(const char* expression, float x, float& y) {
  constexpr float kYMin = -50.0f;
  constexpr float kYMax = 50.0f;
  constexpr float kStep = 0.5f;
  constexpr float kRootEpsilon = 0.0001f;

  bool has_best = false;
  float best_y = 0.0f;
  bool prev_ok = false;
  float prev_y = kYMin;
  float prev_value = 0.0f;

  for (float sample_y = kYMin; sample_y <= kYMax + 0.0001f; sample_y += kStep) {
    bool ok = false;
    const float value = evaluate_graph_relation(expression, x, sample_y, ok);
    if (!ok || !std::isfinite(value)) {
      prev_ok = false;
      continue;
    }

    if (std::fabs(value) < kRootEpsilon) {
      if (prefer_graph_root(sample_y, best_y, has_best)) {
        best_y = sample_y;
        has_best = true;
      }
    }

    if (prev_ok && ((prev_value <= 0.0f && value >= 0.0f) ||
                    (prev_value >= 0.0f && value <= 0.0f))) {
      float low_y = prev_y;
      float high_y = sample_y;
      float low_value = prev_value;
      for (int i = 0; i < 24; ++i) {
        const float mid_y = 0.5f * (low_y + high_y);
        bool mid_ok = false;
        const float mid_value = evaluate_graph_relation(expression, x, mid_y, mid_ok);
        if (!mid_ok || !std::isfinite(mid_value)) {
          break;
        }
        if ((low_value <= 0.0f && mid_value >= 0.0f) ||
            (low_value >= 0.0f && mid_value <= 0.0f)) {
          high_y = mid_y;
        } else {
          low_y = mid_y;
          low_value = mid_value;
        }
      }

      const float root_y = 0.5f * (low_y + high_y);
      if (prefer_graph_root(root_y, best_y, has_best)) {
        best_y = root_y;
        has_best = true;
      }
    }

    prev_ok = true;
    prev_y = sample_y;
    prev_value = value;
  }

  if (!has_best) {
    return false;
  }
  y = best_y;
  return true;
}

float evaluate_graph_expression(const char* expression, float x, bool& ok) {
  const char* equals = expression == nullptr ? nullptr : std::strchr(expression, '=');
  const bool has_equals = equals != nullptr;
  const bool has_y = expression_references_y(expression);

  if (has_equals) {
    char left[kGraphExpressionCapacity] {};
    copy_expression_segment(left, expression, static_cast<size_t>(equals - expression));
    if (expression_is_plain_y(left)) {
      return evaluate_expression_segment(equals + 1, x, 0.0f, false, ok);
    }

    char right[kGraphExpressionCapacity] {};
    copy_expression_segment(right, equals + 1, std::strlen(equals + 1));
    if (expression_is_plain_y(right)) {
      return evaluate_expression_segment(left, x, 0.0f, false, ok);
    }

    if (!has_y) {
      ok = false;
      return 0.0f;
    }

    bool y0_ok = false;
    bool y1_ok = false;
    bool y2_ok = false;
    const float y0_value = evaluate_graph_relation(expression, x, 0.0f, y0_ok);
    const float y1_value = evaluate_graph_relation(expression, x, 1.0f, y1_ok);
    const float y2_value = evaluate_graph_relation(expression, x, 2.0f, y2_ok);
    const float y_slope = y1_value - y0_value;
    const float linear_error = std::fabs((y2_value - y0_value) - 2.0f * y_slope);
    if (!y0_ok || !y1_ok || !y2_ok || std::fabs(y_slope) < 0.00001f ||
        linear_error > 0.001f * (1.0f + std::fabs(y2_value))) {
      float nonlinear_y = 0.0f;
      ok = solve_graph_relation_y(expression, x, nonlinear_y);
      return ok ? nonlinear_y : 0.0f;
    }

    const float y = -y0_value / y_slope;
    ok = std::isfinite(y);
    return y;
  }

  if (has_y) {
    ok = false;
    return 0.0f;
  }

  return evaluate_expression_segment(graph_expression_body(expression), x, 0.0f, false, ok);
}

}  // namespace

namespace esp32calc {

class GraphMode final : public UiMode {
 public:
  const char* name() const override;
  void on_open() override;
  ModeResult handle_key(const KeyEvent& key, const KeyDef& def) override;
  ModeResult open_graph_expression(const char* expression) override;
  void render(MonoCanvas& canvas) override;

 private:
  enum class Screen : uint8_t {
    Menu,
    Plot,
    Table,
    Window,
    Trace,
  };

  Screen screen_ = Screen::Menu;
  uint8_t selected_ = 0;
  char graph_expression_[kGraphExpressionCapacity] = "sin(x)";

  void open_selected_screen();
  float evaluate_graph_y(float x, bool& ok) const;
  void render_plot(MonoCanvas& canvas, int gx, int gy);
  void render_table(MonoCanvas& canvas);
  void render_window(MonoCanvas& canvas);
  void render_trace(MonoCanvas& canvas);
};

const ModeDescriptor& graph_mode_descriptor() {
  static constexpr ModeDescriptor kDescriptor = make_mode_descriptor<GraphMode>("GRAPH");
  return kDescriptor;
}

const char* GraphMode::name() const {
  return "GRAPH";
}

void GraphMode::on_open() {
  screen_ = Screen::Menu;
  std::snprintf(graph_expression_, sizeof(graph_expression_), "sin(x)");
}

ModeResult GraphMode::handle_key(const KeyEvent& key, const KeyDef& def) {
  (void)key;

  if (screen_ == Screen::Menu) {
    switch (handle_menu_action(def, selected_, kItemCount)) {
      case MenuAction::SelectionChanged:
        return ModeResult::Redraw;
      case MenuAction::ItemChosen:
        open_selected_screen();
        return ModeResult::FullRefresh;
      case MenuAction::Exit:
        return ModeResult::ExitToMainMenu;
      case MenuAction::None:
      default:
        return ModeResult::None;
    }
  }

  if (def.role == KeyRole::Clear) {
    screen_ = Screen::Menu;
    return ModeResult::FullRefresh;
  }

  return ModeResult::None;
}

ModeResult GraphMode::open_graph_expression(const char* expression) {
  if (expression == nullptr || expression[0] == '\0') {
    return ModeResult::None;
  }

  std::snprintf(graph_expression_, sizeof(graph_expression_), "%s", expression);
  screen_ = Screen::Menu;
  return ModeResult::FullRefresh;
}

void GraphMode::render(MonoCanvas& canvas) {
  switch (screen_) {
    case Screen::Menu:
      render_mode_menu(canvas, name(), kItems, kItemCount, selected_);
      return;
    case Screen::Plot:
      render_plot(canvas, 10, 42);
      return;
    case Screen::Table:
      render_table(canvas);
      return;
    case Screen::Window:
      render_window(canvas);
      return;
    case Screen::Trace:
      render_trace(canvas);
      return;
  }
}

void GraphMode::open_selected_screen() {
  switch (selected_) {
    case 0:
      screen_ = Screen::Plot;
      return;
    case 1:
      screen_ = Screen::Table;
      return;
    case 2:
      screen_ = Screen::Window;
      return;
    default:
      screen_ = Screen::Trace;
      return;
  }
}

float GraphMode::evaluate_graph_y(float x, bool& ok) const {
  return evaluate_graph_expression(graph_expression_, x, ok);
}

void GraphMode::render_plot(MonoCanvas& canvas, int gx, int gy) {
  canvas.draw_text(4, 18, "GRAPH", 2, true);

  const size_t expression_len = std::strlen(graph_expression_);
  const char* expression_label = graph_expression_;
  if (expression_len > kGraphLabelChars) {
    expression_label = graph_expression_ + expression_len - kGraphLabelChars;
  }
  canvas.draw_text(88, 24, expression_label, 1, true);

  const int gw = MonoCanvas::kWidth - gx * 2;
  const int gh = MonoCanvas::kHeight - gy - 12;
  const int y_axis = gx + gw / 2;

  std::array<float, kGraphPoints> y_values {};
  std::array<bool, kGraphPoints> y_valid {};
  float y_min = 0.0f;
  float y_max = 0.0f;
  bool has_value = false;
  for (int i = 0; i < kGraphPoints; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(kGraphPoints - 1);
    const float x_value = kGraphXMin + (kGraphXMax - kGraphXMin) * t;
    bool ok = false;
    const float y_value = evaluate_graph_y(x_value, ok);
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
  std::array<MonoPoint, kGraphPoints> points {};
  if (has_value) {
    for (int i = 0; i < kGraphPoints; ++i) {
      if (!y_valid[i]) {
        continue;
      }
      points[i] = MonoPoint {
          gx + 1 + i * (gw - 3) / (kGraphPoints - 1),
          map_y(y_values[i]),
      };
    }
  }

  canvas.PlotGraph(gx,
                   gy,
                   gw,
                   gh,
                   x_axis,
                   y_axis,
                   nullptr,
                   0,
                   true);
  if (!has_value) {
    canvas.draw_text(gx + 8, gy + gh / 2 - 4, "GRAPH ERR", 1, true);
    return;
  }

  for (int i = 1; i < kGraphPoints; ++i) {
    if (y_valid[i - 1] && y_valid[i]) {
      canvas.DrawLine(points[i - 1].x, points[i - 1].y, points[i].x, points[i].y, true);
    }
  }
}

void GraphMode::render_table(MonoCanvas& canvas) {
  render_mode_title(canvas, name());
  canvas.draw_text(8, 42, "X", 1, true);
  canvas.draw_text(78, 42, "Y", 1, true);
  canvas.hline(6, 53, 180, true);

  constexpr float kValues[] = {-2.0f, -1.0f, 0.0f, 1.0f, 2.0f};
  for (size_t i = 0; i < sizeof(kValues) / sizeof(kValues[0]); ++i) {
    char x_text[16] {};
    char y_text[16] {};
    bool ok = false;
    const float y_value = evaluate_graph_y(kValues[i], ok);
    std::snprintf(x_text, sizeof(x_text), "%.1f", kValues[i]);
    if (ok) {
      std::snprintf(y_text, sizeof(y_text), "%.2f", y_value);
    } else {
      std::snprintf(y_text, sizeof(y_text), "ERR");
    }

    const int y = 62 + static_cast<int>(i) * 11;
    canvas.draw_text(8, y, x_text, 1, true);
    canvas.draw_text(78, y, y_text, 1, true);
  }
}

void GraphMode::render_window(MonoCanvas& canvas) {
  render_mode_title(canvas, name());
  canvas.draw_text(8, 43, "WINDOW", 1, true);
  canvas.hline(8, 55, 102, true);
  canvas.draw_text(8, 66, "XMIN -5", 1, true);
  canvas.draw_text(8, 78, "XMAX  5", 1, true);
  canvas.draw_text(8, 90, "Y AUTO", 1, true);
  canvas.draw_text(8, 102, graph_expression_, 1, true);
}

void GraphMode::render_trace(MonoCanvas& canvas) {
  render_mode_title(canvas, name());
  canvas.draw_text(8, 43, "TRACE", 1, true);
  canvas.hline(8, 55, 78, true);
  canvas.draw_text(8, 68, "X = 0.00", 1, true);
  canvas.draw_text(8, 82, "Y = 0.00", 1, true);
  canvas.draw_text(8, 100, "CLR RETURNS GRAPH MENU", 1, true);
}

}  // namespace esp32calc
