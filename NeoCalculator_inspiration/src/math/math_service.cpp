#include "math/math_service.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "math/math_engine.h"

namespace esp32calc_alt {
namespace {

constexpr const char* TAG = "alt_math";

}  // namespace

esp_err_t MathService::start() {
  if (request_queue_ != nullptr) {
    return ESP_OK;
  }

  request_queue_ = xQueueCreateStatic(kQueueDepth,
                                      sizeof(MathRequest),
                                      request_queue_buffer_,
                                      &request_queue_storage_);
  result_queue_ = xQueueCreateStatic(kQueueDepth,
                                     sizeof(MathResult),
                                     result_queue_buffer_,
                                     &result_queue_storage_);
  if (request_queue_ == nullptr || result_queue_ == nullptr) {
    return ESP_ERR_NO_MEM;
  }

  const BaseType_t ok = xTaskCreatePinnedToCore(&MathService::task_trampoline,
                                                "alt_math",
                                                kTaskStackBytes,
                                                this,
                                                kTaskPriority,
                                                nullptr,
                                                0);
  return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

bool MathService::submit(const MathRequest& request) {
  if (request_queue_ == nullptr) {
    return false;
  }
  return xQueueSend(request_queue_, &request, 0) == pdTRUE;
}

bool MathService::poll_result(MathResult& result, TickType_t wait_ticks) {
  if (result_queue_ == nullptr) {
    return false;
  }
  return xQueueReceive(result_queue_, &result, wait_ticks) == pdTRUE;
}

void MathService::task_trampoline(void* arg) {
  static_cast<MathService*>(arg)->task();
}

void MathService::task() {
  ESP_LOGI(TAG,
           "ready internal=%u psram=%u",
           heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

  MathRequest request {};
  while (true) {
    if (xQueueReceive(request_queue_, &request, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    MathResult result {};
    handle_request(request, result);
    if (xQueueSend(result_queue_, &result, 0) != pdTRUE) {
      ESP_LOGW(TAG, "result queue full");
    }
  }
}

void MathService::handle_request(const MathRequest& request, MathResult& result) {
  result = evaluate_math_request(request);
}

}  // namespace esp32calc_alt
