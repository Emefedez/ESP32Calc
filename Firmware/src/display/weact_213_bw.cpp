#include "display/weact_213_bw.h"

#include <algorithm>
#include <cstring>

#include "app_log.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pins.h"

namespace esp32calc {
namespace {
constexpr const char* TAG = "epd";

// Command sequence and partial-refresh LUT adapted from avsaase/weact-studio-epd
// v0.1.2, licensed MIT OR Apache-2.0.
namespace cmd {
constexpr uint8_t kDriverControl = 0x01;
constexpr uint8_t kDeepSleep = 0x10;
constexpr uint8_t kDataEntryMode = 0x11;
constexpr uint8_t kSwReset = 0x12;
constexpr uint8_t kTempControl = 0x18;
constexpr uint8_t kMasterActivate = 0x20;
constexpr uint8_t kDisplayUpdateControl = 0x21;
constexpr uint8_t kUpdateDisplayCtrl2 = 0x22;
constexpr uint8_t kWriteBwData = 0x24;
constexpr uint8_t kWriteRedData = 0x26;
constexpr uint8_t kWriteLut = 0x32;
constexpr uint8_t kBorderWaveformControl = 0x3C;
constexpr uint8_t kSetRamXPos = 0x44;
constexpr uint8_t kSetRamYPos = 0x45;
constexpr uint8_t kSetRamXCounter = 0x4E;
constexpr uint8_t kSetRamYCounter = 0x4F;
}  // namespace cmd

namespace flag {
constexpr uint8_t kDeepSleepMode1 = 0x01;
constexpr uint8_t kDataEntryIncrYIncrX = 0x03;
constexpr uint8_t kInternalTempSensor = 0x80;
constexpr uint8_t kBorderWaveformFollowLut = 0x04;
constexpr uint8_t kBorderWaveformLut1 = 0x01;
constexpr uint8_t kDisplayMode1 = 0xF7;
constexpr uint8_t kFastRefreshMagic = 0xCC;
}  // namespace flag

constexpr uint16_t kNativeWidth = config::kDisplayNativeWidth;
constexpr uint16_t kNativeHeight = config::kDisplayNativeHeight;
constexpr size_t kNativeBufferSize = config::kDisplayNativeBufferSize;

constexpr uint8_t kPartialUpdateLut[153] = {
    0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
    0x00, 0x00, 0x00,
};

}  // namespace

esp_err_t Weact213BwDisplay::init() {
  gpio_config_t power_cfg {};
  power_cfg.intr_type = GPIO_INTR_DISABLE;
  power_cfg.mode = GPIO_MODE_OUTPUT;
  power_cfg.pin_bit_mask = 1ULL << pins::kDisplayPower;
  ESP_RETURN_ON_ERROR(gpio_config(&power_cfg), TAG, "configure power");
  gpio_set_drive_capability(pins::kDisplayPower, GPIO_DRIVE_CAP_3);
  gpio_set_level(pins::kDisplayPower, 1);
  vTaskDelay(pdMS_TO_TICKS(ESP32CALC_EPD_POWER_SETTLE_MS));

  gpio_config_t ctrl_cfg {};
  ctrl_cfg.intr_type = GPIO_INTR_DISABLE;
  ctrl_cfg.mode = GPIO_MODE_OUTPUT;
  ctrl_cfg.pin_bit_mask =
      (1ULL << pins::kDisplayRst) | (1ULL << pins::kDisplayDc) | (1ULL << pins::kDisplayCs);
  ESP_RETURN_ON_ERROR(gpio_config(&ctrl_cfg), TAG, "configure control pins");
  gpio_set_level(pins::kDisplayCs, 1);

  gpio_config_t busy_cfg {};
  busy_cfg.intr_type = GPIO_INTR_DISABLE;
  busy_cfg.mode = GPIO_MODE_INPUT;
  busy_cfg.pin_bit_mask = 1ULL << pins::kDisplayBusy;
  busy_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
  ESP_RETURN_ON_ERROR(gpio_config(&busy_cfg), TAG, "configure busy pin");

  spi_bus_config_t bus_cfg {};
  bus_cfg.mosi_io_num = pins::kDisplayMosi;
  bus_cfg.miso_io_num = -1;
  bus_cfg.sclk_io_num = pins::kDisplaySclk;
  bus_cfg.quadwp_io_num = -1;
  bus_cfg.quadhd_io_num = -1;
  bus_cfg.max_transfer_sz = 4096;
  esp_err_t err = spi_bus_initialize(pins::kDisplaySpiHost, &bus_cfg, SPI_DMA_CH_AUTO);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return err;
  }

