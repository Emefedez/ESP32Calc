#include "system/boot_mode.h"

#include <cstdio>
#include <cstring>

#include "app_config.h"
#include "esp_log.h"

namespace esp32calc_alt::boot {
namespace {

constexpr const char* TAG = "boot_mode";
constexpr const char* kModeFilePath = "/internal/boot_mode";
constexpr size_t kLineCapacity = 64;

bool read_line(FILE* file, char* output, size_t output_size) {
  if (std::fgets(output, static_cast<int>(output_size), file) == nullptr) {
    return false;
  }
  size_t len = std::strlen(output);
  while (len > 0 && (output[len - 1] == '\n' || output[len - 1] == '\r')) {
    output[--len] = '\0';
  }
  return len > 0;
}

}  // namespace

Mode current_mode() {
  FILE* file = std::fopen(kModeFilePath, "r");
  if (file == nullptr) {
    return Mode::Calculator;
  }
  char line[kLineCapacity] {};
  const bool ok = read_line(file, line, sizeof(line));
  std::fclose(file);
  if (!ok) {
    return Mode::Calculator;
  }
  if (std::strcmp(line, "mpy") == 0) {
    return Mode::MicroPython;
  }
  return Mode::Calculator;
}

bool set_mode(Mode mode, const char* app_id) {
  FILE* file = std::fopen(kModeFilePath, "w");
  if (file == nullptr) {
    ESP_LOGE(TAG, "failed to write %s", kModeFilePath);
    return false;
  }
  const char* mode_str = (mode == Mode::MicroPython) ? "mpy" : "calc";
  std::fprintf(file, "%s\n", mode_str);
  if (app_id != nullptr && app_id[0] != '\0') {
    std::fprintf(file, "%s\n", app_id);
  }
  std::fclose(file);
  ESP_LOGI(TAG, "mode=%s app=%s", mode_str, app_id ? app_id : "");
  return true;
}

const char* pending_app_id() {
  static char app_id[kLineCapacity] {};
  app_id[0] = '\0';
  FILE* file = std::fopen(kModeFilePath, "r");
  if (file == nullptr) {
    return app_id;
  }
  char line[kLineCapacity] {};
  if (!read_line(file, line, sizeof(line))) {
    std::fclose(file);
    return app_id;
  }
  read_line(file, app_id, sizeof(app_id));
  std::fclose(file);
  return app_id;
}

}  // namespace esp32calc_alt::boot
