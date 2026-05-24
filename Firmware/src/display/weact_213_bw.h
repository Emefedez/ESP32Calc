#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "app_config.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "graphics/mono_canvas.h"

namespace esp32calc {

enum class RefreshMode : uint8_t {
  Full,
  Fast,
};

class Weact213BwDisplay {
 public:
  esp_err_t init();
  bool ready() const { return ready_; }

  esp_err_t full_update(const uint8_t* buffer, size_t len);
  esp_err_t fast_update(const uint8_t* buffer, size_t len);
  esp_err_t fast_partial_update(const uint8_t* buffer, size_t len,
                                uint16_t x, uint16_t y,
                                uint16_t width, uint16_t height);
  esp_err_t update_canvas(const MonoCanvas& canvas, RefreshMode mode);
  esp_err_t update_native_buffer(const std::array<uint8_t, config::kDisplayNativeBufferSize>& buffer,
                                 RefreshMode mode);
  esp_err_t sleep();

 private:
  esp_err_t hw_reset();
  esp_err_t wait_until_idle(uint32_t timeout_ms = 10'000);
  esp_err_t command(uint8_t value);
  esp_err_t data(const uint8_t* data, size_t len, bool wait = true);
  esp_err_t command_with_data(uint8_t cmd, const uint8_t* data, size_t len);
  esp_err_t use_full_frame();
  esp_err_t use_partial_frame(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
  esp_err_t set_ram_area(uint16_t start_x, uint16_t start_y, uint16_t end_x, uint16_t end_y);
  esp_err_t set_ram_counter(uint16_t x, uint16_t y);
  esp_err_t write_bw_buffer(const uint8_t* buffer, size_t len);
  esp_err_t write_red_buffer(const uint8_t* buffer, size_t len);
  esp_err_t write_partial_bw_buffer(const uint8_t* buffer, size_t len,
                                    uint16_t x, uint16_t y,
                                    uint16_t width, uint16_t height);
  esp_err_t write_partial_red_buffer(const uint8_t* buffer, size_t len,
                                     uint16_t x, uint16_t y,
                                     uint16_t width, uint16_t height);
  esp_err_t full_refresh();
  esp_err_t fast_refresh();

  spi_device_handle_t spi_ = nullptr;
  bool ready_ = false;
  bool using_partial_mode_ = false;
  bool initial_full_refresh_done_ = false;
  std::array<uint8_t, config::kDisplayNativeBufferSize> packed_ {};
  std::array<uint8_t, config::kDisplayNativeBufferSize> previous_ {};
  MonoCanvas previous_canvas_ {};
};

}  // namespace esp32calc