  spi_device_interface_config_t dev_cfg {};
  dev_cfg.clock_speed_hz = ESP32CALC_EPD_SPI_HZ;
  dev_cfg.mode = 0;
  dev_cfg.spics_io_num = pins::kDisplayCs;
  dev_cfg.queue_size = 4;
  ESP_RETURN_ON_ERROR(spi_bus_add_device(pins::kDisplaySpiHost, &dev_cfg, &spi_),
                      TAG,
                      "add display spi device");

  ESP_RETURN_ON_ERROR(hw_reset(), TAG, "reset display");
  ESP_RETURN_ON_ERROR(command(cmd::kSwReset), TAG, "software reset");
  vTaskDelay(pdMS_TO_TICKS(10));
  ESP_RETURN_ON_ERROR(wait_until_idle(), TAG, "wait after software reset");

  const uint8_t driver_control[] = {
      static_cast<uint8_t>(kNativeHeight - 1),
      static_cast<uint8_t>((kNativeHeight - 1) >> 8),
      0x00,
  };
  ESP_RETURN_ON_ERROR(command_with_data(cmd::kDriverControl,
                                        driver_control,
                                        sizeof(driver_control)),
                      TAG,
                      "driver control");

  const uint8_t data_entry[] = {flag::kDataEntryIncrYIncrX};
  ESP_RETURN_ON_ERROR(command_with_data(cmd::kDataEntryMode, data_entry, sizeof(data_entry)),
                      TAG,
                      "data entry");

  const uint8_t border[] = {flag::kBorderWaveformFollowLut | flag::kBorderWaveformLut1};
  ESP_RETURN_ON_ERROR(command_with_data(cmd::kBorderWaveformControl, border, sizeof(border)),
                      TAG,
                      "border waveform");

  const uint8_t update_control[] = {0x00, 0x80};
  ESP_RETURN_ON_ERROR(command_with_data(cmd::kDisplayUpdateControl,
                                        update_control,
                                        sizeof(update_control)),
                      TAG,
                      "display update control");

  const uint8_t temp[] = {flag::kInternalTempSensor};
  ESP_RETURN_ON_ERROR(command_with_data(cmd::kTempControl, temp, sizeof(temp)),
                      TAG,
                      "temperature control");

  ESP_RETURN_ON_ERROR(use_full_frame(), TAG, "select full frame");
  ESP_RETURN_ON_ERROR(wait_until_idle(), TAG, "wait after init");

  ready_ = true;
  previous_.fill(0xFF);
  previous_canvas_.clear(true);
  ESP32CALC_LOGI(TAG,
                 "WeAct 2.13 B/W display ready (%ux%u logical, %ux%u visible, spi=%uHz)",
                 config::kDisplayLogicalWidth,
                 config::kDisplayLogicalHeight,
                 config::kDisplayLogicalWidth,
                 config::kDisplayVisibleLogicalHeight,
                 ESP32CALC_EPD_SPI_HZ);
  return ESP_OK;
}

esp_err_t Weact213BwDisplay::hw_reset() {
  gpio_set_level(pins::kDisplayRst, 0);
  vTaskDelay(pdMS_TO_TICKS(50));
  gpio_set_level(pins::kDisplayRst, 1);
  vTaskDelay(pdMS_TO_TICKS(50));
  return ESP_OK;
}

