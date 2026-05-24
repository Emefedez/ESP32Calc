#include "calc/calc_engine.h"

#include "app_config.h"
#include "app_log.h"
#include "freertos/task.h"

namespace esp32calc {
namespace {
constexpr const char* TAG = "calc";
}

esp_err_t CalcEngine::start(QueueHandle_t app_events) {
  app_events_ = app_events;
  BaseType_t ok = xTaskCreatePinnedToCore(
      &CalcEngine::task_trampoline,
      "calc",
      6144,
      this,
      5,
      nullptr,
      config::kCoreServicesCore);
  return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void CalcEngine::task_trampoline(void* arg) {
  static_cast<CalcEngine*>(arg)->task();
}

void CalcEngine::task() {
  ESP32CALC_LOGI(TAG, "symbolic/numeric engine placeholder ready");
  while (true) {
    // Future work: consume requests from a calc queue and emit AppEventType::CalcResult.
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

}  // namespace esp32calc

