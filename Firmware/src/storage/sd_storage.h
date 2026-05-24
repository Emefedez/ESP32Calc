#pragma once

#include "esp_err.h"

namespace esp32calc {

class SdStorage {
 public:
  esp_err_t mount();
  bool mounted() const { return mounted_; }

 private:
  bool mounted_ = false;
};

}  // namespace esp32calc

