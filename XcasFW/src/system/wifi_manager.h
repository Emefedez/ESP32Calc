#pragma once

#include "esp_err.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "system/storage_manager.h"

namespace esp32calc_alt {

class WifiManager {
 public:
  esp_err_t start(const WifiSettings& settings);
  bool started() const { return started_; }
  bool connected() const;
  bool wait_connected(uint32_t timeout_ms) const;

 private:
  static void event_handler(void* arg,
                            esp_event_base_t event_base,
                            int32_t event_id,
                            void* event_data);

  EventGroupHandle_t events_ = nullptr;
  bool started_ = false;
};

}  // namespace esp32calc_alt
