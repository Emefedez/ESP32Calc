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
  void on_key(const char* token);

  AppRuntimeState state() const { return state_; }
  bool running() const { return state_ == AppRuntimeState::Running; }
  const char* active_id() const { return manifest_ ? manifest_->id : ""; }
  const char* active_name() const { return manifest_ ? manifest_->name : ""; }
  const char* entry_path() const { return entry_path_; }
  const char* message() const { return message_; }

 private:
  void release_vm();
  void reset();
  void set_error(const char* message, bool clear_preview = true);

  AppRuntimeState state_ = AppRuntimeState::Idle;
  void* mpy_heap_ = nullptr;
  bool mpy_started_ = false;
  const ExternalAppManifest* manifest_ = nullptr;
  char entry_path_[160] {};
  char message_[48] {};
};

}  // namespace esp32calc_alt
