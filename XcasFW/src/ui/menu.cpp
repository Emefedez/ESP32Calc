#include "ui/menu.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "esp_log.h"
#include "freertos/task.h"
#include "hardware/keymap.h"
#include "ui/input_behavior.h"

namespace esp32calc_alt {
namespace {

constexpr const char* TAG = "alt_ui";
constexpr int kCharWidth = 6;
constexpr int kInputX = 5;
constexpr int kInputY = 28;
constexpr int kInputWidth = 240;
constexpr int kInputHeight = 31;
constexpr int kInputTextX = 10;
constexpr int kInputTextY = 40;
constexpr const char* kVariableTokens[] = {"x", "y", "z", "a", "b", "c"};
constexpr size_t kVariableCount = sizeof(kVariableTokens) / sizeof(kVariableTokens[0]);

bool has_text(const char* text) {
  return text != nullptr && text[0] != '\0';
}

}  // namespace

MenuUi::MenuUi(QueueHandle_t app_events, Weact213BwDisplay& display, MathService& math)
    : app_events_(app_events), display_(display), math_(math) {}

void MenuUi::run() {
  ESP_LOGI(TAG, "starting alt menu");
  render();

  while (true) {
    AppEvent event {};
    if (xQueueReceive(app_events_, &event, pdMS_TO_TICKS(25)) == pdTRUE) {
      if (event.type == AppEventType::Key) {
        update_from_key(event.key);
      }
      while (xQueueReceive(app_events_, &event, 0) == pdTRUE) {
        if (event.type == AppEventType::Key) {
          update_from_key(event.key);
        }
      }
      render();
    }

    MathResult result {};
    if (math_.poll_result(result, 0)) {
      apply_math_result(result);
      render();
    }
  }
}

void MenuUi::update_from_key(const KeyEvent& key) {
  if (key.phase != KeyPhase::Pressed) {
    return;
  }

  const KeyDef& def = key_at(key.row, key.col);
  if (is_blank_key(def)) {
    return;
  }

  if (def.role == KeyRole::Shift) {
    shift_ = !shift_;
    return;
  }
  if (def.role == KeyRole::Alpha) {
    alpha_ = !alpha_;
    return;
  }

  KeyEvent modified = key;
  modified.shift = shift_;
  modified.alpha = alpha_;

  if (def.role == KeyRole::Mode) {
    if (screen_ == Screen::Graph) {
      screen_ = Screen::Standard;
      status_ = "STANDARD";
    } else {
      open_graph_from_expression();
    }
    consume_modifiers();
    full_refresh_pending_ = true;
    return;
  }

  if (screen_ == Screen::Graph) {
    apply_graph_key(modified);
  } else {
    apply_standard_key(modified);
  }

  consume_modifiers();
}

void MenuUi::apply_standard_key(const KeyEvent& key) {
  const KeyDef& def = key_at(key.row, key.col);

  if (variable_palette_ != VariablePalette::None) {
    handle_variable_palette_key(key);
    return;
  }

  if (ui_behavior::kShiftVariableOpensSelector && key.shift && def.role == KeyRole::Variable) {
    open_variable_palette(VariablePalette::Plain);
    return;
  }

  if (ui_behavior::kShiftVariableSquareOpensSelector && key.shift &&
      def.role == KeyRole::VariableSquare) {
    open_variable_palette(VariablePalette::Square);
    return;
  }

  if (def.role == KeyRole::Clear) {
    clear_expression();
    status_ = "ENTER SENDS";
    return;
  }

  if (def.role == KeyRole::Delete) {
    delete_expression_char();
    status_ = "EDIT";
    return;
  }

  if (def.role == KeyRole::Left) {
    move_cursor_left(key.shift);
    status_ = "EDIT";
    return;
  }

  if (def.role == KeyRole::Right) {
    move_cursor_right(key.shift);
    status_ = "EDIT";
    return;
  }

  if (def.role == KeyRole::Up) {
    cursor_ = 0;
    status_ = "EDIT";
    return;
  }

  if (def.role == KeyRole::Down) {
    cursor_ = expression_len_;
    status_ = "EDIT";
    return;
  }

  if (def.role == KeyRole::Enter) {
    if (ui_behavior::kShiftEqualsInsertsEquals && key.shift && key_is_equals(key)) {
      status_ = append_expression(ui_behavior::kShiftEqualsToken) ? "ENTER SENDS" : "EXPR FULL";
      return;
    }

    if (ui_behavior::kAlphaEqualsOpensGraph && key.alpha && key_is_equals(key)) {
      open_graph_from_expression();
      return;
    }

    submit_expression();
    return;
  }

  if (def.role == KeyRole::FractionToggle) {
    status_ = "S<>D TODO";
    return;
  }

  const char* token = key_input(def, key.shift, key.alpha);
  if (append_expression(token)) {
    status_ = "ENTER SENDS";
  } else if (has_text(token)) {
    status_ = "EXPR FULL";
  }
}

void MenuUi::apply_graph_key(const KeyEvent& key) {
  const KeyDef& def = key_at(key.row, key.col);

  if (def.role == KeyRole::Clear || def.role == KeyRole::Mode) {
    screen_ = Screen::Standard;
    status_ = "STANDARD";
    full_refresh_pending_ = true;
    return;
  }

  if (def.role == KeyRole::Enter) {
    screen_ = Screen::Standard;
    status_ = "EDIT GRAPH EXPR";
    full_refresh_pending_ = true;
    return;
  }
}

void MenuUi::apply_math_result(const MathResult& result) {
  std::snprintf(result_text_, sizeof(result_text_), "%s", result.text);
  result_visible_ = true;
  result_is_error_ = !result.ok;
  status_ = result.ok ? "DONE" : "ERROR";
}

void MenuUi::consume_modifiers() {
  shift_ = false;
  alpha_ = false;
}

bool MenuUi::append_expression(const char* token) {
  if (!has_text(token)) {
    return false;
  }

  const size_t token_len = std::strlen(token);
  if (expression_len_ + token_len >= sizeof(expression_)) {
    return false;
  }

  if (cursor_ > expression_len_) {
    cursor_ = expression_len_;
  }

  std::memmove(expression_ + cursor_ + token_len,
               expression_ + cursor_,
               expression_len_ - cursor_ + 1);
  std::memcpy(expression_ + cursor_, token, token_len);
  expression_len_ += token_len;
  cursor_ += token_len;
  expression_[expression_len_] = '\0';
  clear_result();
  return true;
}

void MenuUi::delete_expression_char() {
  if (expression_len_ == 0 || cursor_ == 0) {
    return;
  }

  std::memmove(expression_ + cursor_ - 1,
               expression_ + cursor_,
               expression_len_ - cursor_ + 1);
  --expression_len_;
  --cursor_;
  expression_[expression_len_] = '\0';
  clear_result();
}

void MenuUi::clear_expression() {
  expression_len_ = 0;
  cursor_ = 0;
  expression_[0] = '\0';
  variable_palette_ = VariablePalette::None;
  clear_result();
}

void MenuUi::clear_result() {
  result_text_[0] = '\0';
  result_visible_ = false;
  result_is_error_ = false;
}

void MenuUi::submit_expression() {
  if (expression_len_ == 0) {
    status_ = "EMPTY EXPR";
    return;
  }

  MathRequest request {};
  request.kind = MathJobKind::Numeric;
  std::snprintf(request.expression, sizeof(request.expression), "%s", expression_);

  if (math_.submit(request)) {
    clear_result();
    status_ = "QUEUED";
    ESP_LOGI(TAG, "queued expression: %s", expression_);
  } else {
    status_ = "QUEUE FULL";
  }
}

void MenuUi::open_graph_from_expression() {
  if (expression_len_ == 0) {
    status_ = "EMPTY GRAPH";
    return;
  }
  screen_ = Screen::Graph;
  status_ = "GRAPH MODE";
  full_refresh_pending_ = true;
}

void MenuUi::open_variable_palette(VariablePalette palette) {
  variable_palette_ = palette;
  variable_selected_ = 0;
  status_ = palette == VariablePalette::Square ? "PICK VAR^2" : "PICK VAR";
}

void MenuUi::handle_variable_palette_key(const KeyEvent& key) {
  const KeyDef& def = key_at(key.row, key.col);
  const int digit = key_digit(def);
  if (digit >= 0 && static_cast<size_t>(digit) < kVariableCount) {
    variable_selected_ = static_cast<uint8_t>(digit);
    choose_selected_variable();
    return;
  }

  switch (def.role) {
    case KeyRole::Left:
      move_variable_selection(-1);
      break;
    case KeyRole::Right:
      move_variable_selection(1);
      break;
    case KeyRole::Up:
      move_variable_selection(-3);
      break;
    case KeyRole::Down:
      move_variable_selection(3);
      break;
    case KeyRole::Enter:
      choose_selected_variable();
      break;
    case KeyRole::Clear:
    case KeyRole::Mode:
      variable_palette_ = VariablePalette::None;
      status_ = "ENTER SENDS";
      break;
    default:
      break;
  }
}

void MenuUi::choose_selected_variable() {
  char token[5] {};
  if (variable_palette_ == VariablePalette::Square) {
    std::snprintf(token, sizeof(token), "%s^2", kVariableTokens[variable_selected_]);
  } else {
    std::snprintf(token, sizeof(token), "%s", kVariableTokens[variable_selected_]);
  }

  variable_palette_ = VariablePalette::None;
  status_ = append_expression(token) ? "ENTER SENDS" : "EXPR FULL";
}

void MenuUi::move_variable_selection(int delta) {
  const int count = static_cast<int>(kVariableCount);
  int next = static_cast<int>(variable_selected_) + delta;
  while (next < 0) {
    next += count;
  }
  variable_selected_ = static_cast<uint8_t>(next % count);
}

void MenuUi::move_cursor_left(bool all_the_way) {
  if (all_the_way) {
    cursor_ = 0;
  } else if (cursor_ > 0) {
    --cursor_;
  }
}

void MenuUi::move_cursor_right(bool all_the_way) {
  if (all_the_way) {
    cursor_ = expression_len_;
  } else if (cursor_ < expression_len_) {
    ++cursor_;
  }
}

size_t MenuUi::expression_visible_start() const {
  if (expression_len_ <= kVisibleExpressionChars) {
    return 0;
  }

  const size_t half_window = kVisibleExpressionChars / 2;
  if (cursor_ <= half_window) {
    return 0;
  }

  const size_t max_start = expression_len_ - kVisibleExpressionChars;
  return std::min(cursor_ - half_window, max_start);
}

bool MenuUi::key_is_equals(const KeyEvent& key) const {
  const KeyDef& def = key_at(key.row, key.col);
  return std::strcmp(def.label, "=") == 0;
}

void MenuUi::render() {
  canvas_.begin_frame();
  if (!first_render_done_ || full_refresh_pending_) {
    canvas_.request_full_refresh();
  }
  canvas_.clear(true);
  render_status_bar();

  if (screen_ == Screen::Graph) {
    render_graph();
  } else {
    render_standard();
  }

  const esp_err_t err = display_.update_canvas(canvas_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "display update skipped: %s", esp_err_to_name(err));
  } else {
    first_render_done_ = true;
    full_refresh_pending_ = false;
  }
}

