#pragma once

#include <cstdint>

namespace esp32calc_alt {

struct KeyEvent {
  uint8_t row;
  uint8_t col;
  bool pressed : 1;
  bool shift : 1;
  bool alpha : 1;
};

struct BatterySnapshot {
  uint8_t bars;
};

struct AppEvent {
  bool is_key : 1;
  union {
    KeyEvent key;
    BatterySnapshot battery;
  };
};

static_assert(sizeof(BatterySnapshot) == 1);

}  // namespace esp32calc_alt
