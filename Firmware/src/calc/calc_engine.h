#pragma once

#include <cstdint>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace esp32calc {

struct CalcResult;

enum class CalcRequestKind : uint8_t {
  Evaluate,
  Graph,
};

struct CalcRequest {
  CalcRequestKind kind = CalcRequestKind::Evaluate;
  char* dynamic_expression = nullptr;
};

enum class CalcExpressionKind : uint8_t {
  Invalid,
  Numeric,
  Symbolic,
  Equation,
};

class CalcEngine {
 public:
  esp_err_t start(QueueHandle_t app_events);
  bool submit_expression(const char* expr);
  bool submit_graph_expression(const char* expr);
  CalcExpressionKind expression_identifier(const char* expr);

 private:
  static void task_trampoline(void* arg);
  void task();
  bool submit_request(CalcRequestKind kind, const char* expr);
  void publish_result(CalcResult* result);

  QueueHandle_t app_events_ = nullptr;
  QueueHandle_t requests_queue_ = nullptr;
};

}  // namespace esp32calc
