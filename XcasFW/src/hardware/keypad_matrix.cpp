#include "hardware/keypad_matrix.h"

#include "app_config.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/task.h"
#include "hardware/keymap.h"
#include "hardware/pins.h"

namespace esp32calc_alt {
namespace {
constexpr const char* TAG = "keypad";

uint64_t pin_mask(const gpio_num_t* pins, size_t count) {
  uint64_t mask = 0;
  for (size_t i = 0; i < count; ++i) {
    mask |= (1ULL << static_cast<uint8_t>(pins[i]));
  }
  return mask;
}
}  // namespace

esp_err_t KeypadMatrix::init() {
  gpio_config_t row_cfg {};
  row_cfg.intr_type = GPIO_INTR_DISABLE;
  row_cfg.mode = GPIO_MODE_INPUT;
  row_cfg.pin_bit_mask = pin_mask(pins::kMatrixRows.data(), pins::kMatrixRows.size());
  row_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  row_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
  ESP_RETURN_ON_ERROR(gpio_config(&row_cfg), TAG, "configure rows");

  gpio_config_t col_cfg {};
  col_cfg.intr_type = GPIO_INTR_DISABLE;
  col_cfg.mode = GPIO_MODE_OUTPUT_OD;
  col_cfg.pin_bit_mask = pin_mask(pins::kMatrixCols.data(), pins::kMatrixCols.size());
  col_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  col_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
  ESP_RETURN_ON_ERROR(gpio_config(&col_cfg), TAG, "configure columns");

  for (auto col : pins::kMatrixCols) {
    gpio_set_level(col, 1);
  }

  ESP_LOGI(TAG, "matrix ready: %u rows x %u cols", kRows, kCols);
  return ESP_OK;
}

esp_err_t KeypadMatrix::start(QueueHandle_t app_events) {
  app_events_ = app_events;
  BaseType_t ok = xTaskCreatePinnedToCore(
      &KeypadMatrix::task_trampoline,
      "keypad",
      4096,
      this,
      8,
      nullptr,
      config::kUiCore);
  return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void KeypadMatrix::task_trampoline(void* arg) {
  static_cast<KeypadMatrix*>(arg)->task();
}

void KeypadMatrix::task() {
  bool raw[kRows][kCols] {};
  while (true) {
    scan_raw(raw);

    for (uint8_t row = 0; row < kRows; ++row) {
      for (uint8_t col = 0; col < kCols; ++col) {
        if (raw[row][col] == last_raw_[row][col]) {
          if (counters_[row][col] < config::kInputDebounceSamples) {
            ++counters_[row][col];
          }
        } else {
          counters_[row][col] = 0;
          last_raw_[row][col] = raw[row][col];
        }

        if (counters_[row][col] >= config::kInputDebounceSamples &&
            raw[row][col] != stable_[row][col]) {
          stable_[row][col] = raw[row][col];
          publish_key(row, col, raw[row][col] ? KeyPhase::Pressed : KeyPhase::Released);
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(config::kInputScanPeriodMs));
  }
}

void KeypadMatrix::scan_raw(bool (&pressed)[KeypadMatrix::kRows][KeypadMatrix::kCols]) {
  for (uint8_t row = 0; row < kRows; ++row) {
    for (uint8_t col = 0; col < kCols; ++col) {
      pressed[row][col] = false;
    }
  }

  for (uint8_t col = 0; col < kCols; ++col) {
    for (auto pin : pins::kMatrixCols) {
      gpio_set_level(pin, 1);
    }
    gpio_set_level(pins::kMatrixCols[col], 0);
    esp_rom_delay_us(80);

    for (uint8_t row = 0; row < kRows; ++row) {
      pressed[row][col] = gpio_get_level(pins::kMatrixRows[row]) == 0;
    }
  }

  for (auto pin : pins::kMatrixCols) {
    gpio_set_level(pin, 1);
  }
}

void KeypadMatrix::publish_key(uint8_t row, uint8_t col, KeyPhase phase) {
  const KeyDef& key = key_at(row, col);
  if (is_blank_key(key)) {
    return;
  }

  AppEvent event {};
  event.type = AppEventType::Key;
  event.key = KeyEvent {row, col, key.label, phase};

  if (phase == KeyPhase::Pressed) {
    ESP_LOGI(TAG, "press r%u c%u %s", row, col, key.label);
  }

  if (app_events_ != nullptr) {
    xQueueSend(app_events_, &event, 0);
  }
}

}  // namespace esp32calc_alt
