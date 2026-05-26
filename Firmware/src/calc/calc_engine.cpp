#include "calc/calc_engine.h"

#include "app_config.h"
#include "esp_log.h"
#include "freertos/task.h"

#include <cstring>
#include <new>

namespace esp32calc {
namespace {
constexpr const char* TAG = "calc";
constexpr UBaseType_t kCalcQueueDepth = 10;
}  // namespace

esp_err_t CalcEngine::start(QueueHandle_t app_events) {
  app_events_ = app_events;

  if (requests_queue_ == nullptr) {
    requests_queue_ = xQueueCreate(kCalcQueueDepth, sizeof(CalcRequest));
    if (requests_queue_ == nullptr) {
      return ESP_ERR_NO_MEM;
    }
  }

  BaseType_t ok = xTaskCreatePinnedToCore(
      &CalcEngine::task_trampoline,
      "calc",
      6144,
      this,
      5,
      nullptr,
      config::kCoreServicesCore);
  if (ok != pdPASS) {
    vQueueDelete(requests_queue_);
    requests_queue_ = nullptr;
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

bool CalcEngine::submit_expression(const char* expr) {
  if (requests_queue_ == nullptr || expr == nullptr) {
    return false;
  }

  const size_t len = std::strlen(expr) + 1;
  CalcRequest req {};
  req.dynamic_expression = new char[len];
  if (req.dynamic_expression == nullptr) {
    return false;
  }
  std::memcpy(req.dynamic_expression, expr, len);

  if (xQueueSend(requests_queue_, &req, 0) != pdTRUE) {
    delete[] req.dynamic_expression;
    return false;
  }
  return true;
}

void CalcEngine::task_trampoline(void* arg) {
  static_cast<CalcEngine*>(arg)->task();
}

void CalcEngine::task() {
  ESP_LOGI(TAG, "symbolic/numeric engine placeholder ready");
  CalcRequest req {};
  while (true) {
    if (xQueueReceive(requests_queue_, &req, portMAX_DELAY) == pdTRUE) {
      ESP_LOGI(TAG, "Processing: %s", req.dynamic_expression);
      delete[] req.dynamic_expression;
      req.dynamic_expression = nullptr;
    }
  }
}

}  // namespace esp32calc
