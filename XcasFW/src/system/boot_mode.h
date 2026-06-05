#pragma once

#include <cstdint>

namespace esp32calc_alt::boot {

enum class Mode : uint8_t {
  Calculator,
  MicroPython,
};

Mode current_mode();
bool set_mode(Mode mode, const char* app_id = nullptr);
const char* pending_app_id();

}  // namespace esp32calc_alt::boot
