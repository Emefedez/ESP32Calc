#pragma once

#include "app_events.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "system/storage_manager.h"
#include "system/wifi_manager.h"

namespace esp32calc_alt {

class ChatbotService {
 public:
  esp_err_t start(QueueHandle_t app_events, WifiManager& wifi);
  esp_err_t ask(const ChatbotSettings& settings, const char* prompt);

 private:
  struct Request {
    ChatbotSettings settings {};
    char prompt[128] {};
  };

  static void task_entry(void* arg);
  void task_loop();
  void handle_request(const Request& request);
  void publish(bool ok, const char* text);

  QueueHandle_t app_events_ = nullptr;
  WifiManager* wifi_ = nullptr;
  QueueHandle_t requests_ = nullptr;
  StaticQueue_t request_storage_ {};
  uint8_t request_buffer_[sizeof(Request)] {};
};

}  // namespace esp32calc_alt
