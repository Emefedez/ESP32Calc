#include "app_config.h"
#include "app_events.h"
#include "battery/battery_monitor.h"
#include "display/weact_213_bw.h"
#include "hardware/keypad_matrix.h"
#include "math/math_service.h"
#include "ui/menu.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace {

constexpr const char* TAG = "alt_boot";
constexpr uint32_t kUiTaskStackBytes = 24 * 1024;
constexpr UBaseType_t kUiTaskPriority = 6;

QueueHandle_t g_app_events = nullptr;
esp32calc_alt::Weact213BwDisplay g_display;
esp32calc_alt::KeypadMatrix g_keypad;
esp32calc_alt::BatteryMonitor g_battery;
esp32calc_alt::MathService g_math_service;

void log_memory(const char* label) {
  ESP_LOGI(TAG,
           "%s internal=%u internal_largest=%u psram=%u psram_largest=%u",
           label,
           heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
}

void ui_task(void*) {
  static esp32calc_alt::MenuUi ui(g_app_events, g_display, g_math_service);
  ui.run();
}

}  // namespace

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "ESP32Calc NeoCalculator-inspired firmware lab boot");
  log_memory("boot");

  g_app_events =
      xQueueCreate(esp32calc_alt::config::kAppEventQueueDepth, sizeof(esp32calc_alt::AppEvent));
  if (g_app_events == nullptr) {
    ESP_LOGE(TAG, "app event queue allocation failed");
    return;
  }

  esp_err_t err = g_display.init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "display init failed: %s", esp_err_to_name(err));
    return;
  }

  err = g_keypad.init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "keypad init failed: %s", esp_err_to_name(err));
    return;
  }

  err = g_battery.init();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "battery init failed: %s", esp_err_to_name(err));
  }

  err = g_math_service.start();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "math service start failed: %s", esp_err_to_name(err));
    return;
  }

  err = g_keypad.start(g_app_events);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "keypad start failed: %s", esp_err_to_name(err));
    return;
  }

  err = g_battery.start(g_app_events);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "battery start failed: %s", esp_err_to_name(err));
  }

  const BaseType_t ui_ok = xTaskCreatePinnedToCore(&ui_task,
                                                   "alt_ui",
                                                   kUiTaskStackBytes,
                                                   nullptr,
                                                   kUiTaskPriority,
                                                   nullptr,
                                                   esp32calc_alt::config::kUiCore);
  if (ui_ok != pdPASS) {
    ESP_LOGE(TAG, "ui task start failed");
    return;
  }

  ESP_LOGI(TAG, "boot complete");
}
