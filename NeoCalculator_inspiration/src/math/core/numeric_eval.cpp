#include "math/core/numeric_eval.h"

#include <cctype>
#include <cmath>
#include <cstdlib>

namespace esp32calc_alt::core {
namespace {

void skip_spaces(const char*& p) {
  while (std::isspace(static_cast<unsigned char>(*p))) {
    ++p;
  }
}

struct NumericParser {
  const char* cursor = nullptr;

  NumericStatus evaluate(const char* expr, double& value) {
    if (expr == nullptr) {
      return NumericStatus::Empty;
    }

    cursor = expr;
    skip_spaces(cursor);
    if (*cursor == '\0') {
      return NumericStatus::Empty;
    }

    const NumericStatus status = parse_sum(value);
    if (status != NumericStatus::Ok) {
      return status;
    }

    skip_spaces(cursor);
    return *cursor == '\0' ? NumericStatus::Ok : NumericStatus::UnsupportedOperator;
  }

  NumericStatus parse_sum(double& value) {
    NumericStatus status = parse_product(value);
    if (status != NumericStatus::Ok) {
      return status;
    }

    while (true) {
      skip_spaces(cursor);
      const char op = *cursor;
      if (op != '+' && op != '-') {
        return NumericStatus::Ok;
      }
      ++cursor;

      double rhs = 0.0;
      status = parse_product(rhs);
      if (status != NumericStatus::Ok) {
        return status;
      }

      value = op == '+' ? value + rhs : value - rhs;
      if (!std::isfinite(value)) {
        return NumericStatus::BadNumber;
      }
    }
  }

  NumericStatus parse_product(double& value) {
    NumericStatus status = parse_unary(value);
    if (status != NumericStatus::Ok) {
      return status;
    }

    while (true) {
      skip_spaces(cursor);
      const char op = *cursor;
      if (op != '*' && op != '/') {
        return NumericStatus::Ok;
      }
      ++cursor;

      double rhs = 0.0;
      status = parse_unary(rhs);
      if (status != NumericStatus::Ok) {
        return status;
      }

      if (op == '/') {
        if (rhs == 0.0) {
          return NumericStatus::DivideByZero;
        }
        value /= rhs;
      } else {
        value *= rhs;
      }

      if (!std::isfinite(value)) {
        return NumericStatus::BadNumber;
      }
    }
  }

  NumericStatus parse_unary(double& value) {
    skip_spaces(cursor);
    if (*cursor == '\0') {
      return NumericStatus::MissingNumber;
    }
    if (*cursor == '+') {
      ++cursor;
      return parse_unary(value);
    }
    if (*cursor == '-') {
      ++cursor;
      const NumericStatus status = parse_unary(value);
      if (status == NumericStatus::Ok) {
        value = -value;
      }
      return status;
    }
    return parse_power(value);
  }

  NumericStatus parse_power(double& value) {
    NumericStatus status = parse_primary(value);
    if (status != NumericStatus::Ok) {
      return status;
    }

    skip_spaces(cursor);
    if (*cursor != '^') {
      return NumericStatus::Ok;
    }
    ++cursor;

    double exponent = 0.0;
    status = parse_unary(exponent);
    if (status != NumericStatus::Ok) {
      return status;
    }

    value = std::pow(value, exponent);
    return std::isfinite(value) ? NumericStatus::Ok : NumericStatus::BadNumber;
  }

  NumericStatus parse_primary(double& value) {
    skip_spaces(cursor);
    if (*cursor == '\0') {
      return NumericStatus::MissingNumber;
    }
    if (*cursor == '(') {
      ++cursor;
      const NumericStatus status = parse_sum(value);
      if (status != NumericStatus::Ok) {
        return status;
      }

      skip_spaces(cursor);
      if (*cursor != ')') {
        return NumericStatus::MissingClosingParen;
      }
      ++cursor;
      return NumericStatus::Ok;
    }

    char* end = nullptr;
    value = std::strtod(cursor, &end);
    if (end == cursor) {
      return NumericStatus::BadNumber;
    }
    if (!std::isfinite(value)) {
      return NumericStatus::BadNumber;
    }

    cursor = end;
    return NumericStatus::Ok;
  }
};

}  // namespace

NumericStatus evaluate_numeric_expression(const char* expr, double& value) {
  NumericParser parser {};
  return parser.evaluate(expr, value);
}

const char* numeric_error_text(NumericStatus status) {
  switch (status) {
    case NumericStatus::Empty:
      return "EMPTY EXPRESSION";
    case NumericStatus::MissingClosingParen:
      return "MISSING )";
    case NumericStatus::DivideByZero:
      return "DIV BY ZERO";
    case NumericStatus::UnsupportedOperator:
      return "ONLY +-*/^() READY";
    case NumericStatus::BadNumber:
    case NumericStatus::MissingNumber:
    default:
      return "NUMERIC ERROR";
  }
}

}  // namespace esp32calc_alt::core

