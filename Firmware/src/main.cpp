#include "app_config.h"
#include "battery/battery_monitor.h"
#include "calc/calc_engine.h"
#include "display/weact_213_bw.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "graphics/mono_canvas.h"
#include "keypad_matrix.h"
#include "nvs_flash.h"
#include "storage/sd_storage.h"
#include "ui/menu.h"
#include "wireless/wireless_service.h"

#include <algorithm>
#include <array>

static const char* TAG = "boot";

QueueHandle_t g_app_events = nullptr;
esp32calc::Weact213BwDisplay g_display;
esp32calc::KeypadMatrix g_keypad;
esp32calc::BatteryMonitor g_battery;
esp32calc::SdStorage g_sd;
esp32calc::CalcEngine g_calc;
esp32calc::WirelessService g_wireless;
esp32calc::MonoCanvas g_boot_canvas;

#if ESP32CALC_USE_RAYLIB
constexpr uint32_t kUiTaskStackBytes = 128 * 1024;
#else
constexpr uint32_t kUiTaskStackBytes = 8192;
#endif

void ui_task(void*) {
  esp32calc::MenuUi ui(g_app_events, g_display);
  ui.run();
}

static void log_startup_memory() {
  ESP_LOGI(TAG, "heap internal=%u psram=%u",
           heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

static void log_if_error(const char* action, esp_err_t err) {
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "%s failed: %s (0x%x)", action, esp_err_to_name(err), err);
  }
}

static void render_native_display_test() {
#if ESP32CALC_BOOT_NATIVE_TEST
  if (!g_display.ready()) {
    return;
  }

  using NativeBuf = std::array<uint8_t, esp32calc::config::kDisplayNativeBufferSize>;
  NativeBuf& native_test = *new NativeBuf();
  native_test.fill(0xFF);
  constexpr uint16_t bytes_per_row = esp32calc::config::kDisplayNativeWidth / 8;
  for (uint16_t y = 0; y < esp32calc::config::kDisplayNativeHeight; ++y) {
    uint8_t* row = &native_test[static_cast<size_t>(y) * bytes_per_row];

    row[0] = 0x00;
    row[bytes_per_row - 1] = 0x00;

    if (y < 18 || y >= esp32calc::config::kDisplayNativeHeight - 18 ||
        (y >= 116 && y < 134)) {
      std::fill(row, row + bytes_per_row, 0x00);
    }

    if ((y / 25) % 2 == 0) {
      row[3] = 0x00;
      row[7] = 0x00;
      row[11] = 0x00;
    }
  }

  ESP_LOGI(TAG, "rendering raw native EPD diagnostic");
  log_if_error("native display test",
               g_display.update_native_buffer(native_test, esp32calc::RefreshMode::Full));
  vTaskDelay(pdMS_TO_TICKS(1600));
#endif
}

