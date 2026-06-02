#include "calc/symbolic_engine.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

namespace esp32calc::symbolic {
namespace {

constexpr double kEpsilon = 0.000000001;

enum class ParseStatus : uint8_t {
  Ok,
  Empty,
  BadNumber,
  MissingNumber,
  MissingClosingParen,
  DivideByZero,
  UnsupportedOperator,
  UnsupportedVariable,
  NonPolynomial,
};

enum class RationalParseStatus : uint8_t {
  Ok,
  Empty,
  BadNumber,
  MissingNumber,
  MissingClosingParen,
  DivideByZero,
  UnsupportedOperator,
};

TextResult unhandled() {
  return {};
}

TextResult value_result(const char* text) {
  TextResult out {};
  out.kind = ResultKind::Value;
  std::snprintf(out.text, sizeof(out.text), "%s", text);
  return out;
}

TextResult error_result(const char* text) {
  TextResult out {};
  out.kind = ResultKind::Error;
  std::snprintf(out.text, sizeof(out.text), "%s", text);
  return out;
}

const char* parse_error_text(ParseStatus status) {
  switch (status) {
    case ParseStatus::Empty:
      return "EMPTY EXPRESSION";
    case ParseStatus::MissingClosingParen:
      return "MISSING )";
    case ParseStatus::DivideByZero:
      return "DIV BY ZERO";
    case ParseStatus::UnsupportedVariable:
      return "BAD VARIABLE";
    case ParseStatus::NonPolynomial:
      return "NONPOLY";
    case ParseStatus::UnsupportedOperator:
      return "UNSUPPORTED OP";
    case ParseStatus::BadNumber:
    case ParseStatus::MissingNumber:
    default:
      return "SYMBOLIC ERROR";
  }
}

void skip_spaces(const char*& p) {
  while (std::isspace(static_cast<unsigned char>(*p))) {
    ++p;
  }
}

bool nearly_zero(double value) {
  return std::fabs(value) < kEpsilon;
}

void clean_near_zero(double& value) {
  if (nearly_zero(value)) {
    value = 0.0;
  }
}

bool append_format(char* output, size_t output_size, size_t& used, const char* fmt, ...) {
  if (used >= output_size) {
    return false;
  }

  va_list args;
  va_start(args, fmt);
  const int written = std::vsnprintf(output + used, output_size - used, fmt, args);
  va_end(args);

  if (written < 0) {
    return false;
  }
  if (static_cast<size_t>(written) >= output_size - used) {
    used = output_size - 1;
    return false;
  }
  used += static_cast<size_t>(written);
  return true;
}

void format_number(double value, char* output, size_t output_size) {
  clean_near_zero(value);
  std::snprintf(output, output_size, "%.10g", value);
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

struct Rational {
  int64_t num = 0;
  int64_t den = 1;
  bool valid = true;
};

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
  if (!lhs.valid || !rhs.valid) return {0, 1, false};
  return make_rational(lhs.num * rhs.den + sign * rhs.num * lhs.den, lhs.den * rhs.den);
}

Rational multiply_rational(Rational lhs, Rational rhs) {
  if (!lhs.valid || !rhs.valid) return {0, 1, false};
  return make_rational(lhs.num * rhs.num, lhs.den * rhs.den);
}

Rational divide_rational(Rational lhs, Rational rhs) {
  if (!lhs.valid || !rhs.valid || rhs.num == 0) return {0, 1, false};
  return make_rational(lhs.num * rhs.den, lhs.den * rhs.num);
}

Rational pow_rational(Rational base, int exponent) {
  if (!base.valid) return base;
  Rational out = make_rational(1);
  int count = exponent < 0 ? -exponent : exponent;
  while (count-- > 0) {
    out = multiply_rational(out, base);
    if (!out.valid) return out;
  }
  if (exponent < 0) {
    out = divide_rational(make_rational(1), out);
  }
  return out;
}

struct RationalParser {
  const char* cursor = nullptr;

  RationalParseStatus evaluate(const char* expr, Rational& value) {
    if (expr == nullptr) return RationalParseStatus::Empty;
    cursor = expr;
    skip_spaces(cursor);
    if (*cursor == '\0') return RationalParseStatus::Empty;

    RationalParseStatus status = parse_sum(value);
    if (status != RationalParseStatus::Ok) return status;
    skip_spaces(cursor);
    return *cursor == '\0' ? RationalParseStatus::Ok
                           : RationalParseStatus::UnsupportedOperator;
  }

