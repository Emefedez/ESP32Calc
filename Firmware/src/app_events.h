#pragma once

#include <cstdint>

namespace esp32calc {

enum class AppEventType : uint8_t {
  Key,
  Battery,
  Storage,
  CalcResult,
  Wireless,
};

enum class KeyPhase : uint8_t {
  Pressed,
  Released,
};

struct KeyEvent {
  uint8_t row = 0;
  uint8_t col = 0;
  const char* label = "";
  KeyPhase phase = KeyPhase::Pressed;
};

struct BatterySnapshot {
  uint16_t adc_mv = 0;
  uint16_t pack_mv = 0;
  uint8_t percent = 0;
};

struct AppEvent {
  AppEventType type = AppEventType::Key;
  KeyEvent key {};
  BatterySnapshot battery {};
};

}  // namespace esp32calc

