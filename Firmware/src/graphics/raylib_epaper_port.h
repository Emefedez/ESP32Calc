#pragma once

#include "app_config.h"

#if ESP32CALC_USE_RAYLIB

#include "display/weact_213_bw.h"
#include "esp_err.h"

namespace esp32calc {

class RaylibEpaperPort {
 public:
  esp_err_t init(Weact213BwDisplay& display);
  bool ready() const { return initialized_; }
  void set_next_refresh_mode(RefreshMode mode);

 private:
  bool initialized_ = false;
};

}  // namespace esp32calc

#endif  // ESP32CALC_USE_RAYLIB
