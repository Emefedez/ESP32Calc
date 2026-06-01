#pragma once

#include <array>

#include "app_events.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"

namespace esp32calc_alt {

class KeypadMatrix {
 public:
  static constexpr uint8_t kRows = 9;
  static constexpr uint8_t kCols = 6;

  esp_err_t init();
  esp_err_t start(QueueHandle_t app_events);

 private:
  static void task_trampoline(void* arg);
  void task();
  void scan_raw(bool (&pressed)[kRows][kCols]);
  void publish_key(uint8_t row, uint8_t col, KeyPhase phase);

  QueueHandle_t app_events_ = nullptr;
  bool stable_[kRows][kCols] {};
  bool last_raw_[kRows][kCols] {};
  uint8_t counters_[kRows][kCols] {};
};

}  // namespace esp32calc_alt