static void render_display_self_test() {
#if ESP32CALC_BOOT_DISPLAY_TEST
  if (!g_display.ready()) {
    return;
  }

  g_boot_canvas.clear(true);
  g_boot_canvas.rect(0, 0, esp32calc::MonoCanvas::kWidth, esp32calc::MonoCanvas::kHeight, true);
  g_boot_canvas.hline(0, esp32calc::MonoCanvas::kHeight / 2, esp32calc::MonoCanvas::kWidth, true);
  g_boot_canvas.vline(esp32calc::MonoCanvas::kWidth / 2, 0, esp32calc::MonoCanvas::kHeight, true);

  g_boot_canvas.fill_rect(0, 0, 18, 18, true);
  g_boot_canvas.draw_text(3, 5, "TL", 1, false);
  g_boot_canvas.fill_rect(esp32calc::MonoCanvas::kWidth - 18, 0, 18, 18, true);
  g_boot_canvas.draw_text(esp32calc::MonoCanvas::kWidth - 15, 5, "TR", 1, false);
  g_boot_canvas.fill_rect(0, esp32calc::MonoCanvas::kHeight - 18, 18, 18, true);
  g_boot_canvas.draw_text(3, esp32calc::MonoCanvas::kHeight - 13, "BL", 1, false);
  g_boot_canvas.fill_rect(esp32calc::MonoCanvas::kWidth - 18,
                          esp32calc::MonoCanvas::kHeight - 18,
                          18,
                          18,
                          true);
  g_boot_canvas.draw_text(esp32calc::MonoCanvas::kWidth - 15,
                          esp32calc::MonoCanvas::kHeight - 13,
                          "BR",
                          1,
                          false);

  for (int x = 0; x < esp32calc::MonoCanvas::kWidth; ++x) {
    const int y = x * esp32calc::MonoCanvas::kHeight / esp32calc::MonoCanvas::kWidth;
    g_boot_canvas.set_pixel(x, y, true);
    g_boot_canvas.set_pixel(esp32calc::MonoCanvas::kWidth - 1 - x, y, true);
  }

  g_boot_canvas.draw_text(35, 18, "EPD SELF TEST", 1, true);
  g_boot_canvas.draw_text(35, 34, "FULL REFRESH ONLY", 1, true);
  g_boot_canvas.draw_text(35, 50, "ROT90 250X122", 1, true);
  g_boot_canvas.draw_text(35, 74, "MODE MENU NEXT", 1, true);

  ESP_LOGI(TAG, "rendering boot display self-test");
  log_if_error("display self-test",
               g_display.update_canvas(g_boot_canvas, esp32calc::RefreshMode::Full));
  vTaskDelay(pdMS_TO_TICKS(1200));
#endif
}
static void render_boot_splash() {
  if (!g_display.ready()) {
    return;
  }

  g_boot_canvas.clear(true);

  g_boot_canvas.rect(0, 0, esp32calc::MonoCanvas::kWidth, esp32calc::MonoCanvas::kHeight, true);
  g_boot_canvas.rect(2, 2, esp32calc::MonoCanvas::kWidth - 4, esp32calc::MonoCanvas::kHeight - 4, true);

  g_boot_canvas.fill_rect(5, 5, 6, 2, true);
  g_boot_canvas.fill_rect(5, 5, 2, 6, true);
  g_boot_canvas.fill_rect(esp32calc::MonoCanvas::kWidth - 11, 5, 6, 2, true);
  g_boot_canvas.fill_rect(esp32calc::MonoCanvas::kWidth - 7, 5, 2, 6, true);
  g_boot_canvas.fill_rect(5, esp32calc::MonoCanvas::kHeight - 7, 6, 2, true);
  g_boot_canvas.fill_rect(5, esp32calc::MonoCanvas::kHeight - 11, 2, 6, true);
  g_boot_canvas.fill_rect(esp32calc::MonoCanvas::kWidth - 11, esp32calc::MonoCanvas::kHeight - 7, 6, 2, true);
  g_boot_canvas.fill_rect(esp32calc::MonoCanvas::kWidth - 7, esp32calc::MonoCanvas::kHeight - 11, 2, 6, true);

  g_boot_canvas.draw_text(44, 22, "ESP32Calc", 3, true);

  g_boot_canvas.hline(44, 47, 162, true);
  g_boot_canvas.hline(44, 48, 162, true);

  g_boot_canvas.draw_text(68, 58, "Scientific Calculator", 1, true);

  g_boot_canvas.draw_text(10, 110, "v0.1.0", 1, true);
  g_boot_canvas.draw_text(180, 110, "Booting...", 1, true);

  ESP_LOGI(TAG, "rendering boot splash");
  log_if_error("boot splash",
               g_display.update_canvas(g_boot_canvas, esp32calc::RefreshMode::Full));
}

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "ESP32Calc firmware boot");
  log_startup_memory();

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs init failed: %s (0x%x)", esp_err_to_name(err), err);
  }

  g_app_events = xQueueCreate(esp32calc::config::kAppEventQueueDepth, sizeof(esp32calc::AppEvent));
  if (g_app_events == nullptr) {
    ESP_LOGE(TAG, "failed to create app event queue");
    return;
  }

  err = g_display.init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "display init failed: %s (0x%x)", esp_err_to_name(err), err);
  }
  render_boot_splash();
  render_native_display_test();
  render_display_self_test();

  err = g_keypad.init();
  if (err == ESP_OK) {
    log_if_error("keypad start", g_keypad.start(g_app_events));
  } else {
    ESP_LOGE(TAG, "keypad init failed: %s (0x%x)", esp_err_to_name(err), err);
  }

  err = g_battery.init();
  if (err == ESP_OK) {
    log_if_error("battery start", g_battery.start(g_app_events));
  } else {
    ESP_LOGE(TAG, "battery init failed: %s (0x%x)", esp_err_to_name(err), err);
  }

  g_sd.mount();
  log_if_error("calc start", g_calc.start(g_app_events));
  log_if_error("wireless start", g_wireless.start(g_app_events));

  BaseType_t ui_ok = xTaskCreatePinnedToCore(
      &ui_task,
      "ui",
      kUiTaskStackBytes,
      nullptr,
      6,
      nullptr,
      esp32calc::config::kUiCore);
  if (ui_ok != pdPASS) {
    ESP_LOGE(TAG, "failed to start ui task");
  }

  ESP_LOGI(TAG, "boot complete");
}
