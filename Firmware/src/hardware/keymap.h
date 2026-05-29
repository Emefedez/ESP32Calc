#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace esp32calc {

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

}  // namespace esp32calc
