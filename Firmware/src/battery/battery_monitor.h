#pragma once

#include "app_events.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace esp32calc {

class BatteryMonitor {
 public:
  esp_err_t init();
  esp_err_t start(QueueHandle_t app_events);
  BatterySnapshot latest() const { return latest_; }

 private:
  static void task_trampoline(void* arg);
  void task();
  BatterySnapshot sample();

  QueueHandle_t app_events_ = nullptr;
  BatterySnapshot latest_ {};
  void* adc_handle_ = nullptr;
};

}  // namespace esp32calc

