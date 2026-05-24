#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace esp32calc {

class CalcEngine {
 public:
  esp_err_t start(QueueHandle_t app_events);

 private:
  static void task_trampoline(void* arg);
  void task();

  QueueHandle_t app_events_ = nullptr;
};

}  // namespace esp32calc

