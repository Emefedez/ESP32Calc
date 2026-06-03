#include "system/storage_manager.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "app_config.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "hardware/keymap.h"
#include "hardware/pins.h"
#include "sdmmc_cmd.h"

namespace esp32calc_alt {
namespace {

constexpr const char* TAG = "storage";
constexpr size_t kLineCapacity = 256;
constexpr size_t kCopyBufferSize = 512;

bool has_text(const char* text) {
  return text != nullptr && text[0] != '\0';
}

void copy_text(char* output, size_t output_size, const char* input) {
  if (output == nullptr || output_size == 0) {
    return;
  }
  if (input == nullptr) {
    output[0] = '\0';
    return;
  }
  std::snprintf(output, output_size, "%s", input);
}

char* trim(char* text) {
  if (text == nullptr) {
    return text;
  }
  while (*text != '\0' && (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')) {
    ++text;
  }
  char* end = text + std::strlen(text);
  while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
    --end;
  }
  *end = '\0';
  return text;
}

bool split_kv(char* line, char*& key, char*& value) {
  key = trim(line);
  if (!has_text(key) || key[0] == '#') {
    return false;
  }
  char* equals = std::strchr(key, '=');
  if (equals == nullptr) {
    return false;
  }
  *equals = '\0';
  value = trim(equals + 1);
  key = trim(key);
  return has_text(key);
}

bool ensure_dir(const char* path) {
  if (!has_text(path)) {
    return false;
  }
  if (mkdir(path, 0775) == 0) {
    return true;
  }
  struct stat st {};
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool file_exists(const char* path) {
  struct stat st {};
  return path != nullptr && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

FILE* open_with_example_fallback(const char* path) {
  FILE* file = std::fopen(path, "r");
  if (file != nullptr) {
    return file;
  }
  char fallback[160] {};
  std::snprintf(fallback, sizeof(fallback), "%s.example", path);
  file = std::fopen(fallback, "r");
  if (file != nullptr) {
    ESP_LOGW(TAG, "using example config fallback: %s", fallback);
  }
  return file;
}

bool is_dir_path(const char* path) {
  struct stat st {};
  return path != nullptr && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

void append_path(char* output, size_t output_size, const char* base, const char* leaf) {
  std::snprintf(output, output_size, "%s/%s", base, leaf);
}

bool parse_bool(const char* value) {
  return value != nullptr &&
         (strcasecmp(value, "1") == 0 || strcasecmp(value, "true") == 0 ||
          strcasecmp(value, "yes") == 0 || strcasecmp(value, "on") == 0);
}

bool parse_key_field(const char* key, uint8_t& row, uint8_t& col, const char*& field) {
  if (key == nullptr || std::strncmp(key, "key.", 4) != 0) {
    return false;
  }
  const char* row_text = key + 4;
  char* row_end = nullptr;
  const long parsed_row = std::strtol(row_text, &row_end, 10);
  if (row_end == row_text || row_end == nullptr || *row_end != '.') {
    return false;
  }
  const char* col_text = row_end + 1;
  char* col_end = nullptr;
  const long parsed_col = std::strtol(col_text, &col_end, 10);
  if (col_end == col_text || col_end == nullptr || *col_end != '.') {
    return false;
  }
  if (parsed_row < 0 || parsed_row >= static_cast<long>(kMatrixRowCount) ||
      parsed_col < 0 || parsed_col >= static_cast<long>(kMatrixColCount)) {
    return false;
  }
  row = static_cast<uint8_t>(parsed_row);
  col = static_cast<uint8_t>(parsed_col);
  field = col_end + 1;
  return has_text(field);
}

}  // namespace

esp_err_t StorageManager::init() {
  esp_err_t status = mount_internal();
  if (status != ESP_OK) {
    ESP_LOGW(TAG, "internal FAT mount failed: %s", esp_err_to_name(status));
  }

  const esp_err_t sd_status = mount_sd();
  if (sd_status != ESP_OK) {
    ESP_LOGW(TAG, "SD mount skipped/failed: %s", esp_err_to_name(sd_status));
  }

  load_configs();
  return internal_mounted_ || sd_mounted_ ? ESP_OK : status;
}

esp_err_t StorageManager::mount_sd() {
  esp_vfs_fat_sdmmc_mount_config_t mount_config {};
  mount_config.format_if_mount_failed = false;
  mount_config.max_files = 8;
  mount_config.allocation_unit_size = 16 * 1024;

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = pins::kSdSpiHost;

  sdspi_device_config_t device_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  device_config.host_id = pins::kSdSpiHost;
  device_config.gpio_cs = pins::kSdCs;

  spi_bus_config_t bus_config {};
  bus_config.mosi_io_num = pins::kSdMosi;
  bus_config.miso_io_num = pins::kSdMiso;
  bus_config.sclk_io_num = pins::kSdSclk;
  bus_config.quadwp_io_num = -1;
  bus_config.quadhd_io_num = -1;
  bus_config.max_transfer_sz = 4096;

  esp_err_t err = spi_bus_initialize(pins::kSdSpiHost, &bus_config, SDSPI_DEFAULT_DMA);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return err;
  }

  err = esp_vfs_fat_sdspi_mount(config::kSdMountPoint,
                                &host,
                                &device_config,
                                &mount_config,
                                &sd_card_);
  if (err == ESP_OK) {
    sd_mounted_ = true;
    ESP_LOGI(TAG, "SD mounted at %s", config::kSdMountPoint);
  }
  return err;
}

esp_err_t StorageManager::mount_internal() {
  esp_vfs_fat_mount_config_t mount_config {};
  mount_config.format_if_mount_failed = true;
  mount_config.max_files = 8;
  mount_config.allocation_unit_size = 4096;

  const esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(config::kInternalMountPoint,
                                                         "storage",
                                                         &mount_config,
                                                         &internal_wl_);
  if (err == ESP_OK) {
    internal_mounted_ = true;
    ensure_dir(config::kInternalProgramsPath);
    ESP_LOGI(TAG, "internal FAT mounted at %s", config::kInternalMountPoint);
  }
  return err;
}

void StorageManager::load_configs() {
  if (sd_mounted_) {
    FILE* wifi_file = open_with_example_fallback(config::kWifiConfigPath);
    if (wifi_file != nullptr) {
      char line[kLineCapacity] {};
      while (std::fgets(line, sizeof(line), wifi_file) != nullptr) {
        char* key = nullptr;
        char* value = nullptr;
        if (!split_kv(line, key, value)) {
          continue;
        }
        if (strcasecmp(key, "ssid") == 0) {
          copy_text(wifi_.ssid, sizeof(wifi_.ssid), value);
        } else if (strcasecmp(key, "password") == 0) {
          copy_text(wifi_.password, sizeof(wifi_.password), value);
        } else if (strcasecmp(key, "hostname") == 0) {
          copy_text(wifi_.hostname, sizeof(wifi_.hostname), value);
        }
      }
      std::fclose(wifi_file);
      wifi_.loaded = has_text(wifi_.ssid);
      ESP_LOGI(TAG, "wifi config %s", wifi_.loaded ? "loaded" : "missing ssid");
    }

    FILE* chatbot_file = open_with_example_fallback(config::kChatbotConfigPath);
    if (chatbot_file != nullptr) {
      char line[kLineCapacity] {};
      while (std::fgets(line, sizeof(line), chatbot_file) != nullptr) {
        char* key = nullptr;
        char* value = nullptr;
        if (!split_kv(line, key, value)) {
          continue;
        }
        if (strcasecmp(key, "provider") == 0) {
          copy_text(chatbot_.provider, sizeof(chatbot_.provider), value);
        } else if (strcasecmp(key, "endpoint") == 0) {
          copy_text(chatbot_.endpoint, sizeof(chatbot_.endpoint), value);
        } else if (strcasecmp(key, "model") == 0) {
          copy_text(chatbot_.model, sizeof(chatbot_.model), value);
        } else if (strcasecmp(key, "api_key") == 0) {
          copy_text(chatbot_.api_key, sizeof(chatbot_.api_key), value);
        } else if (strcasecmp(key, "system_prompt") == 0) {
          copy_text(chatbot_.system_prompt, sizeof(chatbot_.system_prompt), value);
        }
      }
      std::fclose(chatbot_file);
      chatbot_.loaded = has_text(chatbot_.api_key) && has_text(chatbot_.endpoint);
      ESP_LOGI(TAG, "chatbot config %s", chatbot_.loaded ? "loaded" : "incomplete");
    }

    apply_global_keymap();
    scan_program_root(config::kProgramsPath, true);
  }

  if (internal_mounted_) {
    scan_program_root(config::kInternalProgramsPath, false);
  }
}

void StorageManager::scan_program_root(const char* root, bool on_sd) {
  DIR* dir = opendir(root);
  if (dir == nullptr) {
    return;
  }

  dirent* entry = nullptr;
  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_name[0] == '.') {
      continue;
    }

    char app_dir[128] {};
    append_path(app_dir, sizeof(app_dir), root, entry->d_name);
    if (!is_dir_path(app_dir)) {
      continue;
    }

    char manifest_path[160] {};
    append_path(manifest_path, sizeof(manifest_path), app_dir, "app.ini");
    ExternalAppManifest manifest {};
    if (load_app_manifest(manifest_path, app_dir, on_sd, manifest)) {
      merge_app_manifest(manifest);
    }
  }
  closedir(dir);
}

bool StorageManager::load_app_manifest(const char* manifest_path,
                                       const char* root_path,
                                       bool on_sd,
                                       ExternalAppManifest& manifest) {
  FILE* file = std::fopen(manifest_path, "r");
  if (file == nullptr) {
    return false;
  }

  manifest.valid = true;
  manifest.on_sd = on_sd;
  manifest.on_internal = !on_sd;
  copy_text(manifest.source_path, sizeof(manifest.source_path), root_path);

  char line[kLineCapacity] {};
  while (std::fgets(line, sizeof(line), file) != nullptr) {
    char* key = nullptr;
    char* value = nullptr;
    if (!split_kv(line, key, value)) {
      continue;
    }
    if (strcasecmp(key, "id") == 0) {
      copy_text(manifest.id, sizeof(manifest.id), value);
    } else if (strcasecmp(key, "name") == 0) {
      copy_text(manifest.name, sizeof(manifest.name), value);
    } else if (strcasecmp(key, "kind") == 0) {
      copy_text(manifest.kind, sizeof(manifest.kind), value);
    } else if (strcasecmp(key, "entry") == 0) {
      copy_text(manifest.entry, sizeof(manifest.entry), value);
    } else if (strcasecmp(key, "allow_keymap") == 0) {
      manifest.allow_keymap = parse_bool(value);
    } else if (strcasecmp(key, "chatbot.provider") == 0) {
      copy_text(manifest.chatbot.provider, sizeof(manifest.chatbot.provider), value);
    } else if (strcasecmp(key, "chatbot.endpoint") == 0) {
      copy_text(manifest.chatbot.endpoint, sizeof(manifest.chatbot.endpoint), value);
    } else if (strcasecmp(key, "chatbot.model") == 0) {
      copy_text(manifest.chatbot.model, sizeof(manifest.chatbot.model), value);
    } else if (strcasecmp(key, "chatbot.api_key") == 0) {
      copy_text(manifest.chatbot.api_key, sizeof(manifest.chatbot.api_key), value);
    } else if (strcasecmp(key, "chatbot.system_prompt") == 0) {
      copy_text(manifest.chatbot.system_prompt, sizeof(manifest.chatbot.system_prompt), value);
    }
  }
  std::fclose(file);

  if (!has_text(manifest.id)) {
    return false;
  }
  if (!has_text(manifest.name)) {
    copy_text(manifest.name, sizeof(manifest.name), manifest.id);
  }
  if (!has_text(manifest.kind)) {
    copy_text(manifest.kind, sizeof(manifest.kind), "external");
  }
  if (!has_text(manifest.chatbot.endpoint)) {
    copy_text(manifest.chatbot.endpoint, sizeof(manifest.chatbot.endpoint), chatbot_.endpoint);
  }
  if (!has_text(manifest.chatbot.model)) {
    copy_text(manifest.chatbot.model, sizeof(manifest.chatbot.model), chatbot_.model);
  }
  if (!has_text(manifest.chatbot.api_key)) {
    copy_text(manifest.chatbot.api_key, sizeof(manifest.chatbot.api_key), chatbot_.api_key);
  }
  manifest.chatbot.loaded =
      has_text(manifest.chatbot.endpoint) && has_text(manifest.chatbot.api_key);
  std::snprintf(manifest.internal_path,
                sizeof(manifest.internal_path),
                "%s/%s",
                config::kInternalProgramsPath,
                manifest.id);
  return true;
}

void StorageManager::merge_app_manifest(const ExternalAppManifest& manifest) {
  for (size_t i = 0; i < app_count_; ++i) {
    if (std::strcmp(apps_[i].id, manifest.id) != 0) {
      continue;
    }
    apps_[i] = manifest;
    return;
  }
  if (app_count_ < kMaxApps) {
    apps_[app_count_++] = manifest;
  }
}

esp_err_t StorageManager::apply_keymap_file(const char* path) {
  FILE* file = std::fopen(path, "r");
  if (file == nullptr) {
    return ESP_ERR_NOT_FOUND;
  }

  char line[kLineCapacity] {};
  while (std::fgets(line, sizeof(line), file) != nullptr) {
    char* key = nullptr;
    char* value = nullptr;
    if (!split_kv(line, key, value)) {
      continue;
    }
    uint8_t row = 0;
    uint8_t col = 0;
    const char* field = nullptr;
    if (parse_key_field(key, row, col, field)) {
      set_key_override_field(row, col, field, value);
    }
  }
  std::fclose(file);
  ESP_LOGI(TAG, "keymap applied: %s", path);
  return ESP_OK;
}

esp_err_t StorageManager::apply_global_keymap() {
  if (!sd_mounted_) {
    return ESP_ERR_INVALID_STATE;
  }
  return apply_keymap_file(config::kGlobalKeymapPath);
}

esp_err_t StorageManager::apply_app_keymap(const char* app_id) {
  if (!has_text(app_id)) {
    return ESP_ERR_INVALID_ARG;
  }
  for (size_t i = 0; i < app_count_; ++i) {
    if (std::strcmp(apps_[i].id, app_id) != 0) {
      continue;
    }
    if (!apps_[i].allow_keymap) {
      return ESP_ERR_NOT_ALLOWED;
    }
    char path[160] {};
    append_path(path, sizeof(path), apps_[i].source_path, "keymap.ini");
    return apply_keymap_file(path);
  }
  return ESP_ERR_NOT_FOUND;
}

esp_err_t StorageManager::copy_program_to_internal(const char* app_id) {
  if (!internal_mounted_ || !sd_mounted_) {
    return ESP_ERR_INVALID_STATE;
  }
  for (size_t i = 0; i < app_count_; ++i) {
    if (std::strcmp(apps_[i].id, app_id) != 0 || !apps_[i].on_sd) {
      continue;
    }
    return copy_flat_directory(apps_[i].source_path, apps_[i].internal_path);
  }
  return ESP_ERR_NOT_FOUND;
}

esp_err_t StorageManager::copy_flat_directory(const char* source, const char* destination) {
  if (!ensure_dir(config::kInternalProgramsPath) || !ensure_dir(destination)) {
    return ESP_FAIL;
  }

  DIR* dir = opendir(source);
  if (dir == nullptr) {
    return ESP_ERR_NOT_FOUND;
  }

  uint8_t buffer[kCopyBufferSize] {};
  dirent* entry = nullptr;
  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_name[0] == '.') {
      continue;
    }

    char source_path[160] {};
    char destination_path[160] {};
    append_path(source_path, sizeof(source_path), source, entry->d_name);
    append_path(destination_path, sizeof(destination_path), destination, entry->d_name);
    if (!file_exists(source_path)) {
      continue;
    }

    FILE* in = std::fopen(source_path, "rb");
    FILE* out = std::fopen(destination_path, "wb");
    if (in == nullptr || out == nullptr) {
      if (in != nullptr) {
        std::fclose(in);
      }
      if (out != nullptr) {
        std::fclose(out);
      }
      closedir(dir);
      return ESP_FAIL;
    }

    size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), in)) > 0) {
      if (std::fwrite(buffer, 1, read, out) != read) {
        std::fclose(in);
        std::fclose(out);
        closedir(dir);
        return ESP_FAIL;
      }
    }
    std::fclose(in);
    std::fclose(out);
  }
  closedir(dir);
  ESP_LOGI(TAG, "program copied %s -> %s", source, destination);
  return ESP_OK;
}

}  // namespace esp32calc_alt