  RationalParseStatus parse_sum(Rational& value) {
    RationalParseStatus status = parse_product(value);
    if (status != RationalParseStatus::Ok) return status;
    while (true) {
      skip_spaces(cursor);
      const char op = *cursor;
      if (op != '+' && op != '-') return RationalParseStatus::Ok;
      ++cursor;
      Rational rhs {};
      status = parse_product(rhs);
      if (status != RationalParseStatus::Ok) return status;
      value = add_rational(value, rhs, op == '+' ? 1 : -1);
      if (!value.valid) return RationalParseStatus::BadNumber;
    }
  }

  RationalParseStatus parse_product(Rational& value) {
    RationalParseStatus status = parse_unary(value);
    if (status != RationalParseStatus::Ok) return status;
    while (true) {
      skip_spaces(cursor);
      const char op = *cursor;
      if (op != '*' && op != '/') return RationalParseStatus::Ok;
      ++cursor;
      Rational rhs {};
      status = parse_unary(rhs);
      if (status != RationalParseStatus::Ok) return status;
      value = op == '*' ? multiply_rational(value, rhs) : divide_rational(value, rhs);
      if (!value.valid) {
        return op == '/' ? RationalParseStatus::DivideByZero : RationalParseStatus::BadNumber;
      }
    }
  }

  RationalParseStatus parse_unary(Rational& value) {
    skip_spaces(cursor);
    if (*cursor == '+') {
      ++cursor;
      return parse_unary(value);
    }
    if (*cursor == '-') {
      ++cursor;
      const RationalParseStatus status = parse_unary(value);
      if (status == RationalParseStatus::Ok) value.num = -value.num;
      return status;
    }
    return parse_power(value);
  }

  RationalParseStatus parse_power(Rational& value) {
    RationalParseStatus status = parse_primary(value);
    if (status != RationalParseStatus::Ok) return status;
    skip_spaces(cursor);
    if (*cursor != '^') return RationalParseStatus::Ok;
    ++cursor;
    Rational exponent {};
    status = parse_unary(exponent);
    if (status != RationalParseStatus::Ok) return status;
    if (exponent.den != 1 || exponent.num < -16 || exponent.num > 16) {
      return RationalParseStatus::UnsupportedOperator;
    }
    value = pow_rational(value, static_cast<int>(exponent.num));
    return value.valid ? RationalParseStatus::Ok : RationalParseStatus::BadNumber;
  }

  RationalParseStatus parse_primary(Rational& value) {
    skip_spaces(cursor);
    if (*cursor == '(') {
      ++cursor;
      const RationalParseStatus status = parse_sum(value);
      if (status != RationalParseStatus::Ok) return status;
      skip_spaces(cursor);
      if (*cursor != ')') return RationalParseStatus::MissingClosingParen;
      ++cursor;
      return RationalParseStatus::Ok;
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
      return RationalParseStatus::BadNumber;
    }
    if (*cursor == 'e' || *cursor == 'E') {
      cursor = start;
      return RationalParseStatus::UnsupportedOperator;
    }
    value = make_rational(integer * scale + decimal, scale);
    return value.valid ? RationalParseStatus::Ok : RationalParseStatus::BadNumber;
  }
};

RationalParseStatus evaluate_rational_expression(const char* expr, Rational& value) {
  RationalParser parser {};
  return parser.evaluate(expr, value);
}

struct PolyTerm {
  int exponent = 0;
  double coefficient = 0.0;
};

struct Polynomial {
  std::vector<PolyTerm> terms {};

  bool add_term(int exponent, double coefficient) {
    if (!std::isfinite(coefficient)) return false;
    if (nearly_zero(coefficient)) return true;
    for (PolyTerm& term : terms) {
      if (term.exponent == exponent) {
        term.coefficient += coefficient;
        clean_near_zero(term.coefficient);
        return true;
      }
    }
    terms.push_back({exponent, coefficient});
    return true;
  }

  bool is_constant() const {
    for (const PolyTerm& term : terms) {
      if (term.exponent != 0 && !nearly_zero(term.coefficient)) return false;
    }
    return true;
  }

  double constant_value() const {
    double value = 0.0;
    for (const PolyTerm& term : terms) {
      if (term.exponent == 0) value += term.coefficient;
    }
    clean_near_zero(value);
    return value;
  }

