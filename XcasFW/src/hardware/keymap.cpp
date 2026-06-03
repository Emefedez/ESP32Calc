#include "hardware/keymap.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <strings.h>

namespace esp32calc_alt {
namespace {

struct KeyOverrideStorage {
  bool active = false;
  KeyDef key {};
  char label[16] {};
  char normal[32] {};
  char shift[32] {};
  char alpha[32] {};
};

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

constexpr KeyDef input_key(uint8_t row,
                           uint8_t col,
                           const char* label,
                           KeyRole role,
                           const char* normal,
                           const char* shift = nullptr,
                           const char* alpha = nullptr) {
  return KeyDef {row, col, label, role, {normal, shift, alpha}};
}

bool has_text(const char* text) {
  return text != nullptr && text[0] != '\0';
}

void copy_text(char* output, size_t output_size, const char* input) {
  if (output == nullptr || output_size == 0) {
    return;
  }
  if (input == nullptr) {
    output[0] = '\0';
    return;
  }
  std::snprintf(output, output_size, "%s", input);
}

bool valid_position(uint8_t row, uint8_t col) {
  return row < kMatrixRowCount && col < kMatrixColCount;
}

KeyOverrideStorage g_overrides[kMatrixRowCount][kMatrixColCount] {};

KeyOverrideStorage& ensure_override(uint8_t row, uint8_t col) {
  KeyOverrideStorage& storage = g_overrides[row][col];
  if (!storage.active) {
    storage.key = kKeyList[row][col];
    storage.active = true;
  }
  return storage;
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
    {{input_key(2, 0, "xyz", KeyRole::Variable, "x", "y", "z"),
      input_key(2, 1, "xyz^2", KeyRole::VariableSquare, "x^2", "y^2", "z^2"),
      input_key(2, 2, "xyz^a", "^"),
      input_key(2, 3, "(/)v", "root("),
      input_key(2, 4, "(/)", "/"),
      input_key(2, 5, "log", "log(")}},
    {{input_key(3, 0, "ln", "ln("),
      input_key(3, 1, "logab", "log("),
      key(3, 2, "rcl", KeyRole::Normal),
      key(3, 3, "eng", KeyRole::Normal),
      input_key(3, 4, "()", "(", ")"),
      key(3, 5, "S<>D", KeyRole::FractionToggle)}},
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
      input_key(5, 5, "/", "/", nullptr, ",")}},
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
  if (valid_position(row, col) && g_overrides[row][col].active) {
    return g_overrides[row][col].key;
  }
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

KeyRole key_role_from_name(const char* name, KeyRole fallback) {
  if (!has_text(name)) {
    return fallback;
  }
  if (strcasecmp(name, "normal") == 0) {
    return KeyRole::Normal;
  }
  if (strcasecmp(name, "shift") == 0) {
    return KeyRole::Shift;
  }
  if (strcasecmp(name, "alpha") == 0) {
    return KeyRole::Alpha;
  }
  if (strcasecmp(name, "mode") == 0) {
    return KeyRole::Mode;
  }
  if (strcasecmp(name, "up") == 0) {
    return KeyRole::Up;
  }
  if (strcasecmp(name, "down") == 0) {
    return KeyRole::Down;
  }
  if (strcasecmp(name, "left") == 0) {
    return KeyRole::Left;
  }
  if (strcasecmp(name, "right") == 0) {
    return KeyRole::Right;
  }
  if (strcasecmp(name, "enter") == 0) {
    return KeyRole::Enter;
  }
  if (strcasecmp(name, "delete") == 0) {
    return KeyRole::Delete;
  }
  if (strcasecmp(name, "clear") == 0) {
    return KeyRole::Clear;
  }
  if (strcasecmp(name, "variable") == 0) {
    return KeyRole::Variable;
  }
  if (strcasecmp(name, "variablesquare") == 0 || strcasecmp(name, "variable_square") == 0) {
    return KeyRole::VariableSquare;
  }
  if (strcasecmp(name, "fractiontoggle") == 0 || strcasecmp(name, "fraction_toggle") == 0) {
    return KeyRole::FractionToggle;
  }
  return fallback;
}

void clear_key_overrides() {
  for (auto& row : g_overrides) {
    for (auto& item : row) {
      item = KeyOverrideStorage {};
    }
  }
}

bool set_key_override(uint8_t row,
                      uint8_t col,
                      const char* label,
                      KeyRole role,
                      const char* normal,
                      const char* shift,
                      const char* alpha) {
  if (!valid_position(row, col)) {
    return false;
  }

  KeyOverrideStorage& storage = ensure_override(row, col);
  storage.key.role = role;
  if (label != nullptr) {
    copy_text(storage.label, sizeof(storage.label), label);
    storage.key.label = storage.label;
  }
  if (normal != nullptr) {
    copy_text(storage.normal, sizeof(storage.normal), normal);
    storage.key.input.normal = storage.normal;
  }
  if (shift != nullptr) {
    copy_text(storage.shift, sizeof(storage.shift), shift);
    storage.key.input.shift = storage.shift;
  }
  if (alpha != nullptr) {
    copy_text(storage.alpha, sizeof(storage.alpha), alpha);
    storage.key.input.alpha = storage.alpha;
  }
  return true;
}

bool set_key_override_field(uint8_t row, uint8_t col, const char* field, const char* value) {
  if (!valid_position(row, col) || !has_text(field)) {
    return false;
  }

  KeyOverrideStorage& storage = ensure_override(row, col);
  if (strcasecmp(field, "label") == 0) {
    copy_text(storage.label, sizeof(storage.label), value);
    storage.key.label = storage.label;
    return true;
  }
  if (strcasecmp(field, "normal") == 0) {
    copy_text(storage.normal, sizeof(storage.normal), value);
    storage.key.input.normal = storage.normal;
    return true;
  }
  if (strcasecmp(field, "shift") == 0) {
    copy_text(storage.shift, sizeof(storage.shift), value);
    storage.key.input.shift = storage.shift;
    return true;
  }
  if (strcasecmp(field, "alpha") == 0) {
    copy_text(storage.alpha, sizeof(storage.alpha), value);
    storage.key.input.alpha = storage.alpha;
    return true;
  }
  if (strcasecmp(field, "role") == 0) {
    storage.key.role = key_role_from_name(value, storage.key.role);
    return true;
  }
  return false;
}

}  // namespace esp32calc_alt
