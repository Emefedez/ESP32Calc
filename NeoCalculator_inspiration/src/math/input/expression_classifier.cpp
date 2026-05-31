#include "math/input/expression_classifier.h"

#include <cctype>

namespace esp32calc_alt::input {
namespace {

bool is_numeric_exponent_marker(const char* expr, const char* p) {
  if (*p != 'E' && *p != 'e') {
    return false;
  }
  if (p == expr) {
    return false;
  }

  const unsigned char previous = static_cast<unsigned char>(*(p - 1));
  return std::isdigit(previous) || *(p - 1) == '.';
}

}  // namespace

ExpressionKind classify_expression(const char* expr) {
  if (expr == nullptr || expr[0] == '\0') {
    return ExpressionKind::Invalid;
  }

  bool has_variable = false;
  for (const char* p = expr; *p != '\0'; ++p) {
    if (*p == '=') {
      return ExpressionKind::Equation;
    }
    if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')) {
      if (is_numeric_exponent_marker(expr, p)) {
        continue;
      }
      has_variable = true;
    }
    if (std::iscntrl(static_cast<unsigned char>(*p))) {
      return ExpressionKind::Invalid;
    }
  }

  return has_variable ? ExpressionKind::Symbolic : ExpressionKind::Numeric;
}

const char* expression_kind_label(ExpressionKind kind) {
  switch (kind) {
    case ExpressionKind::Numeric:
      return "NUMERIC";
    case ExpressionKind::Symbolic:
      return "SYMBOLIC";
    case ExpressionKind::Equation:
      return "EQUATION";
    case ExpressionKind::Invalid:
    default:
      return "INVALID";
  }
}

}  // namespace esp32calc_alt::input

