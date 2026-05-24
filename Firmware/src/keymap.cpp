#include "keymap.h"

namespace esp32calc {

const std::array<std::array<KeyDef, kMatrixColCount>, kMatrixRowCount> kKeyList = {{
    {{{0, 0, "SHIFT", KeyRole::Shift},
      {0, 1, "ALPHA", KeyRole::Alpha},
      {0, 2, "UP", KeyRole::Up},
      {0, 3, "MODE", KeyRole::Mode},
      {0, 4, "INT", KeyRole::Normal},
      {0, 5, "CALC", KeyRole::Enter}}},
    {{{1, 0, "LEFT", KeyRole::Left},
      {1, 1, "DOWN", KeyRole::Down},
      {1, 2, "RIGHT", KeyRole::Right},
      {1, 3, "dx", KeyRole::Normal},
      {1, 4, "'''", KeyRole::Normal},
      {1, 5, "sqrt", KeyRole::Normal}}},
    {{{2, 0, "xyz", KeyRole::Normal},
      {2, 1, "xyz^2", KeyRole::Normal},
      {2, 2, "xyz^a", KeyRole::Normal},
      {2, 3, "(/)v", KeyRole::Normal},
      {2, 4, "(/)", KeyRole::Normal},
      {2, 5, "log", KeyRole::Normal}}},
    {{{3, 0, "ln", KeyRole::Normal},
      {3, 1, "logab", KeyRole::Normal},
      {3, 2, "rcl", KeyRole::Normal},
      {3, 3, "eng", KeyRole::Normal},
      {3, 4, "()", KeyRole::Normal},
      {3, 5, "S<>D", KeyRole::Normal}}},
    {{{4, 0, "M+-", KeyRole::Normal},
      {4, 1, "7", KeyRole::Normal},
      {4, 2, "8", KeyRole::Normal},
      {4, 3, "9", KeyRole::Normal},
      {4, 4, "DEL", KeyRole::Delete},
      {4, 5, "AC", KeyRole::Clear}}},
    {{{5, 0, "sen", KeyRole::Normal},
      {5, 1, "4", KeyRole::Normal},
      {5, 2, "5", KeyRole::Normal},
      {5, 3, "6", KeyRole::Normal},
      {5, 4, "*", KeyRole::Normal},
      {5, 5, "/", KeyRole::Normal}}},
    {{{6, 0, "cos", KeyRole::Normal},
      {6, 1, "1", KeyRole::Normal},
      {6, 2, "2", KeyRole::Normal},
      {6, 3, "3", KeyRole::Normal},
      {6, 4, "+", KeyRole::Normal},
      {6, 5, "-", KeyRole::Normal}}},
    {{{7, 0, "tan", KeyRole::Normal},
      {7, 1, "0", KeyRole::Normal},
      {7, 2, ".", KeyRole::Normal},
      {7, 3, "x10^x", KeyRole::Normal},
      {7, 4, "Ans", KeyRole::Normal},
      {7, 5, "=", KeyRole::Enter}}},
    {{{8, 0, "hyp", KeyRole::Normal},
      {8, 1, "", KeyRole::Normal},
      {8, 2, "", KeyRole::Normal},
      {8, 3, "", KeyRole::Normal},
      {8, 4, "", KeyRole::Normal},
      {8, 5, "", KeyRole::Normal}}},
}};

const KeyDef& key_at(uint8_t row, uint8_t col) {
  return kKeyList[row][col];
}

bool is_blank_key(const KeyDef& key) {
  return key.label == nullptr || key.label[0] == '\0';
}

}  // namespace esp32calc
