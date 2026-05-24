#pragma once

#include "esp_err.h"

namespace esp32calc::log {

void init();
void write(char level, const char* tag, const char* fmt, ...);
void write_error(const char* tag, const char* action, esp_err_t err);

}  // namespace esp32calc::log

#if ESP32CALC_DEBUG_LOGS
#define ESP32CALC_LOGD(tag, fmt, ...) \
  ::esp32calc::log::write('D', tag, fmt, ##__VA_ARGS__)
#else
#define ESP32CALC_LOGD(tag, fmt, ...) ((void)0)
#endif

#define ESP32CALC_LOGI(tag, fmt, ...) \
  ::esp32calc::log::write('I', tag, fmt, ##__VA_ARGS__)

#define ESP32CALC_LOGW(tag, fmt, ...) \
  ::esp32calc::log::write('W', tag, fmt, ##__VA_ARGS__)

#define ESP32CALC_LOGE(tag, fmt, ...) \
  ::esp32calc::log::write('E', tag, fmt, ##__VA_ARGS__)

#define ESP32CALC_LOG_ERR(tag, action, err) \
  ::esp32calc::log::write_error(tag, action, err)

