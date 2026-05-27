#include "ui/modes/mode_registry.h"

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
constexpr size_t kVisibleResultChars = 38;
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
  const char* status_ = "ENTER SENDS";
  char* result_text_ = nullptr;
  bool result_is_error_ = false;
  VariablePalette variable_palette_ = VariablePalette::None;
  uint8_t variable_selected_ = 0;

  bool append_expression(const char* token);
  void delete_expression_char();
  void clear_expression();
  void clear_result();
  bool set_result_text(const char* text);
  bool is_variable_key(const KeyDef& def) const;
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
      if (key.shift && is_variable_key(def)) {
        open_variable_palette(def.row == 2 && def.col == 1 ? VariablePalette::Square
                                                           : VariablePalette::Plain);
        return ModeResult::Redraw;
      }

      if (def.role == KeyRole::Delete) {
        delete_expression_char();
        status_ = "ENTER SENDS";
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

  std::memcpy(expression_ + expression_len_, token, token_len);
  expression_len_ += token_len;
  expression_[expression_len_] = '\0';
  clear_result();
  return true;
}

void StandardMode::delete_expression_char() {
  if (expression_len_ == 0) {
    return;
  }
  expression_[--expression_len_] = '\0';
  clear_result();
}

void StandardMode::clear_expression() {
  expression_len_ = 0;
  expression_[0] = '\0';
  variable_palette_ = VariablePalette::None;
  clear_result();
}

void StandardMode::clear_result() {
  delete[] result_text_;
  result_text_ = nullptr;
  result_is_error_ = false;
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

bool StandardMode::is_variable_key(const KeyDef& def) const {
  return def.row == 2 && (def.col == 0 || def.col == 1);
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

void StandardMode::render_calculate(MonoCanvas& canvas) {
  render_mode_title(canvas, name());

  canvas.draw_text(6, 42, "CALCULATE", 1, true);
  canvas.rect(5, 54, 240, 28, true);

  const char* visible = expression_;
  if (expression_len_ > kVisibleExpressionChars) {
    visible = expression_ + expression_len_ - kVisibleExpressionChars;
  }
  canvas.draw_text(10, 64, expression_len_ == 0 ? "0" : visible, 1, true);

  if (result_text_ != nullptr) {
    const size_t result_len = std::strlen(result_text_);
    const char* visible_result = result_text_;
    if (result_len > kVisibleResultChars) {
      visible_result = result_text_ + result_len - kVisibleResultChars;
    }
    canvas.draw_text(10, 87, result_is_error_ ? "ERR" : "=", 1, true);
    canvas.draw_text(34, 87, visible_result, 1, true);
  }

  canvas.hline(5, 99, 240, true);
  canvas.draw_text(6, 109, status_, 1, true);
  canvas.draw_text(126, 109, "DEL/AC EDIT", 1, true);

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
