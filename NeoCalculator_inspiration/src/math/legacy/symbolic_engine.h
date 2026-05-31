#pragma once

#include <cstddef>
#include <cstdint>

namespace esp32calc_alt::symbolic {

enum class ResultKind : uint8_t {
  Unhandled,
  Value,
  Error,
};

struct TextResult {
  ResultKind kind = ResultKind::Unhandled;
  char text[192] {};

  bool handled() const { return kind != ResultKind::Unhandled; }
  bool is_error() const { return kind == ResultKind::Error; }
};

TextResult try_rational_result(const char* expr);
TextResult try_symbolic_result(const char* expr);
TextResult try_polynomial_equation_result(const char* expr);
TextResult try_matrix_result(const char* expr);

}  // namespace esp32calc_alt::symbolic
