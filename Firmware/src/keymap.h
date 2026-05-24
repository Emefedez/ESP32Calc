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
};

struct KeyDef {
  uint8_t row;
  uint8_t col;
  const char* label;
  KeyRole role;
};

constexpr size_t kMatrixRowCount = 9;
constexpr size_t kMatrixColCount = 6;
extern const std::array<std::array<KeyDef, kMatrixColCount>, kMatrixRowCount> kKeyList;

const KeyDef& key_at(uint8_t row, uint8_t col);
bool is_blank_key(const KeyDef& key);

}  // namespace esp32calc

