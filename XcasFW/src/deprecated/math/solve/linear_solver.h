#pragma once

// Deprecated custom fallback solver API. Move back to src/math/solve only if a
// no-Giac linear solving path is restored, and include it from math_engine.

#include <cstddef>
#include <cstdint>

#include "math/types.h"

namespace esp32calc_alt::solve {

enum class LinearStatus : uint8_t {
  Ok,
  Empty,
  BadNumber,
  MissingNumber,
  MissingEquals,
  MissingClosingParen,
  DivideByZero,
  UnsupportedOperator,
  UnsupportedVariable,
  NonLinear,
  TooManyEquations,
  ResultTooLong,
};

struct LinearExpr {
  double constant = 0.0;
  double coeff[kSolveVariableCount] {};
};

LinearStatus parse_linear_expression(const char* expr,
                                     const SolveOptions& options,
                                     LinearExpr& output);
LinearStatus solve_linear_expression(const char* expr,
                                     const SolveOptions& options,
                                     char* output,
                                     size_t output_size);
LinearStatus solve_linear_system_text(const char* expr,
                                      const SolveOptions& options,
                                      char* output,
                                      size_t output_size);
const char* linear_error_text(LinearStatus status);

}  // namespace esp32calc_alt::solve