void MenuUi::render_status_bar() {
  canvas_.draw_text(2, 2, screen_ == Screen::Graph ? ui_behavior::kGraphModeLabel : "STANDARD", 1, true);

  if (shift_) {
    canvas_.fill_rect(69, 0, 30, 11, true);
    canvas_.draw_text(72, 2, "SHIFT", 1, false);
  } else {
    canvas_.draw_text(72, 2, "SHIFT", 1, true);
  }

  if (alpha_) {
    canvas_.fill_rect(104, 0, 30, 11, true);
    canvas_.draw_text(107, 2, "ALPHA", 1, false);
  } else {
    canvas_.draw_text(107, 2, "ALPHA", 1, true);
  }

  canvas_.draw_text(209, 2, "ALT", 1, true);
  canvas_.hline(0, 13, MonoCanvas::kWidth, true);
}

void MenuUi::render_standard() {
  canvas_.draw_text(6, 17, "CALCULATE", 1, true);
  canvas_.rect(kInputX, kInputY, kInputWidth, kInputHeight, true);

  const size_t visible_start = expression_visible_start();
  const size_t visible_count =
      expression_len_ > visible_start
          ? std::min(kVisibleExpressionChars, expression_len_ - visible_start)
          : 0;
  char visible_expression[kVisibleExpressionChars + 1] {};
  if (visible_count > 0) {
    std::memcpy(visible_expression, expression_ + visible_start, visible_count);
  }
  canvas_.draw_text(kInputTextX,
                    kInputTextY,
                    expression_len_ == 0 ? "0" : visible_expression,
                    1,
                    true);

  const size_t cursor_relative = cursor_ > visible_start ? cursor_ - visible_start : 0;
  const size_t cursor_cells =
      expression_len_ == 0 ? 1 : std::min(cursor_relative, kVisibleExpressionChars);
  const int cursor_x = kInputTextX + static_cast<int>(cursor_cells) * kCharWidth;
  canvas_.vline(cursor_x, kInputTextY - 3, 13, true);

  if (result_visible_) {
    canvas_.draw_text(6, 66, result_is_error_ ? "ERR" : "=", 1, true);
    const size_t result_len = std::strlen(result_text_);
    for (size_t line = 0; line < 2; ++line) {
      const size_t line_offset = line * kVisibleResultChars;
      if (line_offset >= result_len) {
        break;
      }
      char line_text[kVisibleResultChars + 1] {};
      const size_t line_len = std::min(kVisibleResultChars, result_len - line_offset);
      std::memcpy(line_text, result_text_ + line_offset, line_len);
      canvas_.draw_text(28, 66 + static_cast<int>(line) * 12, line_text, 1, true);
    }
  }

  canvas_.hline(5, 98, 240, true);
  canvas_.draw_text(6, 108, status_, 1, true);
  canvas_.draw_text(130, 108, "SHIFT+=  ALPHA+=", 1, true);

  if (variable_palette_ != VariablePalette::None) {
    render_variable_palette();
  }
}

