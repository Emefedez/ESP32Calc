#include "hardware/keymap.h"

namespace esp32calc {
namespace {

constexpr KeyDef key(uint8_t row, uint8_t col, const char* label, KeyRole role) {
  return KeyDef {row, col, label, role, {}};
}

constexpr KeyDef input_key(uint8_t row,
                           uint8_t col,
                           const char* label,
                           const char* normal,
                           const char* shift = nullptr,
                           const char* alpha = nullptr) {
  return KeyDef {row, col, label, KeyRole::Normal, {normal, shift, alpha}};
}

bool has_text(const char* text) {
  return text != nullptr && text[0] != '\0';
}

}  // namespace

const std::array<std::array<KeyDef, kMatrixColCount>, kMatrixRowCount> kKeyList = {{
    {{key(0, 0, "SHIFT", KeyRole::Shift),
      key(0, 1, "ALPHA", KeyRole::Alpha),
      key(0, 2, "UP", KeyRole::Up),
      key(0, 3, "MODE", KeyRole::Mode),
      input_key(0, 4, "INT", "int("),
      key(0, 5, "CALC", KeyRole::Enter)}},
    {{key(1, 0, "LEFT", KeyRole::Left),
      key(1, 1, "DOWN", KeyRole::Down),
      key(1, 2, "RIGHT", KeyRole::Right),
      input_key(1, 3, "dx", "d/dx("),
      input_key(1, 4, "'''", "'"),
      input_key(1, 5, "sqrt", "sqrt(")}},
    {{input_key(2, 0, "xyz", "x", "y", "z"),
      input_key(2, 1, "xyz^2", "x^2", "y^2", "z^2"),
      input_key(2, 2, "xyz^a", "^"),
      input_key(2, 3, "(/)v", "root("),
      input_key(2, 4, "(/)", "/"),
      input_key(2, 5, "log", "log(")}},
    {{input_key(3, 0, "ln", "ln("),
      input_key(3, 1, "logab", "log("),
      key(3, 2, "rcl", KeyRole::Normal),
      key(3, 3, "eng", KeyRole::Normal),
      input_key(3, 4, "()", "(", ")"),
      key(3, 5, "S<>D", KeyRole::Normal)}},
    {{key(4, 0, "M+-", KeyRole::Normal),
      input_key(4, 1, "7", "7"),
      input_key(4, 2, "8", "8"),
      input_key(4, 3, "9", "9"),
      key(4, 4, "DEL", KeyRole::Delete),
      key(4, 5, "AC", KeyRole::Clear)}},
    {{input_key(5, 0, "sen", "sin("),
      input_key(5, 1, "4", "4"),
      input_key(5, 2, "5", "5"),
      input_key(5, 3, "6", "6"),
      input_key(5, 4, "*", "*"),
      input_key(5, 5, "/", "/")}},
    {{input_key(6, 0, "cos", "cos("),
      input_key(6, 1, "1", "1"),
      input_key(6, 2, "2", "2"),
      input_key(6, 3, "3", "3"),
      input_key(6, 4, "+", "+"),
      input_key(6, 5, "-", "-")}},
    {{input_key(7, 0, "tan", "tan("),
      input_key(7, 1, "0", "0"),
      input_key(7, 2, ".", "."),
      input_key(7, 3, "x10^x", "E"),
      input_key(7, 4, "Ans", "Ans"),
      key(7, 5, "=", KeyRole::Enter)}},
    {{key(8, 0, "hyp", KeyRole::Normal),
      key(8, 1, "", KeyRole::Normal),
      key(8, 2, "", KeyRole::Normal),
      key(8, 3, "", KeyRole::Normal),
      key(8, 4, "", KeyRole::Normal),
      key(8, 5, "", KeyRole::Normal)}},
}};

const KeyDef& key_at(uint8_t row, uint8_t col) {
  return kKeyList[row][col];
}

bool is_blank_key(const KeyDef& key) {
  return key.label == nullptr || key.label[0] == '\0';
}

const char* key_input(const KeyDef& key, bool shift, bool alpha) {
  if (alpha && has_text(key.input.alpha)) {
    return key.input.alpha;
  }
  if (shift && has_text(key.input.shift)) {
    return key.input.shift;
  }
  if (has_text(key.input.normal)) {
    return key.input.normal;
  }
  return nullptr;
}

int key_digit(const KeyDef& key) {
  const char* token = key_input(key);
  if (token == nullptr || token[1] != '\0' || token[0] < '0' || token[0] > '9') {
    return -1;
  }
  return token[0] - '0';
}

}  // namespace esp32calc
