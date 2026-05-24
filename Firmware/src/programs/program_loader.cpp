#include "programs/program_loader.h"

#include <dirent.h>

#include "app_config.h"
#include "esp_log.h"

namespace esp32calc {
namespace {
constexpr const char* TAG = "programs";
}

esp_err_t ProgramLoader::list_programs(std::vector<std::string>& out) const {
  out.clear();
  DIR* dir = opendir(config::kProgramsPath);
  if (dir == nullptr) {
    ESP_LOGW(TAG, "program directory unavailable: %s", config::kProgramsPath);
    return ESP_ERR_NOT_FOUND;
  }

  while (dirent* entry = readdir(dir)) {
    if (entry->d_name[0] != '.') {
      out.emplace_back(entry->d_name);
    }
  }
  closedir(dir);
  return ESP_OK;
}

}  // namespace esp32calc