  int degree() const {
    int degree = std::numeric_limits<int>::min();
    for (const PolyTerm& term : terms) {
      if (!nearly_zero(term.coefficient)) degree = std::max(degree, term.exponent);
    }
    return degree == std::numeric_limits<int>::min() ? 0 : degree;
  }

  double coeff_for(int exponent) const {
    double value = 0.0;
    for (const PolyTerm& term : terms) {
      if (term.exponent == exponent) value += term.coefficient;
    }
    clean_near_zero(value);
    return value;
  }

  void sort_terms() {
    terms.erase(std::remove_if(terms.begin(),
                               terms.end(),
                               [](const PolyTerm& term) {
                                 return nearly_zero(term.coefficient);
                               }),
                terms.end());
    std::sort(terms.begin(), terms.end(), [](const PolyTerm& lhs, const PolyTerm& rhs) {
      return lhs.exponent > rhs.exponent;
    });
  }
};

Polynomial make_poly_constant(double value) {
  Polynomial out {};
  out.add_term(0, value);
  return out;
}

Polynomial make_poly_x() {
  Polynomial out {};
  out.add_term(1, 1.0);
  return out;
}

bool add_poly(Polynomial& lhs, const Polynomial& rhs, double sign) {
  for (const PolyTerm& term : rhs.terms) {
    if (!lhs.add_term(term.exponent, sign * term.coefficient)) return false;
  }
  return true;
}

bool multiply_poly(const Polynomial& lhs, const Polynomial& rhs, Polynomial& out) {
  out = {};
  for (const PolyTerm& a : lhs.terms) {
    for (const PolyTerm& b : rhs.terms) {
      if (!out.add_term(a.exponent + b.exponent, a.coefficient * b.coefficient)) return false;
    }
  }
  return true;
}

bool scale_poly(Polynomial& poly, double factor) {
  for (PolyTerm& term : poly.terms) {
    term.coefficient *= factor;
    if (!std::isfinite(term.coefficient)) return false;
    clean_near_zero(term.coefficient);
  }
  return true;
}

bool pow_poly(Polynomial base, int exponent, Polynomial& out) {
  out = make_poly_constant(1.0);
  if (exponent == 0) return true;
  if (exponent < 0) {
    base.sort_terms();
    if (base.terms.size() != 1 || nearly_zero(base.terms[0].coefficient)) return false;
    out.add_term(base.terms[0].exponent * exponent,
                 std::pow(base.terms[0].coefficient, static_cast<double>(exponent)));
    return std::isfinite(out.terms.empty() ? 0.0 : out.terms[0].coefficient);
  }

  for (int i = 0; i < exponent; ++i) {
    Polynomial next {};
    if (!multiply_poly(out, base, next)) return false;
    out = next;
  }
  return true;
}

struct PolynomialParser {
  const char* cursor = nullptr;

  explicit PolynomialParser(const char* expr) : cursor(expr) {}

  ParseStatus parse(Polynomial& poly) {
    skip_spaces(cursor);
    if (*cursor == '\0') return ParseStatus::Empty;
    ParseStatus status = parse_sum(poly);
    if (status != ParseStatus::Ok) return status;
    skip_spaces(cursor);
    return *cursor == '\0' ? ParseStatus::Ok : ParseStatus::UnsupportedOperator;
  }

  ParseStatus parse_sum(Polynomial& poly) {
    ParseStatus status = parse_product(poly);
    if (status != ParseStatus::Ok) return status;
    while (true) {
      skip_spaces(cursor);
      const char op = *cursor;
      if (op != '+' && op != '-') return ParseStatus::Ok;
      ++cursor;
      Polynomial rhs {};
      status = parse_product(rhs);
      if (status != ParseStatus::Ok) return status;
      if (!add_poly(poly, rhs, op == '+' ? 1.0 : -1.0)) return ParseStatus::BadNumber;
    }
  }

  bool starts_primary() const {
    const char* p = cursor;
    skip_spaces(p);
    const unsigned char c = static_cast<unsigned char>(*p);
    return *p == '(' || *p == '.' || std::isdigit(c) || std::isalpha(c);
  }

