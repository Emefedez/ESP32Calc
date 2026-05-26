#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace esp32calc {

struct CalcRequest {
  char* dynamic_expression = nullptr;
};

class CalcEngine {
 public:
  esp_err_t start(QueueHandle_t app_events);
  bool submit_expression(const char* expr);

 private:
  static void task_trampoline(void* arg);
  void task();

  QueueHandle_t app_events_ = nullptr;
  QueueHandle_t requests_queue_ = nullptr;
};

}  // namespace esp32calc
