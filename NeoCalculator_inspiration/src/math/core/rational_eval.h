#pragma once

#include <cstddef>

namespace esp32calc_alt::core {

bool try_rational_result(const char* expr, char* output, size_t output_size);

}  // namespace esp32calc_alt::core