esp_err_t Weact213BwDisplay::wait_until_idle(uint32_t timeout_ms) {
  const TickType_t start = xTaskGetTickCount();
  const TickType_t timeout = pdMS_TO_TICKS(timeout_ms);
  while (gpio_get_level(pins::kDisplayBusy) == 1) {
    if (xTaskGetTickCount() - start > timeout) {
      ESP32CALC_LOGW(TAG,
                     "busy timeout after %ums, busy=%d",
                     timeout_ms,
                     gpio_get_level(pins::kDisplayBusy));
      return ESP_ERR_TIMEOUT;
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
  return ESP_OK;
}

esp_err_t Weact213BwDisplay::command(uint8_t value) {
  gpio_set_level(pins::kDisplayDc, 0);
  spi_transaction_t tx {};
  tx.length = 8;
  tx.tx_buffer = &value;
  return spi_device_polling_transmit(spi_, &tx);
}

esp_err_t Weact213BwDisplay::data(const uint8_t* bytes, size_t len, bool wait) {
  if (bytes == nullptr || len == 0) {
    return ESP_OK;
  }

  int64_t t0 = esp_timer_get_time();
  gpio_set_level(pins::kDisplayDc, 1);
  while (len > 0) {
    const size_t chunk = std::min<size_t>(len, 4096);
    spi_transaction_t tx {};
    tx.length = chunk * 8;
    tx.tx_buffer = bytes;
    ESP_RETURN_ON_ERROR(spi_device_polling_transmit(spi_, &tx), TAG, "spi data");
    bytes += chunk;
    len -= chunk;
  }
  int64_t t1 = esp_timer_get_time();

  if (wait) {
    esp_err_t err = wait_until_idle();
    int64_t t2 = esp_timer_get_time();
    ESP32CALC_LOGI(TAG, "data: spi_tx=%lldus busy_wait=%lldus total=%lldus",
                   t1 - t0, t2 - t1, t2 - t0);
    return err;
  }
  ESP32CALC_LOGD(TAG, "data: spi_tx=%lldus (no wait)", t1 - t0);
  return ESP_OK;
}

esp_err_t Weact213BwDisplay::command_with_data(uint8_t command_value,
                                               const uint8_t* bytes,
                                               size_t len) {
  ESP_RETURN_ON_ERROR(command(command_value), TAG, "command");
  return data(bytes, len, true);
}

esp_err_t Weact213BwDisplay::data_repeat(uint8_t value, uint32_t repetitions) {
  std::array<uint8_t, 128> chunk {};
  std::fill(chunk.begin(), chunk.end(), value);
  gpio_set_level(pins::kDisplayDc, 1);

  while (repetitions > 0) {
    const uint32_t count = std::min<uint32_t>(repetitions, chunk.size());
    spi_transaction_t tx {};
    tx.length = count * 8;
    tx.tx_buffer = chunk.data();
    ESP_RETURN_ON_ERROR(spi_device_polling_transmit(spi_, &tx), TAG, "repeat data");
    repetitions -= count;
  }
  return ESP_OK;
}

esp_err_t Weact213BwDisplay::use_full_frame() {
  return use_partial_frame(0, 0, kNativeWidth, kNativeHeight);
}

esp_err_t Weact213BwDisplay::use_partial_frame(uint16_t x,
                                               uint16_t y,
                                               uint16_t width,
                                               uint16_t height) {
  ESP_RETURN_ON_FALSE(width > 0 && height > 0, ESP_ERR_INVALID_ARG, TAG, "empty frame");
  ESP_RETURN_ON_FALSE((x % 8) == 0 && (width % 8) == 0,
                      ESP_ERR_INVALID_ARG,
                      TAG,
                      "partial frame must be byte aligned");
  ESP_RETURN_ON_ERROR(set_ram_area(x, y, x + width - 1, y + height - 1), TAG, "ram area");
  return set_ram_counter(x, y);
}

esp_err_t Weact213BwDisplay::set_ram_area(uint16_t start_x,
                                          uint16_t start_y,
                                          uint16_t end_x,
                                          uint16_t end_y) {
  const uint8_t x_data[] = {
      static_cast<uint8_t>(start_x >> 3),
      static_cast<uint8_t>(end_x >> 3),
  };
  ESP_RETURN_ON_ERROR(command_with_data(cmd::kSetRamXPos, x_data, sizeof(x_data)),
                      TAG,
                      "set x area");

  const uint8_t y_data[] = {
      static_cast<uint8_t>(start_y),
      static_cast<uint8_t>(start_y >> 8),
      static_cast<uint8_t>(end_y),
      static_cast<uint8_t>(end_y >> 8),
  };
  return command_with_data(cmd::kSetRamYPos, y_data, sizeof(y_data));
}

esp_err_t Weact213BwDisplay::set_ram_counter(uint16_t x, uint16_t y) {
  const uint8_t x_data[] = {static_cast<uint8_t>(x >> 3)};
  ESP_RETURN_ON_ERROR(command_with_data(cmd::kSetRamXCounter, x_data, sizeof(x_data)),
                      TAG,
                      "set x counter");

  const uint8_t y_data[] = {
      static_cast<uint8_t>(y),
      static_cast<uint8_t>(y >> 8),
  };
  return command_with_data(cmd::kSetRamYCounter, y_data, sizeof(y_data));
}

esp_err_t Weact213BwDisplay::write_bw_buffer(const uint8_t* buffer, size_t len) {
  ESP_RETURN_ON_FALSE(buffer != nullptr && len == kNativeBufferSize,
                      ESP_ERR_INVALID_ARG,
                      TAG,
                      "bad BW buffer");
  ESP_RETURN_ON_ERROR(use_full_frame(), TAG, "full frame before BW write");
  return command_with_data(cmd::kWriteBwData, buffer, len);
}

esp_err_t Weact213BwDisplay::write_red_buffer(const uint8_t* buffer, size_t len) {
  ESP_RETURN_ON_FALSE(buffer != nullptr && len == kNativeBufferSize,
                      ESP_ERR_INVALID_ARG,
                      TAG,
                      "bad red/old buffer");
  ESP_RETURN_ON_ERROR(use_full_frame(), TAG, "full frame before red write");
  return command_with_data(cmd::kWriteRedData, buffer, len);
}

esp_err_t Weact213BwDisplay::full_refresh() {
  initial_full_refresh_done_ = true;
  using_partial_mode_ = false;
  const uint8_t mode[] = {flag::kDisplayMode1};
  ESP_RETURN_ON_ERROR(command_with_data(cmd::kUpdateDisplayCtrl2, mode, sizeof(mode)),
                      TAG,
                      "full refresh mode");
  ESP_RETURN_ON_ERROR(command(cmd::kMasterActivate), TAG, "master activate");
  vTaskDelay(pdMS_TO_TICKS(10));
  return wait_until_idle(20'000);
}

esp_err_t Weact213BwDisplay::fast_refresh() {
  if (!initial_full_refresh_done_) {
    ESP_RETURN_ON_ERROR(full_refresh(), TAG, "initial full refresh");
  }
  if (!using_partial_mode_) {
    ESP_RETURN_ON_ERROR(command_with_data(cmd::kWriteLut,
                                         kPartialUpdateLut,
                                         sizeof(kPartialUpdateLut)),
                        TAG,
                        "partial LUT");
    using_partial_mode_ = true;
  }

  const uint8_t mode[] = {flag::kFastRefreshMagic};
  int64_t t0 = esp_timer_get_time();
  ESP_RETURN_ON_ERROR(command_with_data(cmd::kUpdateDisplayCtrl2, mode, sizeof(mode)),
                      TAG,
                      "fast refresh mode");
  ESP_RETURN_ON_ERROR(command(cmd::kMasterActivate), TAG, "fast master activate");
  vTaskDelay(pdMS_TO_TICKS(10));
  esp_err_t err = wait_until_idle(5'000);
  int64_t t1 = esp_timer_get_time();
  ESP32CALC_LOGI(TAG, "fast_refresh: busy_wait=%lldus (total cmd=%lldus)",
                 t1 - t0, t1 - t0);
  return err;
}

esp_err_t Weact213BwDisplay::full_update(const uint8_t* buffer, size_t len) {
  ESP32CALC_LOGD(TAG, "full update");
  ESP_RETURN_ON_ERROR(write_red_buffer(buffer, len), TAG, "write old buffer");
  ESP_RETURN_ON_ERROR(write_bw_buffer(buffer, len), TAG, "write bw buffer");
  ESP_RETURN_ON_ERROR(full_refresh(), TAG, "full refresh");
  ESP_RETURN_ON_ERROR(write_red_buffer(buffer, len), TAG, "sync old buffer");
  return write_bw_buffer(buffer, len);
}

esp_err_t Weact213BwDisplay::fast_update(const uint8_t* buffer, size_t len) {
  ESP32CALC_LOGD(TAG, "fast update");
  ESP_RETURN_ON_ERROR(write_bw_buffer(buffer, len), TAG, "write bw buffer");
  ESP_RETURN_ON_ERROR(fast_refresh(), TAG, "fast refresh");
  ESP_RETURN_ON_ERROR(write_red_buffer(buffer, len), TAG, "sync old buffer");
  return write_bw_buffer(buffer, len);
}

esp_err_t Weact213BwDisplay::write_partial_bw_buffer(
    const uint8_t* buffer, size_t len,
    uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
  ESP_RETURN_ON_ERROR(use_partial_frame(x, y, width, height), TAG, "partial frame");
  return command_with_data(cmd::kWriteBwData, buffer, len);
}

esp_err_t Weact213BwDisplay::write_partial_red_buffer(
    const uint8_t* buffer, size_t len,
    uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
  ESP_RETURN_ON_ERROR(use_partial_frame(x, y, width, height), TAG, "partial frame");
  return command_with_data(cmd::kWriteRedData, buffer, len);
}

esp_err_t Weact213BwDisplay::fast_partial_update(
    const uint8_t* buffer, size_t len,
    uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
  ESP32CALC_LOGD(TAG, "partial update (%ux%u @ %u,%u)", width, height, x, y);
  ESP_RETURN_ON_ERROR(write_partial_bw_buffer(buffer, len, x, y, width, height),
                      TAG, "partial bw write");
  ESP_RETURN_ON_ERROR(fast_refresh(), TAG, "fast refresh");
  ESP_RETURN_ON_ERROR(write_partial_red_buffer(buffer, len, x, y, width, height),
                      TAG, "partial red sync");
  return write_partial_bw_buffer(buffer, len, x, y, width, height);
}

esp_err_t Weact213BwDisplay::update_canvas(const MonoCanvas& canvas, RefreshMode mode) {
  if (!ready_) {
    return ESP_ERR_INVALID_STATE;
  }

  int64_t t0 = esp_timer_get_time();

  // Quick skip: canvas byte-identical to previous → nothing changed
  if (std::memcmp(canvas.data(), previous_canvas_.data(), canvas.size()) == 0) {
    return ESP_OK;
  }
  int64_t t_memcmp = esp_timer_get_time();

  // Find dirty region from canvas byte comparison
  constexpr size_t kCanvasBpr = (MonoCanvas::kWidth + 7) / 8;
  size_t first_cb = canvas.size(), last_cb = 0;
  for (size_t i = 0; i < canvas.size(); i++) {
    if (canvas.data()[i] != previous_canvas_.data()[i]) {
      if (i < first_cb) first_cb = i;
      if (i > last_cb) last_cb = i;
    }
  }
  previous_canvas_ = canvas;

  // Affine: native_x = 127 - logical_y → a native byte (8 px) spans 8 logical
  // rows. Align dirty rows to 8-row boundaries so full native columns are rebuilt.
  const int16_t ly1 = static_cast<int16_t>(first_cb / kCanvasBpr);
  const int16_t ly2 = static_cast<int16_t>(last_cb / kCanvasBpr);
  const int16_t ly1_aligned = (ly1 / 8) * 8;
  const int16_t ly2_aligned = std::min<int16_t>(
      static_cast<int16_t>(((ly2 + 7) / 8) * 8 - 1),
      static_cast<int16_t>(MonoCanvas::kHeight - 1));
  const int16_t dirty_h = ly2_aligned - ly1_aligned + 1;
  constexpr int16_t kFullThresholdH = MonoCanvas::kHeight - 16;
  const bool full_dirty = (dirty_h >= kFullThresholdH);

  previous_ = packed_;
  int64_t t_before_native = esp_timer_get_time();
  canvas.to_epd_native(packed_);
  int64_t t_after_native = esp_timer_get_time();

  // Find dirty bytes in native buffer for partial update positioning
  size_t first = 0, last = 0;
  for (size_t i = 0; i < packed_.size(); i++) {
    if (packed_[i] != previous_[i]) {
      first = i;
      break;
    }
  }
  for (size_t i = packed_.size(); i > 0; i--) {
    if (packed_[i - 1] != previous_[i - 1]) {
      last = i - 1;
      break;
    }
  }
  if (last < first) {
    return ESP_OK;
  }
  int64_t t_dirty = esp_timer_get_time();

  const uint16_t sb = first % 16;
  const uint16_t eb = last % 16;
  const uint16_t sy = first / 16;
  const uint16_t ey = last / 16;
  const uint16_t dirty_w2 = (eb - sb + 1) * 8;
  const uint16_t dirty_h2 = ey - sy + 1;
  const uint16_t dirty_bytes = (eb - sb + 1) * dirty_h2;

  if (mode == RefreshMode::Full || dirty_bytes > packed_.size() / 2) {
    esp_err_t ret = mode == RefreshMode::Full
                    ? full_update(packed_.data(), packed_.size())
                    : fast_update(packed_.data(), packed_.size());
    int64_t t_end = esp_timer_get_time();
    ESP32CALC_LOGI(TAG, "update_canvas: memcmp=%lldus canvas_diff=%lldus to_epd_native=%lldus dirty_scan=%lldus update=%lldus total=%lldus",
                   t_memcmp - t0,
                   t_before_native - t_memcmp,
                   t_after_native - t_before_native,
                   t_dirty - t_after_native,
                   t_end - t_dirty,
                   t_end - t0);
    return ret;
  }

  uint8_t partial[4000];
  size_t idx = 0;
  for (uint16_t r = sy; r <= ey; r++) {
    for (uint16_t bc = sb; bc <= eb; bc++) {
      partial[idx++] = packed_[r * 16 + bc];
    }
  }
  int64_t t_copy = esp_timer_get_time();
  esp_err_t ret = fast_partial_update(partial, idx, sb * 8, sy, dirty_w2, dirty_h2);
  int64_t t_end = esp_timer_get_time();
  ESP32CALC_LOGI(TAG, "update_canvas(PARTIAL): memcmp=%lldus canvas_diff=%lldus to_epd_native=%lldus dirty_scan=%lldus partial_copy=%lldus send=%lldus total=%lldus dirty=(%ux%u @ %u,%u)",
                 t_memcmp - t0,
                 t_before_native - t_memcmp,
                 t_after_native - t_before_native,
                 t_dirty - t_after_native,
                 t_copy - t_dirty,
                 t_end - t_copy,
                 t_end - t0,
                 dirty_w2, dirty_h2, sb * 8, sy);
  return ret;
}

esp_err_t Weact213BwDisplay::update_native_buffer(
    const std::array<uint8_t, config::kDisplayNativeBufferSize>& buffer,
    RefreshMode mode) {
  if (!ready_) {
    return ESP_ERR_INVALID_STATE;
  }

  previous_ = packed_;
  packed_ = buffer;
  previous_canvas_.clear(true);  // invalidate canvas cache for next update_canvas

  size_t first = 0, last = 0;
  for (size_t i = 0; i < packed_.size(); i++) {
    if (packed_[i] != previous_[i]) {
      first = i;
      break;
    }
  }
  for (size_t i = packed_.size(); i > 0; i--) {
    if (packed_[i - 1] != previous_[i - 1]) {
      last = i - 1;
      break;
    }
  }

  if (last < first) {
    return ESP_OK;
  }

  const uint16_t sb = first % 16;
  const uint16_t eb = last % 16;
  const uint16_t sy = first / 16;
  const uint16_t ey = last / 16;
  const uint16_t dirty_w = (eb - sb + 1) * 8;
  const uint16_t dirty_h = ey - sy + 1;
  const uint16_t dirty_bytes = (eb - sb + 1) * dirty_h;

  if (mode == RefreshMode::Full || dirty_bytes > packed_.size() / 2) {
    return mode == RefreshMode::Full
               ? full_update(packed_.data(), packed_.size())
               : fast_update(packed_.data(), packed_.size());
  }

  uint8_t partial[4000];
  size_t idx = 0;
  for (uint16_t r = sy; r <= ey; r++) {
    for (uint16_t bc = sb; bc <= eb; bc++) {
      partial[idx++] = packed_[r * 16 + bc];
    }
  }
  return fast_partial_update(partial, idx, sb * 8, sy, dirty_w, dirty_h);
}

esp_err_t Weact213BwDisplay::sleep() {
  const uint8_t mode[] = {flag::kDeepSleepMode1};
  ESP_RETURN_ON_ERROR(command(cmd::kDeepSleep), TAG, "deep sleep command");
  return data(mode, sizeof(mode), false);
}

}  // namespace esp32calc
