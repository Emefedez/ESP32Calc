#include "wireless/wireless_service.h"

#include "app_config.h"
#include "esp_log.h"
#include "freertos/task.h"

namespace esp32calc {
namespace {
constexpr const char* TAG = "wireless";
}

esp_err_t WirelessService::start(QueueHandle_t app_events) {
  app_events_ = app_events;
  BaseType_t ok = xTaskCreatePinnedToCore(
      &WirelessService::task_trampoline,
      "wireless",
      4096,
      this,
      2,
      nullptr,
      config::kCoreServicesCore);
  return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void WirelessService::task_trampoline(void* arg) {
  static_cast<WirelessService*>(arg)->task();
}

void WirelessService::task() {
  ESP_LOGI(TAG, "wireless reserved, not started yet");
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

}  // namespace esp32calc