  ParseStatus parse_product(Polynomial& poly) {
    ParseStatus status = parse_unary(poly);
    if (status != ParseStatus::Ok) return status;
    while (true) {
      skip_spaces(cursor);
      const char op = *cursor;
      if (op == '*' || op == '/') {
        ++cursor;
        Polynomial rhs {};
        status = parse_unary(rhs);
        if (status != ParseStatus::Ok) return status;
        if (op == '/') {
          if (!rhs.is_constant()) return ParseStatus::NonPolynomial;
          const double divisor = rhs.constant_value();
          if (nearly_zero(divisor)) return ParseStatus::DivideByZero;
          if (!scale_poly(poly, 1.0 / divisor)) return ParseStatus::BadNumber;
        } else {
          Polynomial combined {};
          if (!multiply_poly(poly, rhs, combined)) return ParseStatus::BadNumber;
          poly = combined;
        }
      } else if (starts_primary()) {
        Polynomial rhs {};
        status = parse_unary(rhs);
        if (status != ParseStatus::Ok) return status;
        Polynomial combined {};
        if (!multiply_poly(poly, rhs, combined)) return ParseStatus::BadNumber;
        poly = combined;
      } else {
        return ParseStatus::Ok;
      }
    }
  }

  ParseStatus parse_unary(Polynomial& poly) {
    skip_spaces(cursor);
    if (*cursor == '+') {
      ++cursor;
      return parse_unary(poly);
    }
    if (*cursor == '-') {
      ++cursor;
      const ParseStatus status = parse_unary(poly);
      if (status == ParseStatus::Ok && !scale_poly(poly, -1.0)) {
        return ParseStatus::BadNumber;
      }
      return status;
    }
    return parse_power(poly);
  }

  ParseStatus parse_power(Polynomial& poly) {
    ParseStatus status = parse_primary(poly);
    if (status != ParseStatus::Ok) return status;
    skip_spaces(cursor);
    if (*cursor != '^') return ParseStatus::Ok;
    ++cursor;

    Polynomial exponent_poly {};
    status = parse_unary(exponent_poly);
    if (status != ParseStatus::Ok) return status;
    if (!exponent_poly.is_constant()) return ParseStatus::NonPolynomial;
    const double exponent_value = exponent_poly.constant_value();
    const double rounded = std::round(exponent_value);
    if (std::fabs(exponent_value - rounded) > kEpsilon || rounded < -32.0 ||
        rounded > 32.0) {
      return ParseStatus::NonPolynomial;
    }

    Polynomial powered {};
    if (!pow_poly(poly, static_cast<int>(rounded), powered)) return ParseStatus::NonPolynomial;
    poly = powered;
    return ParseStatus::Ok;
  }

