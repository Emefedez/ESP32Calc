#include "math/core/rational_eval.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace esp32calc_alt::core {
namespace {

enum class RationalStatus : uint8_t {
  Ok,
  Empty,
  BadNumber,
  MissingNumber,
  MissingClosingParen,
  DivideByZero,
  UnsupportedOperator,
};

struct Rational {
  int64_t num = 0;
  int64_t den = 1;
  bool valid = true;
};

void skip_spaces(const char*& p) {
  while (std::isspace(static_cast<unsigned char>(*p))) {
    ++p;
  }
}

int64_t gcd_i64(int64_t a, int64_t b) {
  a = a < 0 ? -a : a;
  b = b < 0 ? -b : b;
  while (b != 0) {
    const int64_t r = a % b;
    a = b;
    b = r;
  }
  return a == 0 ? 1 : a;
}

Rational make_rational(int64_t num, int64_t den = 1) {
  Rational out {};
  if (den == 0) {
    out.valid = false;
    return out;
  }
  if (den < 0) {
    num = -num;
    den = -den;
  }
  const int64_t gcd = gcd_i64(num, den);
  out.num = num / gcd;
  out.den = den / gcd;
  return out;
}

Rational add_rational(Rational lhs, Rational rhs, int sign = 1) {
  if (!lhs.valid || !rhs.valid) {
    return {0, 1, false};
  }
  return make_rational(lhs.num * rhs.den + sign * rhs.num * lhs.den, lhs.den * rhs.den);
}

Rational multiply_rational(Rational lhs, Rational rhs) {
  if (!lhs.valid || !rhs.valid) {
    return {0, 1, false};
  }
  return make_rational(lhs.num * rhs.num, lhs.den * rhs.den);
}

Rational divide_rational(Rational lhs, Rational rhs) {
  if (!lhs.valid || !rhs.valid || rhs.num == 0) {
    return {0, 1, false};
  }
  return make_rational(lhs.num * rhs.den, lhs.den * rhs.num);
}

Rational pow_rational(Rational base, int exponent) {
  if (!base.valid) {
    return base;
  }
  Rational out = make_rational(1);
  int count = exponent < 0 ? -exponent : exponent;
  while (count-- > 0) {
    out = multiply_rational(out, base);
    if (!out.valid) {
      return out;
    }
  }
  if (exponent < 0) {
    out = divide_rational(make_rational(1), out);
  }
  return out;
}

struct RationalParser {
  const char* cursor = nullptr;

  RationalStatus evaluate(const char* expr, Rational& value) {
    if (expr == nullptr) {
      return RationalStatus::Empty;
    }
    cursor = expr;
    skip_spaces(cursor);
    if (*cursor == '\0') {
      return RationalStatus::Empty;
    }

    RationalStatus status = parse_sum(value);
    if (status != RationalStatus::Ok) {
      return status;
    }
    skip_spaces(cursor);
    return *cursor == '\0' ? RationalStatus::Ok : RationalStatus::UnsupportedOperator;
  }

  RationalStatus parse_sum(Rational& value) {
    RationalStatus status = parse_product(value);
    if (status != RationalStatus::Ok) {
      return status;
    }
    while (true) {
      skip_spaces(cursor);
      const char op = *cursor;
      if (op != '+' && op != '-') {
        return RationalStatus::Ok;
      }
      ++cursor;
      Rational rhs {};
      status = parse_product(rhs);
      if (status != RationalStatus::Ok) {
        return status;
      }
      value = add_rational(value, rhs, op == '+' ? 1 : -1);
      if (!value.valid) {
        return RationalStatus::BadNumber;
      }
    }
  }

  RationalStatus parse_product(Rational& value) {
    RationalStatus status = parse_unary(value);
    if (status != RationalStatus::Ok) {
      return status;
    }
    while (true) {
      skip_spaces(cursor);
      const char op = *cursor;
      if (op != '*' && op != '/') {
        return RationalStatus::Ok;
      }
      ++cursor;
      Rational rhs {};
      status = parse_unary(rhs);
      if (status != RationalStatus::Ok) {
        return status;
      }
      value = op == '*' ? multiply_rational(value, rhs) : divide_rational(value, rhs);
      if (!value.valid) {
        return op == '/' ? RationalStatus::DivideByZero : RationalStatus::BadNumber;
      }
    }
  }

  RationalStatus parse_unary(Rational& value) {
    skip_spaces(cursor);
    if (*cursor == '+') {
      ++cursor;
      return parse_unary(value);
    }
    if (*cursor == '-') {
      ++cursor;
      const RationalStatus status = parse_unary(value);
      if (status == RationalStatus::Ok) {
        value.num = -value.num;
      }
      return status;
    }
    return parse_power(value);
  }

  RationalStatus parse_power(Rational& value) {
    RationalStatus status = parse_primary(value);
    if (status != RationalStatus::Ok) {
      return status;
    }
    skip_spaces(cursor);
    if (*cursor != '^') {
      return RationalStatus::Ok;
    }
    ++cursor;
    Rational exponent {};
    status = parse_unary(exponent);
    if (status != RationalStatus::Ok) {
      return status;
    }
    if (exponent.den != 1 || exponent.num < -16 || exponent.num > 16) {
      return RationalStatus::UnsupportedOperator;
    }
    value = pow_rational(value, static_cast<int>(exponent.num));
    return value.valid ? RationalStatus::Ok : RationalStatus::BadNumber;
  }

  RationalStatus parse_primary(Rational& value) {
    skip_spaces(cursor);
    if (*cursor == '\0') {
      return RationalStatus::MissingNumber;
    }
    if (*cursor == '(') {
      ++cursor;
      const RationalStatus status = parse_sum(value);
      if (status != RationalStatus::Ok) {
        return status;
      }
      skip_spaces(cursor);
      if (*cursor != ')') {
        return RationalStatus::MissingClosingParen;
      }
      ++cursor;
      return RationalStatus::Ok;
    }

    const char* start = cursor;
    int64_t integer = 0;
    int64_t decimal = 0;
    int64_t scale = 1;
    bool saw_digit = false;
    while (std::isdigit(static_cast<unsigned char>(*cursor))) {
      saw_digit = true;
      integer = integer * 10 + (*cursor - '0');
      ++cursor;
    }
    if (*cursor == '.') {
      ++cursor;
      while (std::isdigit(static_cast<unsigned char>(*cursor))) {
        saw_digit = true;
        decimal = decimal * 10 + (*cursor - '0');
        scale *= 10;
        ++cursor;
      }
    }
    if (!saw_digit) {
      cursor = start;
      return RationalStatus::BadNumber;
    }
    if (*cursor == 'e' || *cursor == 'E') {
      cursor = start;
      return RationalStatus::UnsupportedOperator;
    }
    value = make_rational(integer * scale + decimal, scale);
    return value.valid ? RationalStatus::Ok : RationalStatus::BadNumber;
  }
};

RationalStatus evaluate_rational_expression(const char* expr, Rational& value) {
  RationalParser parser {};
  return parser.evaluate(expr, value);
}

}  // namespace

bool try_rational_result(const char* expr, char* output, size_t output_size) {
  if (expr == nullptr || output == nullptr || output_size == 0 || std::strchr(expr, '/') == nullptr) {
    return false;
  }

  Rational rational {};
  if (evaluate_rational_expression(expr, rational) != RationalStatus::Ok ||
      !rational.valid || rational.den == 1) {
    return false;
  }

  std::snprintf(output,
                output_size,
                "%lld/%lld",
                static_cast<long long>(rational.num),
                static_cast<long long>(rational.den));
  return true;
}

}  // namespace esp32calc_alt::core