void MenuUi::render_graph() {
  canvas_.draw_text(6, 17, "GRAPH", 2, true);
  canvas_.rect(6, 42, 238, 62, true);
  canvas_.hline(7, 73, 236, true);
  canvas_.vline(125, 43, 60, true);

  const size_t len = std::strlen(expression_);
  const char* expression_label = expression_;
  if (len > 30) {
    expression_label = expression_ + len - 30;
  }
  canvas_.draw_text(12, 31, expression_len_ == 0 ? "EMPTY" : expression_label, 1, true);
  canvas_.draw_text(14, 56, "GRAPH VIEW PLACEHOLDER", 1, true);
  canvas_.draw_text(14, 84, "MODE/AC RETURNS", 1, true);
  canvas_.draw_text(6, 108, status_, 1, true);
}

void MenuUi::render_variable_palette() {
  canvas_.fill_rect(73, 31, 104, 72, false);
  canvas_.rect(72, 30, 106, 74, true);
  canvas_.draw_text(82,
                    38,
                    variable_palette_ == VariablePalette::Square ? "VAR^2" : "VAR",
                    1,
                    true);

  for (size_t i = 0; i < kVariableCount; ++i) {
    const int x = 85 + static_cast<int>(i % 3) * 28;
    const int y = 57 + static_cast<int>(i / 3) * 22;
    const bool selected = i == variable_selected_;
    if (selected) {
      canvas_.fill_rect(x - 6, y - 5, 22, 17, true);
    } else {
      canvas_.rect(x - 6, y - 5, 22, 17, true);
    }
    canvas_.draw_text(x, y, kVariableTokens[i], 1, !selected);
  }
}

}  // namespace esp32calc_alt