  ParseStatus parse_primary(Polynomial& poly) {
    skip_spaces(cursor);
    if (*cursor == '(') {
      ++cursor;
      const ParseStatus status = parse_sum(poly);
      if (status != ParseStatus::Ok) return status;
      skip_spaces(cursor);
      if (*cursor != ')') return ParseStatus::MissingClosingParen;
      ++cursor;
      return ParseStatus::Ok;
    }

    const unsigned char current = static_cast<unsigned char>(*cursor);
    if (std::isalpha(current)) {
      char identifier[8] {};
      size_t used = 0;
      while (std::isalpha(static_cast<unsigned char>(*cursor))) {
        if (used + 1 < sizeof(identifier)) {
          identifier[used++] =
              static_cast<char>(std::tolower(static_cast<unsigned char>(*cursor)));
        }
        ++cursor;
      }
      identifier[used] = '\0';
      if (std::strcmp(identifier, "x") != 0) return ParseStatus::UnsupportedVariable;
      poly = make_poly_x();
      return ParseStatus::Ok;
    }

    char* end = nullptr;
    const double value = std::strtod(cursor, &end);
    if (end == cursor || !std::isfinite(value)) return ParseStatus::BadNumber;
    cursor = end;
    poly = make_poly_constant(value);
    return ParseStatus::Ok;
  }
};

ParseStatus parse_polynomial(const char* expr, Polynomial& poly) {
  PolynomialParser parser(expr);
  ParseStatus status = parser.parse(poly);
  if (status == ParseStatus::Ok) poly.sort_terms();
  return status;
}

bool approximate_fraction(double value, int64_t& numerator, int64_t& denominator) {
  const double abs_value = std::fabs(value);
  if (!std::isfinite(value) || abs_value > 1000000000.0) return false;
  constexpr int64_t kMaxDenominator = 10000;
  int64_t best_num = static_cast<int64_t>(std::llround(value));
  int64_t best_den = 1;
  double best_error = std::fabs(value - static_cast<double>(best_num));
  for (int64_t den = 2; den <= kMaxDenominator; ++den) {
    const int64_t num = static_cast<int64_t>(std::llround(value * static_cast<double>(den)));
    const double error = std::fabs(value - static_cast<double>(num) / static_cast<double>(den));
    if (error < best_error) {
      best_error = error;
      best_num = num;
      best_den = den;
      if (error < 1e-12) break;
    }
  }
  if (best_error > 1e-9) return false;
  const int64_t gcd = gcd_i64(best_num, best_den);
  numerator = best_num / gcd;
  denominator = best_den / gcd;
  return true;
}

bool append_poly_term(char* output,
                      size_t output_size,
                      size_t& used,
                      const PolyTerm& term,
                      bool& wrote_any) {
  if (nearly_zero(term.coefficient)) return true;
  const bool negative = term.coefficient < 0.0;
  const double magnitude = std::fabs(term.coefficient);
  if (!wrote_any) {
    if (negative && !append_format(output, output_size, used, "-")) return false;
  } else if (!append_format(output, output_size, used, negative ? "-" : "+")) {
    return false;
  }

  int64_t frac_num = 0;
  int64_t frac_den = 1;
  const bool has_fraction =
      approximate_fraction(magnitude, frac_num, frac_den) && frac_den != 1;
  const bool has_variable = term.exponent != 0;
  if (has_fraction && has_variable) {
    if (frac_num != 1 && !append_format(output,
                                        output_size,
                                        used,
                                        "%lld",
                                        static_cast<long long>(frac_num))) {
      return false;
    }
    if (!append_format(output, output_size, used, "x")) return false;
    if (term.exponent != 1 && !append_format(output, output_size, used, "^%d", term.exponent)) {
      return false;
    }
    return append_format(output, output_size, used, "/%lld", static_cast<long long>(frac_den));
  }

  if (has_fraction && !has_variable) {
    return append_format(output,
                         output_size,
                         used,
                         "%lld/%lld",
                         static_cast<long long>(frac_num),
                         static_cast<long long>(frac_den));
  }

  if (!has_variable || std::fabs(magnitude - 1.0) >= kEpsilon) {
    char number[24] {};
    format_number(magnitude, number, sizeof(number));
    if (!append_format(output, output_size, used, "%s", number)) return false;
  }
  if (has_variable) {
    if (!append_format(output, output_size, used, "x")) return false;
    if (term.exponent != 1 && !append_format(output, output_size, used, "^%d", term.exponent)) {
      return false;
    }
  }
  wrote_any = true;
  return true;
}

bool format_polynomial(Polynomial poly, char* output, size_t output_size) {
  poly.sort_terms();
  size_t used = 0;
  bool wrote_any = false;
  for (const PolyTerm& term : poly.terms) {
    if (!append_poly_term(output, output_size, used, term, wrote_any)) return false;
    if (!nearly_zero(term.coefficient)) wrote_any = true;
  }
  if (!wrote_any) return append_format(output, output_size, used, "0");
  return true;
}

Polynomial derivative_of(const Polynomial& input) {
  Polynomial out {};
  for (const PolyTerm& term : input.terms) {
    if (term.exponent == 0) continue;
    out.add_term(term.exponent - 1, term.coefficient * static_cast<double>(term.exponent));
  }
  out.sort_terms();
  return out;
}

ParseStatus integrate_polynomial(const Polynomial& input,
                                 Polynomial& output,
                                 bool& needs_log_term) {
  output = {};
  needs_log_term = false;
  for (const PolyTerm& term : input.terms) {
    if (term.exponent == -1) {
      needs_log_term = true;
      continue;
    }
    if (!output.add_term(term.exponent + 1,
                         term.coefficient / static_cast<double>(term.exponent + 1))) {
      return ParseStatus::BadNumber;
    }
  }
  output.sort_terms();
  return ParseStatus::Ok;
}

char* duplicate_range(const char* begin, size_t length) {
  char* out = new (std::nothrow) char[length + 1];
  if (out == nullptr) return nullptr;
  std::memcpy(out, begin, length);
  out[length] = '\0';
  return out;
}

bool starts_with_case_insensitive(const char* text, const char* prefix) {
  while (*prefix != '\0') {
    if (std::tolower(static_cast<unsigned char>(*text)) !=
        std::tolower(static_cast<unsigned char>(*prefix))) {
      return false;
    }
    ++text;
    ++prefix;
  }
  return true;
}

bool extract_call_argument(const char* expr, const char* name, const char*& begin, size_t& length) {
  if (expr == nullptr) return false;
  const char* p = expr;
  skip_spaces(p);
  if (!starts_with_case_insensitive(p, name)) return false;
  p += std::strlen(name);
  skip_spaces(p);
  if (*p != '(') return false;
  ++p;
  begin = p;
  int depth = 1;
  while (*p != '\0') {
    if (*p == '(') {
      ++depth;
    } else if (*p == ')') {
      --depth;
      if (depth == 0) {
        length = static_cast<size_t>(p - begin);
        ++p;
        skip_spaces(p);
        return *p == '\0';
      }
    }
    ++p;
  }
  return false;
}

TextResult make_polynomial_calculus_result(const char* expr) {
  const char* arg_begin = nullptr;
  size_t arg_len = 0;
  bool derivative = false;
  bool integral = false;
  if (extract_call_argument(expr, "d/dx", arg_begin, arg_len) ||
      extract_call_argument(expr, "deriv", arg_begin, arg_len)) {
    derivative = true;
  } else if (extract_call_argument(expr, "int", arg_begin, arg_len) ||
             extract_call_argument(expr, "integral", arg_begin, arg_len)) {
    integral = true;
  } else {
    return unhandled();
  }

  char* argument = duplicate_range(arg_begin, arg_len);
  if (argument == nullptr) return error_result("NO MEMORY");
  Polynomial poly {};
  const ParseStatus status = parse_polynomial(argument, poly);
  delete[] argument;
  if (status != ParseStatus::Ok) return error_result(parse_error_text(status));

  Polynomial result {};
  bool needs_log_term = false;
  if (derivative) {
    result = derivative_of(poly);
  } else if (integral) {
    const ParseStatus int_status = integrate_polynomial(poly, result, needs_log_term);
    if (int_status != ParseStatus::Ok) {
      return error_result(parse_error_text(int_status));
    }
  }

  char text[160] {};
  if (!format_polynomial(result, text, sizeof(text))) {
    return error_result("RESULT TOO LONG");
  }
  if (needs_log_term) {
    size_t used = std::strlen(text);
    if (used == 1 && text[0] == '0') used = 0;
    if (used > 0 && !append_format(text, sizeof(text), used, "+")) {
      return error_result("RESULT TOO LONG");
    }
    if (!append_format(text, sizeof(text), used, "ln(abs(x))")) {
      return error_result("RESULT TOO LONG");
    }
  }
  return value_result(text);
}

struct Matrix {
  size_t rows = 0;
  size_t cols = 0;
  std::vector<double> data {};

