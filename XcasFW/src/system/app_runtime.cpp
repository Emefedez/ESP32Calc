#include "system/app_runtime.h"

#include <cstdio>
#include <cstring>
#include <strings.h>

#include "app_config.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "system/micropython_runtime.h"

#if ESP32CALC_APP_RUNTIME_SOFT_REBOOT_ON_CLOSE
#include "esp_system.h"
#endif

namespace esp32calc_alt {
namespace {

constexpr const char* TAG = "app_runtime";
constexpr size_t kMicroPythonHeapBytes = ESP32CALC_MICROPYTHON_HEAP_BYTES;
constexpr size_t kMicroPythonSourceBytes = ESP32CALC_MICROPYTHON_SOURCE_BYTES;

bool has_text(const char* text) {
  return text != nullptr && text[0] != '\0';
}

void copy_text(char* output, size_t output_size, const char* input) {
  if (output == nullptr || output_size == 0) {
    return;
  }
  std::snprintf(output, output_size, "%s", input == nullptr ? "" : input);
}

char* trim(char* text) {
  if (text == nullptr) {
    return text;
  }
  while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
    ++text;
  }
  char* end = text + std::strlen(text);
  while (end > text && (end[-1] == ' ' || end[-1] == '\t' ||
                        end[-1] == '\r' || end[-1] == '\n')) {
    --end;
  }
  *end = '\0';
  return text;
}

void trim_in_place(char* text) {
  char* trimmed = trim(text);
  if (trimmed != text) {
    std::memmove(text, trimmed, std::strlen(trimmed) + 1);
  }
}

bool build_entry_path(char* output,
                      size_t output_size,
                      const ExternalAppManifest& manifest) {
  const int written = std::snprintf(output,
                                    output_size,
                                    "%s/%s",
                                    manifest.source_path,
                                    manifest.entry);
  return written >= 0 && static_cast<size_t>(written) < output_size;
}

esp_err_t read_source_file(const char* path, char* output, size_t output_size) {
  if (!has_text(path) || output == nullptr || output_size == 0) {
    return ESP_ERR_INVALID_ARG;
  }
  output[0] = '\0';
  FILE* file = std::fopen(path, "rb");
  if (file == nullptr) {
    return ESP_ERR_NOT_FOUND;
  }

  const size_t used = std::fread(output, 1, output_size - 1, file);
  if (std::ferror(file) != 0) {
    std::fclose(file);
    return ESP_FAIL;
  }
  const int extra = std::fgetc(file);
  std::fclose(file);
  if (extra != EOF) {
    output[0] = '\0';
    return ESP_ERR_INVALID_SIZE;
  }
  output[used] = '\0';
  return ESP_OK;
}

void* allocate_mpy_heap(size_t size) {
  void* heap = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (heap == nullptr) {
    heap = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }
  return heap;
}

}  // namespace

esp_err_t AppRuntime::open(const ExternalAppManifest& manifest) {
  close(false);
  manifest_ = &manifest;

  if (strcasecmp(manifest.kind, "micropython") != 0) {
    set_error("unsupported app kind");
    return ESP_ERR_NOT_SUPPORTED;
  }
  if (!has_text(manifest.source_path) || !has_text(manifest.entry)) {
    set_error("manifest missing entry");
    return ESP_ERR_INVALID_ARG;
  }
  if (!build_entry_path(entry_path_, sizeof(entry_path_), manifest)) {
    set_error("entry path too long");
    return ESP_ERR_INVALID_SIZE;
  }

  char source[kMicroPythonSourceBytes] {};
  const esp_err_t read_err = read_source_file(entry_path_, source, sizeof(source));
  if (read_err == ESP_ERR_NOT_FOUND) {
    set_error("entry file missing");
    return read_err;
  }
  if (read_err == ESP_ERR_INVALID_SIZE) {
    set_error("entry file too large");
    return read_err;
  }
  if (read_err != ESP_OK) {
    set_error("entry read failed");
    return read_err;
  }

  mpy_heap_ = allocate_mpy_heap(kMicroPythonHeapBytes);
  if (mpy_heap_ == nullptr) {
    set_error("MicroPython heap failed");
    return ESP_ERR_NO_MEM;
  }

  bool script_ok = false;
  const esp_err_t run_err = micropython_runtime_start(
      mpy_heap_, kMicroPythonHeapBytes, source, manifest.source_path, preview_, sizeof(preview_), &script_ok);
  mpy_started_ = run_err == ESP_OK;
  trim_in_place(preview_);

  if (run_err != ESP_OK) {
    release_vm();
    set_error("MicroPython start failed", false);
    return run_err;
  }
  if (!script_ok) {
    release_vm();
    set_error("MicroPython error", false);
    return ESP_FAIL;
  }

  state_ = AppRuntimeState::Running;
  copy_text(message_, sizeof(message_), "MicroPython executed");
  if (!has_text(preview_)) {
    copy_text(preview_, sizeof(preview_), "no output");
  }
  ESP_LOGI(TAG, "open id=%s entry=%s heap=%u", active_id(), entry_path_, static_cast<unsigned>(kMicroPythonHeapBytes));
  return ESP_OK;
}

void AppRuntime::close(bool allow_soft_reboot) {
  const bool had_runtime = state_ != AppRuntimeState::Idle || mpy_started_ || mpy_heap_ != nullptr;
  if (had_runtime) {
    ESP_LOGI(TAG, "close id=%s", active_id());
  }
  release_vm();
  reset();

#if ESP32CALC_APP_RUNTIME_SOFT_REBOOT_ON_CLOSE
  if (allow_soft_reboot && had_runtime) {
    ESP_LOGW(TAG, "soft reboot after app runtime close");
    esp_restart();
  }
#else
  (void)allow_soft_reboot;
#endif
}

void AppRuntime::on_key(const char* token) {
  if (state_ != AppRuntimeState::Running || !mpy_started_) {
    return;
  }
  micropython_runtime_on_key(token);
}

void AppRuntime::release_vm() {
  if (mpy_started_) {
    micropython_runtime_stop();
    mpy_started_ = false;
  }
  if (mpy_heap_ != nullptr) {
    heap_caps_free(mpy_heap_);
    mpy_heap_ = nullptr;
  }
}

void AppRuntime::reset() {
  state_ = AppRuntimeState::Idle;
  mpy_heap_ = nullptr;
  mpy_started_ = false;
  manifest_ = nullptr;
  entry_path_[0] = '\0';
  message_[0] = '\0';
  preview_[0] = '\0';
}

void AppRuntime::set_error(const char* message, bool clear_preview) {
  state_ = AppRuntimeState::Error;
  copy_text(message_, sizeof(message_), message);
  if (clear_preview) {
    preview_[0] = '\0';
  }
  ESP_LOGW(TAG, "open failed id=%s: %s", active_id(), message_);
}

}  // namespace esp32calc_alt
