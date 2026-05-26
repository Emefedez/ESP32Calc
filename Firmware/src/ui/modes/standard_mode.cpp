#include "ui/modes/mode_registry.h"

#include <cstring>

#include "ui/modes/mode_common.h"

namespace {

constexpr esp32calc::ModeMenuItem kItems[] = {
    {"CALCULATE", "READY"},
    {"HISTORY", "EMPTY"},
    {"CONSTANTS", "LIST"},
};
constexpr const char* kMessages[] = {
    "EXPRESSION ENTRY READY",
    "NO HISTORY YET",
    "CONSTANTS MENU",
};
constexpr size_t kItemCount = sizeof(kItems) / sizeof(kItems[0]);
constexpr size_t kExpressionCapacity = 96;
constexpr size_t kVisibleExpressionChars = 38;

}  // namespace

namespace esp32calc {

class StandardMode final : public UiMode {
 public:
  const char* name() const override;
  void on_open() override;
  ModeResult handle_key(const KeyEvent& key, const KeyDef& def) override;
  void render(MonoCanvas& canvas) override;

 private:
  uint8_t selected_ = 0;
  bool detail_open_ = false;
  char expression_[kExpressionCapacity] {};
  size_t expression_len_ = 0;

  bool append_expression(const char* token);
  void delete_expression_char();
  void clear_expression();
  void render_calculate(MonoCanvas& canvas);
};

const ModeDescriptor& standard_mode_descriptor() {
  static constexpr ModeDescriptor kDescriptor = make_mode_descriptor<StandardMode>("STANDARD");
  return kDescriptor;
}

const char* StandardMode::name() const {
  return "STANDARD";
}

void StandardMode::on_open() {
  detail_open_ = false;
  clear_expression();
}

ModeResult StandardMode::handle_key(const KeyEvent& key, const KeyDef& def) {
  if (detail_open_) {
    if (def.role == KeyRole::Clear) {
      if (selected_ == 0 && expression_len_ > 0) {
        clear_expression();
        return ModeResult::Redraw;
      } else {
        detail_open_ = false;
        return ModeResult::FullRefresh;
      }
    }

    if (selected_ == 0) {
      if (def.role == KeyRole::Delete) {
        delete_expression_char();
        return ModeResult::Redraw;
      }

      const char* token = key_input(def, key.shift, key.alpha);
      if (token != nullptr && append_expression(token)) {
        return ModeResult::Redraw;
      }
    }
    return ModeResult::None;
  }

  const ModeResult nav = handle_menu_navigation(def, selected_, kItemCount);
  if (nav != ModeResult::None) {
    return nav;
  }

  if (def.role == KeyRole::Enter) {
    detail_open_ = true;
    return ModeResult::FullRefresh;
  }
  return ModeResult::None;
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
  return true;
}

void StandardMode::delete_expression_char() {
  if (expression_len_ == 0) {
    return;
  }
  expression_[--expression_len_] = '\0';
}

void StandardMode::clear_expression() {
  expression_len_ = 0;
  expression_[0] = '\0';
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

  canvas.hline(5, 91, 240, true);
  canvas.draw_text(6, 101, "DEL ERASES  AC RETURNS", 1, true);
}

}  // namespace esp32calc
