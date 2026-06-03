#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace esp32calc_alt {

enum class KeyRole : uint8_t {
  Normal,
  Shift,
  Alpha,
  Mode,
  Up,
  Down,
  Left,
  Right,
  Enter,
  Delete,
  Clear,
  Variable,
  VariableSquare,
  FractionToggle,
};

struct KeyInputTokens {
  const char* normal = nullptr;
  const char* shift = nullptr;
  const char* alpha = nullptr;
};

struct KeyDef {
  uint8_t row;
  uint8_t col;
  const char* label;
  KeyRole role;
  KeyInputTokens input {};
};

constexpr size_t kMatrixRowCount = 9;
constexpr size_t kMatrixColCount = 6;
extern const std::array<std::array<KeyDef, kMatrixColCount>, kMatrixRowCount> kKeyList;

const KeyDef& key_at(uint8_t row, uint8_t col);
bool is_blank_key(const KeyDef& key);
const char* key_input(const KeyDef& key, bool shift = false, bool alpha = false);
int key_digit(const KeyDef& key);
KeyRole key_role_from_name(const char* name, KeyRole fallback = KeyRole::Normal);
void clear_key_overrides();
bool set_key_override(uint8_t row,
                      uint8_t col,
                      const char* label,
                      KeyRole role,
                      const char* normal,
                      const char* shift,
                      const char* alpha);
bool set_key_override_field(uint8_t row, uint8_t col, const char* field, const char* value);

}  // namespace esp32calc_alt
