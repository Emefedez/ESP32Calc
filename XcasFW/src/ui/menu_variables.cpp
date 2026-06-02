#include "ui/menu.h"

#include <cstdio>

#include "hardware/keymap.h"
#include "ui/menu_constants.h"

namespace esp32calc_alt {
namespace {

namespace constants = menu_constants;

}  // namespace

void MenuUi::open_variable_palette(VariablePalette palette) {
  variable_palette_ = palette;
  variable_selected_ = 0;
  status_ = palette == VariablePalette::Square ? "PICK VAR^2" : "PICK VAR";
}

void MenuUi::handle_variable_palette_key(const KeyEvent& key) {
  const KeyDef& def = key_at(key.row, key.col);
  const int digit = key_digit(def);
  if (digit >= 0 && static_cast<size_t>(digit) < constants::kVariableCount) {
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
    char power_token[8] {};
    std::snprintf(power_token,
                  sizeof(power_token),
                  "%s^(2)",
                  constants::kVariableTokens[variable_selected_]);
    variable_palette_ = VariablePalette::None;
    status_ = append_expression(power_token) ? "ENTER SENDS" : "EXPR FULL";
    return;
  }

  std::snprintf(token, sizeof(token), "%s", constants::kVariableTokens[variable_selected_]);
  variable_palette_ = VariablePalette::None;
  status_ = append_expression(token) ? "ENTER SENDS" : "EXPR FULL";
}

void MenuUi::move_variable_selection(int delta) {
  const int count = static_cast<int>(constants::kVariableCount);
  int next = static_cast<int>(variable_selected_) + delta;
  while (next < 0) {
    next += count;
  }
  variable_selected_ = static_cast<uint8_t>(next % count);
}

void MenuUi::render_variable_palette() {
  canvas_.fill_rect(73, 31, 104, 72, false);
  canvas_.rect(72, 30, 106, 74, true);
  canvas_.draw_text(82,
                    38,
                    variable_palette_ == VariablePalette::Square ? "VAR^2" : "VAR",
                    1,
                    true);

  for (size_t i = 0; i < constants::kVariableCount; ++i) {
    const int x = 85 + static_cast<int>(i % 3) * 28;
    const int y = 57 + static_cast<int>(i / 3) * 22;
    const bool selected = i == variable_selected_;
    if (selected) {
      canvas_.fill_rect(x - 6, y - 5, 22, 17, true);
    } else {
      canvas_.rect(x - 6, y - 5, 22, 17, true);
    }
    canvas_.draw_text(x, y, constants::kVariableTokens[i], 1, !selected);
  }
}

}  // namespace esp32calc_alt
