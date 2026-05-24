#include "battery/battery_monitor.h"

#include <algorithm>
#include <cmath>

#include "app_config.h"
#include "app_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "freertos/task.h"
#include "pins.h"

namespace esp32calc {
namespace {
constexpr const char* TAG = "battery";
constexpr adc_channel_t kBatteryAdcChannel = ADC_CHANNEL_2;  // GPIO3 on ESP32-S3 ADC1.
}  // namespace

esp_err_t BatteryMonitor::init() {
  adc_oneshot_unit_init_cfg_t unit_cfg {};
  unit_cfg.unit_id = ADC_UNIT_1;
  adc_oneshot_unit_handle_t handle = nullptr;
  ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_cfg, &handle), TAG, "create adc unit");

  adc_oneshot_chan_cfg_t chan_cfg {};
  chan_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;
  chan_cfg.atten = ADC_ATTEN_DB_12;
  ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(handle, kBatteryAdcChannel, &chan_cfg),
                      TAG,
                      "configure battery adc");

  adc_handle_ = handle;
  latest_ = sample();
  ESP32CALC_LOGI(TAG, "adc ready on GPIO%d, pack=%umV", pins::kBatterySense, latest_.pack_mv);
  return ESP_OK;
}

esp_err_t BatteryMonitor::start(QueueHandle_t app_events) {
  app_events_ = app_events;
  BaseType_t ok = xTaskCreatePinnedToCore(
      &BatteryMonitor::task_trampoline,
      "battery",
      3072,
      this,
      3,
      nullptr,
      config::kCoreServicesCore);
  return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void BatteryMonitor::task_trampoline(void* arg) {
  static_cast<BatteryMonitor*>(arg)->task();
}

void BatteryMonitor::task() {
  while (true) {
    latest_ = sample();

    AppEvent event {};
    event.type = AppEventType::Battery;
    event.battery = latest_;
    if (app_events_ != nullptr) {
      xQueueSend(app_events_, &event, 0);
    }

    ESP32CALC_LOGD(TAG,
                   "adc=%umV pack=%umV charge=%u%%",
                   latest_.adc_mv,
                   latest_.pack_mv,
                   latest_.percent);
    vTaskDelay(pdMS_TO_TICKS(config::kBatteryPollPeriodMs));
  }
}

BatterySnapshot BatteryMonitor::sample() {
  auto* handle = static_cast<adc_oneshot_unit_handle_t>(adc_handle_);
  int raw = 0;
  if (handle != nullptr) {
    esp_err_t err = adc_oneshot_read(handle, kBatteryAdcChannel, &raw);
    if (err != ESP_OK) {
      ESP32CALC_LOGW(TAG, "adc read failed: %s", esp_err_to_name(err));
    }
  }

  const uint16_t adc_mv = static_cast<uint16_t>((raw * 3300) / 4095);
  const uint16_t pack_mv = static_cast<uint16_t>(
      std::lround(static_cast<float>(adc_mv) * config::kBatteryDividerRatio));
  const int percent = ((static_cast<int>(pack_mv) - config::kBatteryEmptyMv) * 100) /
                      (config::kBatteryFullMv - config::kBatteryEmptyMv);

  BatterySnapshot snapshot {};
  snapshot.adc_mv = adc_mv;
  snapshot.pack_mv = pack_mv;
  snapshot.percent = static_cast<uint8_t>(std::clamp(percent, 0, 100));
  return snapshot;
}

}  // namespace esp32calc