  double& at(size_t row, size_t col) { return data[row * cols + col]; }
  double at(size_t row, size_t col) const { return data[row * cols + col]; }
};

bool parse_matrix_body(const char* body, Matrix& matrix) {
  if (body == nullptr) return false;
  const char* cursor = body;
  std::vector<double> values {};
  size_t cols = 0;
  size_t current_cols = 0;
  size_t rows = 0;

  while (true) {
    skip_spaces(cursor);
    if (*cursor == '[' || *cursor == '(') {
      ++cursor;
      continue;
    }
    if (*cursor == '\0' || *cursor == ']' || *cursor == ')') {
      if (current_cols > 0) {
        if (cols == 0) cols = current_cols;
        if (current_cols != cols) return false;
        ++rows;
      }
      break;
    }

    char* end = nullptr;
    const double value = std::strtod(cursor, &end);
    if (end == cursor || !std::isfinite(value)) return false;
    values.push_back(value);
    cursor = end;
    ++current_cols;

    skip_spaces(cursor);
    if (*cursor == ',') {
      ++cursor;
      continue;
    }
    if (*cursor == ';') {
      ++cursor;
      if (cols == 0) cols = current_cols;
      if (current_cols != cols) return false;
      ++rows;
      current_cols = 0;
      continue;
    }
    if (*cursor == '\0' || *cursor == ']' || *cursor == ')') {
      continue;
    }
    return false;
  }

  if (rows == 0 || cols == 0 || values.size() != rows * cols) return false;
  matrix.rows = rows;
  matrix.cols = cols;
  matrix.data = values;
  return true;
}

bool extract_matrix_call(const char* expr, const char* name, Matrix& matrix) {
  const char* begin = nullptr;
  size_t length = 0;
  if (!extract_call_argument(expr, name, begin, length)) return false;
  char* body = duplicate_range(begin, length);
  if (body == nullptr) return false;
  const bool ok = parse_matrix_body(body, matrix);
  delete[] body;
  return ok;
}

bool call_name_present(const char* expr, const char* name) {
  if (expr == nullptr) return false;
  const char* p = expr;
  skip_spaces(p);
  return starts_with_case_insensitive(p, name);
}

bool determinant(Matrix matrix, double& det) {
  if (matrix.rows != matrix.cols || matrix.rows == 0) return false;
  const size_t n = matrix.rows;
  det = 1.0;
  for (size_t col = 0; col < n; ++col) {
    size_t pivot = col;
    double best = std::fabs(matrix.at(col, col));
    for (size_t row = col + 1; row < n; ++row) {
      const double candidate = std::fabs(matrix.at(row, col));
      if (candidate > best) {
        best = candidate;
        pivot = row;
      }
    }
    if (best < kEpsilon) {
      det = 0.0;
      return true;
    }
    if (pivot != col) {
      for (size_t c = 0; c < n; ++c) std::swap(matrix.at(col, c), matrix.at(pivot, c));
      det = -det;
    }
    const double pivot_value = matrix.at(col, col);
    det *= pivot_value;
    for (size_t row = col + 1; row < n; ++row) {
      const double factor = matrix.at(row, col) / pivot_value;
      for (size_t c = col; c < n; ++c) {
        matrix.at(row, c) -= factor * matrix.at(col, c);
        clean_near_zero(matrix.at(row, c));
      }
    }
  }
  clean_near_zero(det);
  return std::isfinite(det);
}

bool invert_matrix(const Matrix& input, Matrix& inverse) {
  if (input.rows != input.cols || input.rows == 0) return false;
  const size_t n = input.rows;
  std::vector<double> aug(n * n * 2, 0.0);
  auto aug_at = [&](size_t row, size_t col) -> double& { return aug[row * (2 * n) + col]; };
  for (size_t row = 0; row < n; ++row) {
    for (size_t col = 0; col < n; ++col) {
      aug_at(row, col) = input.at(row, col);
    }
    aug_at(row, n + row) = 1.0;
  }

  for (size_t col = 0; col < n; ++col) {
    size_t pivot = col;
    double best = std::fabs(aug_at(col, col));
    for (size_t row = col + 1; row < n; ++row) {
      const double candidate = std::fabs(aug_at(row, col));
      if (candidate > best) {
        best = candidate;
        pivot = row;
      }
    }
    if (best < kEpsilon) return false;
    if (pivot != col) {
      for (size_t c = 0; c < 2 * n; ++c) std::swap(aug_at(col, c), aug_at(pivot, c));
    }
    const double divisor = aug_at(col, col);
    for (size_t c = 0; c < 2 * n; ++c) {
      aug_at(col, c) /= divisor;
      clean_near_zero(aug_at(col, c));
    }
    for (size_t row = 0; row < n; ++row) {
      if (row == col || nearly_zero(aug_at(row, col))) continue;
      const double factor = aug_at(row, col);
      for (size_t c = 0; c < 2 * n; ++c) {
        aug_at(row, c) -= factor * aug_at(col, c);
        clean_near_zero(aug_at(row, c));
      }
    }
  }

  inverse.rows = n;
  inverse.cols = n;
  inverse.data.assign(n * n, 0.0);
  for (size_t row = 0; row < n; ++row) {
    for (size_t col = 0; col < n; ++col) {
      inverse.at(row, col) = aug_at(row, n + col);
    }
  }
  return true;
}

bool format_matrix(const Matrix& matrix, char* output, size_t output_size) {
  size_t used = 0;
  if (!append_format(output, output_size, used, "[")) return false;
  for (size_t row = 0; row < matrix.rows; ++row) {
    if (row > 0 && !append_format(output, output_size, used, ";")) return false;
    for (size_t col = 0; col < matrix.cols; ++col) {
      if (col > 0 && !append_format(output, output_size, used, ",")) return false;
      char value[24] {};
      format_number(matrix.at(row, col), value, sizeof(value));
      if (!append_format(output, output_size, used, "%s", value)) return false;
    }
  }
  return append_format(output, output_size, used, "]");
}

}  // namespace

TextResult try_rational_result(const char* expr) {
  if (expr == nullptr || std::strchr(expr, '/') == nullptr) {
    return unhandled();
  }

  Rational rational {};
  if (evaluate_rational_expression(expr, rational) != RationalParseStatus::Ok ||
      !rational.valid || rational.den == 1) {
    return unhandled();
  }

  char text[48] {};
  std::snprintf(text,
                sizeof(text),
                "%lld/%lld",
                static_cast<long long>(rational.num),
                static_cast<long long>(rational.den));
  return value_result(text);
}

TextResult try_symbolic_result(const char* expr) {
  TextResult calculus = make_polynomial_calculus_result(expr);
  if (calculus.handled()) {
    return calculus;
  }

  TextResult matrix = try_matrix_result(expr);
  if (matrix.handled()) {
    return matrix;
  }

  Polynomial poly {};
  const ParseStatus status = parse_polynomial(expr, poly);
  if (status != ParseStatus::Ok) {
    return unhandled();
  }

  char text[128] {};
  if (!format_polynomial(poly, text, sizeof(text))) {
    return error_result("RESULT TOO LONG");
  }
  return value_result(text);
}

TextResult try_polynomial_equation_result(const char* expr) {
  const char* equals = expr == nullptr ? nullptr : std::strchr(expr, '=');
  if (equals == nullptr || std::strchr(equals + 1, '=') != nullptr ||
      std::strchr(expr, ';') != nullptr || std::strchr(expr, ',') != nullptr) {
    return unhandled();
  }

  char* left_text = duplicate_range(expr, static_cast<size_t>(equals - expr));
  char* right_text = duplicate_range(equals + 1, std::strlen(equals + 1));
  if (left_text == nullptr || right_text == nullptr) {
    delete[] left_text;
    delete[] right_text;
    return error_result("NO MEMORY");
  }

  Polynomial left {};
  Polynomial right {};
  ParseStatus status = parse_polynomial(left_text, left);
  if (status == ParseStatus::Ok) status = parse_polynomial(right_text, right);
  delete[] left_text;
  delete[] right_text;
  if (status != ParseStatus::Ok) return unhandled();

  add_poly(left, right, -1.0);
  left.sort_terms();
  const int degree = left.degree();
  char text[160] {};
  if (degree < 0) return error_result("NONPOLY");
  if (degree == 0) {
    return nearly_zero(left.constant_value()) ? value_result("TRUE") : error_result("NO SOLUTION");
  }
  if (degree == 1) {
    const double a = left.coeff_for(1);
    const double b = left.coeff_for(0);
    if (nearly_zero(a)) return error_result("NO SOLUTION");
    char value[24] {};
    format_number(-b / a, value, sizeof(value));
    std::snprintf(text, sizeof(text), "x=%s", value);
    return value_result(text);
  }
  if (degree == 2) {
    const double a = left.coeff_for(2);
    const double b = left.coeff_for(1);
    const double c = left.coeff_for(0);
    if (nearly_zero(a)) return unhandled();
    const double discriminant = b * b - 4.0 * a * c;
    if (discriminant < -kEpsilon) {
      return error_result("NO REAL ROOTS");
    }
    const double root = std::sqrt(std::max(0.0, discriminant));
    char x0[24] {};
    char x1[24] {};
    format_number((-b - root) / (2.0 * a), x0, sizeof(x0));
    format_number((-b + root) / (2.0 * a), x1, sizeof(x1));
    if (nearly_zero(root)) {
      std::snprintf(text, sizeof(text), "x=%s", x0);
    } else {
      std::snprintf(text, sizeof(text), "x=%s; x=%s", x0, x1);
    }
    return value_result(text);
  }
  return error_result("DEGREE > 2");
}

TextResult try_matrix_result(const char* expr) {
  Matrix matrix {};
  if (extract_matrix_call(expr, "det", matrix)) {
    double det = 0.0;
    if (!determinant(matrix, det)) return error_result("BAD MATRIX");
    char text[32] {};
    format_number(det, text, sizeof(text));
    return value_result(text);
  }
  if (call_name_present(expr, "det")) {
    return error_result("BAD MATRIX");
  }

  if (extract_matrix_call(expr, "inv", matrix) || extract_matrix_call(expr, "inverse", matrix)) {
    Matrix inverse {};
    if (!invert_matrix(matrix, inverse)) return error_result("SINGULAR");
    char text[192] {};
    if (!format_matrix(inverse, text, sizeof(text))) {
      return error_result("RESULT TOO LONG");
    }
    return value_result(text);
  }
  if (call_name_present(expr, "inv") || call_name_present(expr, "inverse")) {
    return error_result("BAD MATRIX");
  }

  return unhandled();
}

}  // namespace esp32calc::symbolic
