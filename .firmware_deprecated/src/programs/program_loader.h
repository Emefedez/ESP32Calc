#pragma once

#include <string>
#include <vector>

#include "esp_err.h"

namespace esp32calc {

class ProgramLoader {
 public:
  esp_err_t list_programs(std::vector<std::string>& out) const;
};

}  // namespace esp32calc

