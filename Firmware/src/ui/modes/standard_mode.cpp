#include "ui/modes/mode_registry.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>

#include "calc/calc_engine.h"
#include "esp_log.h"
#include "ui/modes/mode_common.h"

extern esp32calc::CalcEngine g_calc;

namespace {

constexpr const char* TAG = "standard";
constexpr esp32calc::ModeMenuItem kItems[] = {
    {"CALCULATE", "READY"},
    //{"HISTORY", "EMPTY"},
    //{"CONSTANTS", "LIST"},
};
constexpr const char* kMessages[] = {
    "EXPRESSION ENTRY READY",
    //"NO HISTORY YET",
    //"CONSTANTS MENU",
};
constexpr size_t kItemCount = sizeof(kItems) / sizeof(kItems[0]);
constexpr size_t kExpressionCapacity = 96;
constexpr size_t kVisibleExpressionChars = 38;
constexpr size_t kVisibleResultChars = 36;
constexpr int kCharWidth = 6;
constexpr int kInputX = 5;
constexpr int kInputY = 28;
constexpr int kInputWidth = 240;
constexpr int kInputHeight = 31;
constexpr int kInputTextX = 10;
constexpr int kInputTextY = 40;
constexpr const char* kVariableTokens[] = {"x", "y", "z", "a", "b", "c"};
constexpr size_t kVariableCount = sizeof(kVariableTokens) / sizeof(kVariableTokens[0]);

char* duplicate_text(const char* text) {
  if (text == nullptr) {
    return nullptr;
  }

  const size_t len = std::strlen(text) + 1;
  char* copy = new (std::nothrow) char[len];
  if (copy == nullptr) {
    return nullptr;
  }
  std::memcpy(copy, text, len);
  return copy;
}

}  // namespace

namespace esp32calc {

class StandardMode final : public UiMode {
 public:
  ~StandardMode() override;

  const char* name() const override;
  void on_open() override;
  ModeResult handle_key(const KeyEvent& key, const KeyDef& def) override;
  ModeResult handle_calc_result(const CalcResult& result) override;
  void render(MonoCanvas& canvas) override;

 private:
  enum class VariablePalette : uint8_t {
    None,
    Plain,
    Square,
  };

  uint8_t selected_ = 0; // should disable fake modes
  bool detail_open_ = false;
  char expression_[kExpressionCapacity] {};
  size_t expression_len_ = 0;
  size_t cursor_ = 0;
  const char* status_ = "ENTER SENDS";
  char* result_text_ = nullptr;
  bool result_is_error_ = false;
  VariablePalette variable_palette_ = VariablePalette::None;
  uint8_t variable_selected_ = 0;

