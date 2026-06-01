#pragma once

#include <cstdint>

namespace esp32calc_alt::core {

enum class NumericStatus : uint8_t {
  Ok,
  Empty,
  BadNumber,
  MissingNumber,
  MissingClosingParen,
  DivideByZero,
  UnsupportedOperator,
};

NumericStatus evaluate_numeric_expression(const char* expr, double& value);
const char* numeric_error_text(NumericStatus status);

}  // namespace esp32calc_alt::core

