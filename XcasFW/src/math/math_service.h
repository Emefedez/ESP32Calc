#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "math/giac/giac_bridge.h"
#include "math/types.h"

namespace esp32calc_alt {

class MathService {
 public:
  esp_err_t start();
  bool submit(const MathRequest& request);
  bool poll_result(MathResult& result, TickType_t wait_ticks);

 private:
  static constexpr UBaseType_t kQueueDepth = 4;
  static constexpr uint32_t kTaskStackBytes = 64 * 1024;
  static constexpr uint32_t kFallbackTaskStackBytes = 24 * 1024;
  static constexpr UBaseType_t kTaskPriority = 5;

  static void task_trampoline(void* arg);
  void task();
  void handle_request(const MathRequest& request, MathResult& result);

  QueueHandle_t request_queue_ = nullptr;
  QueueHandle_t result_queue_ = nullptr;
  StaticQueue_t request_queue_storage_ {};
  StaticQueue_t result_queue_storage_ {};
  uint8_t request_queue_buffer_[kQueueDepth * sizeof(MathRequest)] {};
  uint8_t result_queue_buffer_[kQueueDepth * sizeof(MathResult)] {};
  giac::GiacBridge giac_bridge_ {};
};

}  // namespace esp32calc_alt
