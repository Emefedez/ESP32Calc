#pragma once

#include <cstdint>
#include <cstddef>

#include "esp_err.h"
#include "system/storage_manager.h"

namespace esp32calc_alt {

enum class AppRuntimeState : uint8_t {
  Idle,
  Running,
  Error,
};

class AppRuntime {
 public:
  esp_err_t open(const ExternalAppManifest& manifest);
  void close(bool allow_soft_reboot = true);

  AppRuntimeState state() const { return state_; }
  bool running() const { return state_ == AppRuntimeState::Running; }
  const char* active_name() const { return active_name_; }
  const char* entry_path() const { return entry_path_; }
  const char* message() const { return message_; }
  const char* preview() const { return preview_; }

 private:
  void release_vm();
  void reset();
  void set_error(const char* message, bool clear_preview = true);

  AppRuntimeState state_ = AppRuntimeState::Idle;
  void* mpy_heap_ = nullptr;
  bool mpy_started_ = false;
  char active_id_[32] {};
  char active_name_[48] {};
  char entry_path_[160] {};
  char message_[72] {};
  char preview_[96] {};
};

}  // namespace esp32calc_alt
