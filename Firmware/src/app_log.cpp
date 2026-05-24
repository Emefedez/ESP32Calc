#include "app_log.h"

#include <cstdarg>
#include <cstdio>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace esp32calc::log {
namespace {
SemaphoreHandle_t s_log_mutex = nullptr;
}

void init() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (s_log_mutex == nullptr) {
    s_log_mutex = xSemaphoreCreateMutex();
  }
}

void write(char level, const char* tag, const char* fmt, ...) {
  if (s_log_mutex != nullptr) {
    xSemaphoreTake(s_log_mutex, portMAX_DELAY);
  }

  timeval tv {};
  gettimeofday(&tv, nullptr);
  std::printf("[%lld.%03ld] %c/%s: ",
              static_cast<long long>(tv.tv_sec),
              static_cast<long>(tv.tv_usec / 1000),
              level,
              tag);

  va_list args;
  va_start(args, fmt);
  std::vprintf(fmt, args);
  va_end(args);
  std::printf("\n");

  if (s_log_mutex != nullptr) {
    xSemaphoreGive(s_log_mutex);
  }
}

void write_error(const char* tag, const char* action, esp_err_t err) {
  write('E', tag, "%s failed: %s (0x%x)", action, esp_err_to_name(err), err);
}

}  // namespace esp32calc::log

