#pragma once

#include <cstddef>
#include <cstdint>
#include <algorithm>

#ifndef ESP32CALC_DEBUG_LOGS
#define ESP32CALC_DEBUG_LOGS 1
#endif

#ifndef ESP32CALC_USE_RAYLIB
#define ESP32CALC_USE_RAYLIB 0
#endif

#ifndef ESP32CALC_FORCE_FULL_REFRESH
#define ESP32CALC_FORCE_FULL_REFRESH 1
#endif

#ifndef ESP32CALC_BOOT_DISPLAY_TEST
#define ESP32CALC_BOOT_DISPLAY_TEST 0
#endif

#ifndef ESP32CALC_BOOT_NATIVE_TEST
#define ESP32CALC_BOOT_NATIVE_TEST 0
#endif

#ifndef ESP32CALC_EPD_SPI_HZ
#define ESP32CALC_EPD_SPI_HZ 100000
#endif

#ifndef ESP32CALC_EPD_POWER_SETTLE_MS
#define ESP32CALC_EPD_POWER_SETTLE_MS 250
#endif

namespace esp32calc::config {

constexpr uint16_t kDisplayLogicalWidth = 250;
constexpr uint16_t kDisplayLogicalHeight = 128;
constexpr uint16_t kDisplayVisibleLogicalHeight = 122;
constexpr uint16_t kDisplayNativeWidth = 128;
constexpr uint16_t kDisplayVisibleNativeWidth = 122;
constexpr uint16_t kDisplayNativeHeight = 250;
constexpr size_t kDisplayNativeBufferSize =
    (kDisplayNativeWidth / 8) * kDisplayNativeHeight;

constexpr uint32_t kInputScanPeriodMs = 5;
constexpr uint8_t kInputDebounceSamples = 4;
constexpr uint32_t kBatteryPollPeriodMs = 30'000;

constexpr uint8_t kCoreServicesCore = 0;
constexpr uint8_t kUiCore = 1;

constexpr uint32_t kAppEventQueueDepth = 32;

// Adjust this to match the actual resistor divider between VBAT and GPIO3.
constexpr float kBatteryDividerRatio = 2.0f;
constexpr uint16_t kBatteryEmptyMv = 3300;
constexpr uint16_t kBatteryFullMv = 4200;

constexpr const char* kSdMountPoint = "/sdcard";
constexpr const char* kProgramsPath = "/sdcard/programs";

}  // namespace esp32calc::config

struct DirtyRect {
  bool is_full = true;
  int16_t x = 0, y = 0, w = 0, h = 0;

  void expand(int16_t px, int16_t py) {
    if (is_full) return;
    if (w == 0) { x = px; y = py; w = 1; h = 1; return; }
    if (px < x) { w += static_cast<int16_t>(x - px); x = px; }
    if (py < y) { h += static_cast<int16_t>(y - py); y = py; }
    if (px - x + 1 > w) w = static_cast<int16_t>(px - x + 1);
    if (py - y + 1 > h) h = static_cast<int16_t>(py - y + 1);
  }
};
