#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"
#include "sdmmc_cmd.h"
#include "wear_levelling.h"

namespace esp32calc_alt {

struct WifiSettings {
  bool loaded = false;
  char ssid[64] {};
  char password[64] {};
  char hostname[32] {};
};

struct ChatbotSettings {
  bool loaded = false;
  char provider[16] {};
  char endpoint[128] {};
  char model[48] {};
  char api_key[128] {};
  char system_prompt[128] {};
};

struct ExternalAppManifest {
  bool valid = false;
  bool on_sd = false;
  bool on_internal = false;
  bool allow_keymap = false;
  char id[32] {};
  char name[48] {};
  char kind[24] {};
  char entry[64] {};
  char source_path[128] {};
  char internal_path[128] {};
  ChatbotSettings chatbot {};
};

class StorageManager {
 public:
  esp_err_t init();
  bool sd_mounted() const { return sd_mounted_; }
  bool internal_mounted() const { return internal_mounted_; }
  const WifiSettings& wifi() const { return wifi_; }
  const ChatbotSettings& chatbot() const { return chatbot_; }
  size_t app_count() const { return app_count_; }
  const ExternalAppManifest& app(size_t index) const { return apps_[index]; }

  esp_err_t apply_global_keymap();
  esp_err_t apply_app_keymap(const char* app_id);
  esp_err_t copy_program_to_internal(const char* app_id);

 private:
  static constexpr size_t kMaxApps = 8;

  esp_err_t mount_sd();
  esp_err_t mount_internal();
  void load_configs();
  void scan_program_root(const char* root, bool on_sd);
  bool load_app_manifest(const char* manifest_path,
                         const char* root_path,
                         bool on_sd,
                         ExternalAppManifest& manifest);
  void merge_app_manifest(const ExternalAppManifest& manifest);
  esp_err_t apply_keymap_file(const char* path);
  esp_err_t copy_flat_directory(const char* source, const char* destination);

  bool sd_mounted_ = false;
  bool internal_mounted_ = false;
  sdmmc_card_t* sd_card_ = nullptr;
  wl_handle_t internal_wl_ = WL_INVALID_HANDLE;
  WifiSettings wifi_ {};
  ChatbotSettings chatbot_ {};
  ExternalAppManifest apps_[kMaxApps] {};
  size_t app_count_ = 0;
};

}  // namespace esp32calc_alt