  bool append_expression(const char* token);
  void delete_expression_char();
  void clear_expression();
  void clear_result();
  void move_cursor_left(bool all_the_way);
  void move_cursor_right(bool all_the_way);
  size_t expression_visible_start() const;
  bool set_result_text(const char* text);
  void open_variable_palette(VariablePalette palette);
  ModeResult handle_variable_palette_key(const KeyDef& def);
  ModeResult choose_selected_variable();
  void render_calculate(MonoCanvas& canvas);
  void render_variable_palette(MonoCanvas& canvas);
};

const ModeDescriptor& standard_mode_descriptor() {
  static constexpr ModeDescriptor kDescriptor = make_mode_descriptor<StandardMode>("STANDARD");
  return kDescriptor;
}

StandardMode::~StandardMode() {
  clear_result();
}

const char* StandardMode::name() const {
  return "STANDARD";
}

void StandardMode::on_open() {
  detail_open_ = true;
  variable_palette_ = VariablePalette::None;
  clear_expression();
  clear_result();
  status_ = "ENTER SENDS";
}

ModeResult StandardMode::handle_key(const KeyEvent& key, const KeyDef& def) {
  if (detail_open_) {
    if (variable_palette_ != VariablePalette::None) {
      return handle_variable_palette_key(def);
    }

    if (def.role == KeyRole::Clear) {
      if (selected_ == 0 && expression_len_ > 0) {
        clear_expression();
        status_ = "ENTER SENDS";
        return ModeResult::Redraw;
      } else {
        return ModeResult::ExitToMainMenu;
      }
    }

    if (selected_ == 0) {
      if (key.shift && def.role == KeyRole::Variable) {
        open_variable_palette(VariablePalette::Plain);
        return ModeResult::Redraw;
      }

      if (key.shift && def.role == KeyRole::VariableSquare) {
        open_variable_palette(VariablePalette::Square);
        return ModeResult::Redraw;
      }

      if (def.role == KeyRole::Delete) {
        delete_expression_char();
        status_ = "ENTER SENDS";
        return ModeResult::Redraw;
      }

      if (def.role == KeyRole::Left) {
        move_cursor_left(key.shift);
        status_ = "EDIT";
        return ModeResult::Redraw;
      }

      if (def.role == KeyRole::Right) {
        move_cursor_right(key.shift);
        status_ = "EDIT";
        return ModeResult::Redraw;
      }

      if (def.role == KeyRole::Up) {
        cursor_ = 0;
        status_ = "EDIT";
        return ModeResult::Redraw;
      }

      if (def.role == KeyRole::Down) {
        cursor_ = expression_len_;
        status_ = "EDIT";
        return ModeResult::Redraw;
      }

      if (def.role == KeyRole::Enter) {
        if (key.shift && std::strcmp(def.label, "=") == 0) {
          if (append_expression("=")) {
            status_ = "ENTER SENDS";
            return ModeResult::Redraw;
          }
          status_ = "EXPR FULL";
          return ModeResult::Redraw;
        }

        if (expression_len_ == 0) {
          status_ = "EMPTY EXPR";
          return ModeResult::Redraw;
        }

        const bool submitted = key.alpha ? g_calc.submit_graph_expression(expression_)
                                         : g_calc.submit_expression(expression_);
        status_ = submitted ? (key.alpha ? "GRAPH QUEUED" : "QUEUED") : "QUEUE FULL";
        if (submitted) {
          clear_result();
        }
        if (submitted) {
          ESP_LOGI(TAG, "queued expression: %s", expression_);
        } else {
          ESP_LOGW(TAG, "failed to queue expression: %s", expression_);
        }
        return ModeResult::Redraw;
      }

      const char* token = key_input(def, key.shift, key.alpha);
      if (token != nullptr && append_expression(token)) {
        status_ = "ENTER SENDS";
        return ModeResult::Redraw;
      }
    }
    return ModeResult::None;
  }

  switch (handle_menu_action(def, selected_, kItemCount)) {
    case MenuAction::SelectionChanged:
      return ModeResult::Redraw;
    case MenuAction::ItemChosen:
      detail_open_ = true;
      status_ = "ENTER SENDS";
      return ModeResult::FullRefresh;
    case MenuAction::Exit:
      return ModeResult::ExitToMainMenu;
    case MenuAction::None:
    default:
      return ModeResult::None;
  }
}

ModeResult StandardMode::handle_calc_result(const CalcResult& result) {
  if (result.action != CalcResultAction::ShowText) {
    return ModeResult::None;
  }

  const char* text = result.display_text;
  if (text == nullptr || text[0] == '\0') {
    text = result.is_error_ ? "ERROR" : "OK";
  }

  if (!set_result_text(text)) {
    status_ = "NO MEMORY";
    result_is_error_ = true;
    return ModeResult::Redraw;
  }

  result_is_error_ = result.is_error_;
  status_ = result.is_error_ ? "ERROR" : "DONE";
  return ModeResult::Redraw;
}

void StandardMode::render(MonoCanvas& canvas) {
  if (detail_open_) {
    if (selected_ == 0) {
      render_calculate(canvas);
    } else {
      render_mode_message(canvas, name(), kMessages[selected_], "CLR RETURNS MODE MENU");
    }
    return;
  }

  render_mode_menu(canvas, name(), kItems, kItemCount, selected_);
}

bool StandardMode::append_expression(const char* token) {
  const size_t token_len = std::strlen(token);
  if (token_len == 0 || expression_len_ + token_len >= sizeof(expression_)) {
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

void StandardMode::delete_expression_char() {
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

void StandardMode::clear_expression() {
  expression_len_ = 0;
  cursor_ = 0;
  expression_[0] = '\0';
  variable_palette_ = VariablePalette::None;
  clear_result();
}

void StandardMode::clear_result() {
  delete[] result_text_;
  result_text_ = nullptr;
  result_is_error_ = false;
}

void StandardMode::move_cursor_left(bool all_the_way) {
  if (all_the_way) {
    cursor_ = 0;
  } else if (cursor_ > 0) {
    --cursor_;
  }
}

void StandardMode::move_cursor_right(bool all_the_way) {
  if (all_the_way) {
    cursor_ = expression_len_;
  } else if (cursor_ < expression_len_) {
    ++cursor_;
  }
}

size_t StandardMode::expression_visible_start() const {
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

bool StandardMode::set_result_text(const char* text) {
  char* copy = duplicate_text(text);
  if (copy == nullptr) {
    return false;
  }

  clear_result();
  result_text_ = copy;
  return true;
}

void StandardMode::open_variable_palette(VariablePalette palette) {
  variable_palette_ = palette;
  variable_selected_ = 0;
  status_ = palette == VariablePalette::Square ? "PICK VAR^2" : "PICK VAR";
}

ModeResult StandardMode::handle_variable_palette_key(const KeyDef& def) {
  switch (handle_menu_action(def, variable_selected_, kVariableCount)) {
    case MenuAction::SelectionChanged:
      return ModeResult::Redraw;
    case MenuAction::ItemChosen:
      return choose_selected_variable();
    case MenuAction::Exit:
      variable_palette_ = VariablePalette::None;
      status_ = "ENTER SENDS";
      return ModeResult::Redraw;
    case MenuAction::None:
    default:
      return ModeResult::None;
  }
}
// Submenu that lists the possible variables, very clean, I would have not thought of doing it with global constants/tokens.
ModeResult StandardMode::choose_selected_variable() {
  char token[5] {};
  if (variable_palette_ == VariablePalette::Square) {
    std::snprintf(token, sizeof(token), "%s^2", kVariableTokens[variable_selected_]);
  } else {
    std::snprintf(token, sizeof(token), "%s", kVariableTokens[variable_selected_]);
  }

  variable_palette_ = VariablePalette::None;
  if (!append_expression(token)) {
    status_ = "EXPR FULL";
    return ModeResult::Redraw;
  }

  status_ = "ENTER SENDS";
  return ModeResult::Redraw;
}
// Operation Box
void StandardMode::render_calculate(MonoCanvas& canvas) {
  canvas.draw_text(6, 17, "CALCULATE", 1, true);
  canvas.rect(kInputX, kInputY, kInputWidth, kInputHeight, true);

  const size_t visible_start = expression_visible_start();
  const size_t visible_count =
      expression_len_ > visible_start
          ? std::min(kVisibleExpressionChars, expression_len_ - visible_start)
          : 0;
  char visible_expression[kVisibleExpressionChars + 1] {};
  if (visible_count > 0) {
    std::memcpy(visible_expression, expression_ + visible_start, visible_count);
    visible_expression[visible_count] = '\0';
  }
  canvas.draw_text(kInputTextX,
                   kInputTextY,
                   expression_len_ == 0 ? "0" : visible_expression,
                   1,
                   true);

  const size_t cursor_relative = cursor_ > visible_start ? cursor_ - visible_start : 0;
  const size_t cursor_cells = expression_len_ == 0
                                  ? 1
                                  : std::min(cursor_relative, kVisibleExpressionChars);
  const int cursor_x =
      kInputTextX + static_cast<int>(cursor_cells) * kCharWidth;
  canvas.vline(cursor_x, kInputTextY - 3, 13, true);

  if (result_text_ != nullptr) {
    const size_t result_len = std::strlen(result_text_);
    const size_t max_visible_result = kVisibleResultChars * 2;
    size_t result_offset = 0;
    if (result_len > max_visible_result) {
      result_offset = result_len - max_visible_result;
    }

    canvas.draw_text(6, 66, result_is_error_ ? "ERR" : "=", 1, true);
    for (size_t line = 0; line < 2; ++line) {
      const size_t line_offset = result_offset + line * kVisibleResultChars;
      if (line_offset >= result_len) {
        break;
      }

      const size_t line_len = std::min(kVisibleResultChars, result_len - line_offset);
      char line_text[kVisibleResultChars + 1] {};
      std::memcpy(line_text, result_text_ + line_offset, line_len);
      line_text[line_len] = '\0';
      canvas.draw_text(28, 66 + static_cast<int>(line) * 12, line_text, 1, true);
    }
  }

  canvas.hline(5, 98, 240, true);
  canvas.draw_text(6, 108, status_, 1, true);
  canvas.draw_text(154, 108, "DEL AC EDIT", 1, true);

  if (variable_palette_ != VariablePalette::None) {
    render_variable_palette(canvas);
  }
}

void StandardMode::render_variable_palette(MonoCanvas& canvas) {
  canvas.fill_rect(73, 31, 104, 72, false);
  canvas.rect(72, 30, 106, 74, true);
  canvas.draw_text(82, 38, variable_palette_ == VariablePalette::Square ? "VAR^2" : "VAR", 1, true);

  for (size_t i = 0; i < kVariableCount; ++i) {
    const int x = 85 + static_cast<int>(i % 3) * 28;
    const int y = 57 + static_cast<int>(i / 3) * 22;
    const bool selected = i == variable_selected_;
    if (selected) {
      canvas.fill_rect(x - 6, y - 5, 22, 17, true);
    } else {
      canvas.rect(x - 6, y - 5, 22, 17, true);
    }
    canvas.draw_text(x, y, kVariableTokens[i], 1, !selected);
  }
}

}  // namespace esp32calc
