#include "calc/calc_engine.h"

#include "app_events.h"
#include "app_config.h"
#include "calc/symbolic_engine.h"
#include "esp_log.h"
#include "freertos/task.h"

#include <cctype>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace esp32calc {
namespace {
constexpr const char* TAG = "calc";
constexpr UBaseType_t kCalcQueueDepth = 10;
constexpr size_t kSolveMaxEquations = 8;
constexpr size_t kSolveMaxVars = kCalcSolveVariableCount;
constexpr double kSolveEpsilon = 0.000000001;
constexpr char kSolveVariables[kSolveMaxVars] = {'x', 'y', 'z', 'a', 'b', 'c'};

struct ParsedExpression {
  CalcRequestKind request_kind = CalcRequestKind::Evaluate;
  CalcExpressionKind expression_kind = CalcExpressionKind::Invalid;
  const char* expression = nullptr;
};

enum class LinearParseStatus : uint8_t {
  // all of these cases need different handling, as such, instead of making an unreadable huge switch in the solver, a barely readable enum helps us kind of understand stuff slighly cleaner.
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
};

enum class NumericParseStatus : uint8_t {
  Ok,
  Empty,
  BadNumber,
  MissingNumber,
  MissingClosingParen,
  DivideByZero,
  UnsupportedOperator,
};

const char* expression_kind_label(CalcExpressionKind kind) {
  switch (kind) {
    case CalcExpressionKind::Numeric:
      return "NUMERIC";
    case CalcExpressionKind::Symbolic:
      return "SYMBOLIC";
    case CalcExpressionKind::Equation:
      return "EQUATION";
    case CalcExpressionKind::Invalid:
    default:
      return "INVALID";
  }
}

char* duplicate_text(const char* text) {
  if (text == nullptr) {
    return nullptr;
  }

  const size_t len = std::strlen(text) + 1;
  char* copy = new (std::nothrow) char[len];
  if (copy == nullptr) {
    return nullptr;
  }
  std::memcpy(copy, text, len);
  return copy;
}

void skip_spaces(const char*& p) {
  while (std::isspace(static_cast<unsigned char>(*p))) {
    ++p;
  }
}

bool is_numeric_exponent_marker(const char* expr, const char* p) {
  // pi is not a normal variable unlike e, so e needs a special check
  if (*p != 'E' && *p != 'e') return false;
  if (p == expr) return false;
  
  const unsigned char previous = static_cast<unsigned char>(*(p - 1)); // now that we know this expression is empty, we can go back, although something like x + e could potentially be dangerous. Will need to refine.
  return std::isdigit(previous) || *(p - 1) == '.';
}

/* Numeric expression evaluation.
 * 1. Parse additions/subtractions as the outermost operation.
 * 2. Parse multiplications/divisions before sums to preserve precedence.
 * 3. Parse unary signs, parenthesized expressions, and floating-point literals.
 * 4. Reject leftover characters so symbolic/equation input does not pass as numeric.
 */
struct NumericParser {
  const char* cursor = nullptr;

  NumericParseStatus evaluate(const char* expr, double& value) {
    if (expr == nullptr) return NumericParseStatus::Empty;

    cursor = expr;
    skip_spaces(cursor);
    if (*cursor == '\0') return NumericParseStatus::Empty; //if we are at the end after the first step, there is nothing to parse

    // to properly parse and solve, we should first discover the lowest priority operations, so we can then recursively finish
    const NumericParseStatus status = parse_sum(value); 
    if (status != NumericParseStatus::Ok) return status;

    skip_spaces(cursor);
    return *cursor == '\0' ? NumericParseStatus::Ok : NumericParseStatus::UnsupportedOperator; //not implemented yet, so we should leave
  }

  NumericParseStatus parse_sum(double& value) {
    NumericParseStatus status = parse_product(value);
    if (status != NumericParseStatus::Ok) return status;

    while (true) {
      skip_spaces(cursor);
      const char op = *cursor;
      if (op != '+' && op != '-') return NumericParseStatus::Ok;
      ++cursor;

      double rhs = 0.0;
      status = parse_product(rhs);
      if (status != NumericParseStatus::Ok) return status;
      

      value = op == '+' ? value + rhs : value - rhs;
      if (!std::isfinite(value)) return NumericParseStatus::BadNumber;
    }
  }

  NumericParseStatus parse_product(double& value) {
    NumericParseStatus status = parse_unary(value);
    if (status != NumericParseStatus::Ok) return status;
    

    while (true) {
      skip_spaces(cursor);
      const char op = *cursor;
      if (op != '*' && op != '/') return NumericParseStatus::Ok;
      ++cursor;

      double rhs = 0.0;
      status = parse_unary(rhs);
      if (status != NumericParseStatus::Ok) return status;

      if (op == '/') {
        if (rhs == 0.0) {
          return NumericParseStatus::DivideByZero;
        }
        value /= rhs;
      } else {
        value *= rhs;
      }

      if (!std::isfinite(value)) {
        return NumericParseStatus::BadNumber;
      }
    }
  }

  NumericParseStatus parse_unary(double& value) {
    skip_spaces(cursor);
    if (*cursor == '\0') {
      return NumericParseStatus::MissingNumber;
    }
    if (*cursor == '+') {
      ++cursor;
      return parse_unary(value);
    }
    if (*cursor == '-') {
      ++cursor;
      const NumericParseStatus status = parse_unary(value);
      if (status == NumericParseStatus::Ok) {
        value = -value;
      }
      return status;
    }
    return parse_power(value);
  }

  NumericParseStatus parse_power(double& value) {
    NumericParseStatus status = parse_primary(value);
    if (status != NumericParseStatus::Ok) {
      return status;
    }

    skip_spaces(cursor);
    if (*cursor != '^') {
      return NumericParseStatus::Ok;
    }
    ++cursor;

    double exponent = 0.0;
    status = parse_unary(exponent);
    if (status != NumericParseStatus::Ok) {
      return status;
    }

    value = std::pow(value, exponent);
    return std::isfinite(value) ? NumericParseStatus::Ok : NumericParseStatus::BadNumber;
  }

  NumericParseStatus parse_primary(double& value) {
    skip_spaces(cursor);
    if (*cursor == '\0') {
      return NumericParseStatus::MissingNumber;
    }
    if (*cursor == '(') {
      ++cursor;
      const NumericParseStatus status = parse_sum(value);
      if (status != NumericParseStatus::Ok) {
        return status;
      }

      skip_spaces(cursor);
      if (*cursor != ')') {
        return NumericParseStatus::MissingClosingParen;
      }
      ++cursor;
      return NumericParseStatus::Ok;
    }

    char* end = nullptr;
    value = std::strtod(cursor, &end);
    if (end == cursor) {
      return NumericParseStatus::BadNumber;
    }
    if (!std::isfinite(value)) {
      return NumericParseStatus::BadNumber;
    }

    cursor = end;
    return NumericParseStatus::Ok;
  }
};

NumericParseStatus evaluate_numeric_expression(const char* expr, double& value) {
  NumericParser parser {};
  return parser.evaluate(expr, value);
}

const char* numeric_error_text(NumericParseStatus status) {
  switch (status) {
    case NumericParseStatus::Empty:
      return "EMPTY EXPRESSION";
    case NumericParseStatus::MissingClosingParen:
      return "MISSING )";
    case NumericParseStatus::DivideByZero:
      return "DIV BY ZERO";
    case NumericParseStatus::UnsupportedOperator:
      return "ONLY +-*/^() READY";
    case NumericParseStatus::BadNumber:
    case NumericParseStatus::MissingNumber:
    default:
      return "NUMERIC ERROR";
  }
}

bool nearly_zero(double value) {
  return std::fabs(value) < kSolveEpsilon;
}

void clean_near_zero(double& value) {
  if (nearly_zero(value)) {
    value = 0.0;
  }
}

uint8_t variable_mask_for_index(size_t index) {
  return static_cast<uint8_t>(1u << index);
}

int variable_index(char variable) {
  const char lowered = static_cast<char>(std::tolower(static_cast<unsigned char>(variable)));
  for (size_t i = 0; i < kSolveMaxVars; ++i) {
    if (kSolveVariables[i] == lowered) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

const char* linear_error_text(LinearParseStatus status) {
  switch (status) {
    case LinearParseStatus::Empty:
      return "EMPTY EXPRESSION";
    case LinearParseStatus::MissingEquals:
      return "EXPECT =";
    case LinearParseStatus::MissingClosingParen:
      return "MISSING )";
    case LinearParseStatus::DivideByZero:
      return "DIV BY ZERO";
    case LinearParseStatus::UnsupportedVariable:
      return "BAD VARIABLE";
    case LinearParseStatus::NonLinear:
      return "NONLINEAR";
    case LinearParseStatus::TooManyEquations:
      return "TOO MANY EQUATIONS";
    case LinearParseStatus::UnsupportedOperator:
      return "UNSUPPORTED OP";
    case LinearParseStatus::BadNumber:
    case LinearParseStatus::MissingNumber:
    default:
      return "SOLVE ERROR";
  }
}

struct LinearExpr {
  double constant = 0.0;
  double coeff[kSolveMaxVars] {}; // what numbers are associated with each variable, for example, in 7x+4y, coeff[0] = 7
};

struct LinearSystem {
  size_t row_count = 0;
  double coeff[kSolveMaxEquations][kSolveMaxVars] {};
  double rhs[kSolveMaxEquations] {};
  uint8_t variable_mask = 0;
};

bool has_variables(const LinearExpr& expr) {
  for (size_t i = 0; i < kSolveMaxVars; ++i) {
    if (!nearly_zero(expr.coeff[i])) {
      return true;
    }
  }
  return false;
}

LinearExpr make_constant_expr(double value) {
  LinearExpr expr {};
  expr.constant = value;
  clean_near_zero(expr.constant);
  return expr;
}

LinearExpr make_variable_expr(size_t index) {
  LinearExpr expr {};
  if (index < kSolveMaxVars) {
    expr.coeff[index] = 1.0;
  }
  return expr;
}

void add_expr(LinearExpr& lhs, const LinearExpr& rhs, double sign) {
  lhs.constant += sign * rhs.constant;
  clean_near_zero(lhs.constant);
  for (size_t i = 0; i < kSolveMaxVars; ++i) {
    lhs.coeff[i] += sign * rhs.coeff[i];
    clean_near_zero(lhs.coeff[i]);
  }
}

// scaling is what turns, for example, 20/10 into 2/1, it helps when using constants or variables too.
void scale_expr(LinearExpr& expr, double factor) {
  expr.constant *= factor;
  clean_near_zero(expr.constant);
  for (size_t i = 0; i < kSolveMaxVars; ++i) {
    expr.coeff[i] *= factor;
    clean_near_zero(expr.coeff[i]);
  }
}
// multiplying complex expressions requires first identifying
LinearParseStatus multiply_expr(const LinearExpr& lhs, const LinearExpr& rhs, LinearExpr& out) {
  const bool lhs_has_vars = has_variables(lhs);
  const bool rhs_has_vars = has_variables(rhs);
  if (lhs_has_vars && rhs_has_vars) {
    return LinearParseStatus::NonLinear;
  }

  out = lhs_has_vars ? lhs : rhs; // this syntax equates to "if lhs has variables, copy it to out, otherwise copy rhs", one of the sides **must** have a variable, by norm this is the left side
  scale_expr(out, lhs_has_vars ? rhs.constant : lhs.constant);
  /* Example with numbers of scaling after passing values:
   * 2x * 3y:
   * out = 2x (copying lhs)
   * scale out by 3 (rhs constant) to get 6x, but since rhs has variables, we know this is nonlinear and return before this step
   */
  return LinearParseStatus::Ok;
}

LinearParseStatus divide_expr(const LinearExpr& lhs, const LinearExpr& rhs, LinearExpr& out) {
  if (has_variables(rhs)) {
    return LinearParseStatus::NonLinear;
  }
  if (nearly_zero(rhs.constant)) {
    return LinearParseStatus::DivideByZero;
  }

  out = lhs;
  scale_expr(out, 1.0 / rhs.constant);
  return LinearParseStatus::Ok;
}

/* Linear expression parsing.
 * 1. Parse the same precedence ladder as the numeric parser: sum, product, power, unary, primary.
 * 2. Substitute known variable values immediately, before multiplication/division checks.
 * 3. Store each expression as constant + coefficients for x, y, z, a, b, c.
 * 4. Allow scalar multiplication/division, including implicit multiplication like 2x or 2(x+1).
 * 5. Reject nonlinear expressions when two variable-bearing expressions multiply, variables divide,
 *    or a variable expression is raised to a power other than 0 or 1.
 */
struct LinearParser {
  const char* cursor = nullptr;
  const CalcSolveOptions* solve_options = nullptr;

  explicit LinearParser(const char* expr, const CalcSolveOptions* options = nullptr)
      : cursor(expr), solve_options(options) {}

  LinearParseStatus parse_expression(LinearExpr& expr) {
    return parse_sum(expr);
  }

  LinearParseStatus parse_sum(LinearExpr& expr) {
    LinearParseStatus status = parse_product(expr);
    if (status != LinearParseStatus::Ok) {
      return status;
    }

    while (true) {
      skip_spaces(cursor);
      const char op = *cursor;
      if (op != '+' && op != '-') {
        return LinearParseStatus::Ok;
      }
      ++cursor;

      LinearExpr rhs {};
      status = parse_product(rhs);
      if (status != LinearParseStatus::Ok) {
        return status;
      }

      add_expr(expr, rhs, op == '+' ? 1.0 : -1.0);
    }
  }

  bool starts_primary() const {
    const char* p = cursor;
    skip_spaces(p);
    const unsigned char c = static_cast<unsigned char>(*p);
    return *p == '(' || *p == '.' || std::isdigit(c) || std::isalpha(c);
  }

  LinearParseStatus parse_product(LinearExpr& expr) {
    LinearParseStatus status = parse_unary(expr);
    if (status != LinearParseStatus::Ok) {
      return status;
    }

    while (true) {
      skip_spaces(cursor);
      const char op = *cursor;
      if (op == '*' || op == '/') {
        ++cursor;

        LinearExpr rhs {};
        status = parse_unary(rhs);
        if (status != LinearParseStatus::Ok) {
          return status;
        }

        LinearExpr combined {};
        status = op == '*' ? multiply_expr(expr, rhs, combined) : divide_expr(expr, rhs, combined);
        if (status != LinearParseStatus::Ok) {
          return status;
        }
        expr = combined;
      } else if (starts_primary()) {
        LinearExpr rhs {};
        status = parse_unary(rhs);
        if (status != LinearParseStatus::Ok) {
          return status;
        }

        LinearExpr combined {};
        status = multiply_expr(expr, rhs, combined);
        if (status != LinearParseStatus::Ok) {
          return status;
        }
        expr = combined;
      } else {
        return LinearParseStatus::Ok;
      }
    }
  }

  LinearParseStatus parse_power(LinearExpr& expr) {
    LinearParseStatus status = parse_primary(expr);
    if (status != LinearParseStatus::Ok) {
      return status;
    }

    skip_spaces(cursor);
    if (*cursor != '^') {
      return LinearParseStatus::Ok;
    }
    ++cursor;

    LinearExpr exponent {};
    status = parse_unary(exponent);
    if (status != LinearParseStatus::Ok) {
      return status;
    }
    if (has_variables(exponent)) {
      return LinearParseStatus::NonLinear;
    }

    if (has_variables(expr)) {
      if (nearly_zero(exponent.constant)) {
        expr = make_constant_expr(1.0);
        return LinearParseStatus::Ok;
      }
      if (std::fabs(exponent.constant - 1.0) < kSolveEpsilon) {
        return LinearParseStatus::Ok;
      }
      return LinearParseStatus::NonLinear;
    }

    expr.constant = std::pow(expr.constant, exponent.constant);
    if (!std::isfinite(expr.constant)) {
      return LinearParseStatus::BadNumber;
    }
    clean_near_zero(expr.constant);
    return LinearParseStatus::Ok;
  }

  LinearParseStatus parse_unary(LinearExpr& expr) {
    skip_spaces(cursor);
    if (*cursor == '+') {
      ++cursor;
      return parse_unary(expr);
    }
    if (*cursor == '-') {
      ++cursor;
      const LinearParseStatus status = parse_unary(expr);
      if (status == LinearParseStatus::Ok) {
        scale_expr(expr, -1.0);
      }
      return status;
    }
    return parse_primary(expr);
  }

  LinearParseStatus parse_primary(LinearExpr& expr) {
    skip_spaces(cursor);
    if (*cursor == '\0' || *cursor == '=' || *cursor == ';' || *cursor == ',') {
      return LinearParseStatus::MissingNumber;
    }

    if (*cursor == '(') {
      ++cursor;
      const LinearParseStatus status = parse_sum(expr);
      if (status != LinearParseStatus::Ok) {
        return status;
      }
      skip_spaces(cursor);
      if (*cursor != ')') {
        return LinearParseStatus::MissingClosingParen;
      }
      ++cursor;
      return LinearParseStatus::Ok;
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

      if (identifier[1] != '\0') {
        return LinearParseStatus::UnsupportedOperator;
      }

      const int index = variable_index(identifier[0]);
      if (index < 0) {
        return LinearParseStatus::UnsupportedVariable;
      }

      const uint8_t bit = variable_mask_for_index(static_cast<size_t>(index));
      if (solve_options != nullptr && (solve_options->known_mask & bit) != 0) {
        expr = make_constant_expr(solve_options->known_values[index]);
        return LinearParseStatus::Ok;
      }

      expr = make_variable_expr(static_cast<size_t>(index));
      return LinearParseStatus::Ok;
    }

    char* end = nullptr;
    const double value = std::strtod(cursor, &end);
    if (end == cursor) {
      return LinearParseStatus::BadNumber;
    }
    if (!std::isfinite(value)) {
      return LinearParseStatus::BadNumber;
    }

    cursor = end;
    expr = make_constant_expr(value);
    return LinearParseStatus::Ok;
  }
};

/* Equation normalization.
 * 1. Move the right side to the left so each equation is left - right = 0.
 * 2. Copy variable coefficients into the system matrix row.
 * 3. Move the constant to the right-hand side so the row is A*x = b.
 * 4. Track which variables appear so the solver can choose default unknowns.
 */
LinearParseStatus append_equation(LinearSystem& system, const LinearExpr& left,
                                  const LinearExpr& right) {
  if (system.row_count >= kSolveMaxEquations) {
    return LinearParseStatus::TooManyEquations;
  }

  LinearExpr normalized = left;
  add_expr(normalized, right, -1.0); // changing side implies inversion of signs

  const size_t row = system.row_count++;
  for (size_t i = 0; i < kSolveMaxVars; ++i) {
    system.coeff[row][i] = normalized.coeff[i];
    clean_near_zero(system.coeff[row][i]);
    if (!nearly_zero(system.coeff[row][i])) {
      system.variable_mask |= variable_mask_for_index(i);
    }
  }
  system.rhs[row] = -normalized.constant;
  clean_near_zero(system.rhs[row]);
  return LinearParseStatus::Ok;
}

/* Linear system parsing.
 * 1. Parse one equation as left expression, equals sign, right expression.
 * 2. Normalize that equation into one matrix row.
 * 3. Accept ';' or ',' as separators for additional equations.
 * 4. Reject missing equals signs, trailing separators, unsupported tokens, and oversized systems.
 */
LinearParseStatus parse_linear_expression(const char* expr,
                                          const CalcSolveOptions& options,
                                          LinearExpr& output) {
  if (expr == nullptr) {
    return LinearParseStatus::Empty;
  }

  LinearParser parser(expr, &options);
  LinearParseStatus status = parser.parse_expression(output);
  if (status != LinearParseStatus::Ok) {
    return status;
  }

  skip_spaces(parser.cursor);
  return *parser.cursor == '\0' ? LinearParseStatus::Ok : LinearParseStatus::UnsupportedOperator;
}

LinearParseStatus parse_linear_system(const char* expr,
                                      const CalcSolveOptions& options,
                                      LinearSystem& system) {
  if (expr == nullptr) {
    return LinearParseStatus::Empty;
  }

  const char* cursor = expr;
  skip_spaces(cursor);
  if (*cursor == '\0') {
    return LinearParseStatus::Empty;
  }

  while (*cursor != '\0') {
    LinearParser parser(cursor, &options);
    LinearExpr left {};
    LinearParseStatus status = parser.parse_expression(left);
    if (status != LinearParseStatus::Ok) {
      return status;
    }

    cursor = parser.cursor;
    skip_spaces(cursor);
    if (*cursor != '=') {
      return LinearParseStatus::MissingEquals;
    }
    ++cursor;

    LinearParser rhs_parser(cursor, &options);
    LinearExpr right {};
    status = rhs_parser.parse_expression(right);
    if (status != LinearParseStatus::Ok) {
      return status;
    }

    status = append_equation(system, left, right);
    if (status != LinearParseStatus::Ok) {
      return status;
    }

    cursor = rhs_parser.cursor;
    skip_spaces(cursor);
    if (*cursor == ';' || *cursor == ',') {
      ++cursor;
      skip_spaces(cursor);
      if (*cursor == '\0') {
        return LinearParseStatus::MissingNumber;
      }
      continue;
    }
    if (*cursor != '\0') {
      return LinearParseStatus::UnsupportedOperator;
    }
  }

  return system.row_count == 0 ? LinearParseStatus::Empty : LinearParseStatus::Ok;
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

void add_variable_order(uint8_t mask, int* variable_order, size_t& variable_count,
                        uint8_t& ordered_mask) {
  for (size_t i = 0; i < kSolveMaxVars; ++i) {
    const uint8_t bit = variable_mask_for_index(i);
    if ((mask & bit) == 0 || (ordered_mask & bit) != 0) {
      continue;
    }
    variable_order[variable_count++] = static_cast<int>(i);
    ordered_mask |= bit;
  }
}

void add_variable_to_order(size_t index,
                           uint8_t mask,
                           int* variable_order,
                           size_t& variable_count,
                           uint8_t& ordered_mask) {
  const uint8_t bit = variable_mask_for_index(index);
  if ((mask & bit) == 0 || (ordered_mask & bit) != 0) {
    return;
  }

  variable_order[variable_count++] = static_cast<int>(index);
  ordered_mask |= bit;
}

void add_default_solve_order(uint8_t mask,
                             size_t equation_count,
                             int* variable_order,
                             size_t& variable_count,
                             uint8_t& ordered_mask) {
  if (equation_count == 1) {
    add_variable_to_order(1, mask, variable_order, variable_count, ordered_mask);
  }
  add_variable_order(mask, variable_order, variable_count, ordered_mask);
}

bool append_solution_expression(char* output,
                                size_t output_size,
                                size_t& used,
                                char variable,
                                const double* row,
                                size_t variable_count,
                                const int* variable_order,
                                size_t pivot_col,
                                double rhs) {
  if (!append_format(output, output_size, used, "%c=", variable)) {
    return false;
  }

  bool wrote_value = false;
  if (!nearly_zero(rhs)) {
    char number[24] {};
    format_number(rhs, number, sizeof(number));
    if (!append_format(output, output_size, used, "%s", number)) {
      return false;
    }
    wrote_value = true;
  }

  for (size_t col = 0; col < variable_count; ++col) {
    if (col == pivot_col || nearly_zero(row[col])) {
      continue;
    }

    const double term = -row[col];
    const bool negative = term < 0.0;
    const double magnitude = std::fabs(term);
    char number[24] {};

    if (!wrote_value) {
      if (negative && !append_format(output, output_size, used, "-")) {
        return false;
      }
    } else if (!append_format(output, output_size, used, negative ? "-" : "+")) {
      return false;
    }

    if (std::fabs(magnitude - 1.0) >= kSolveEpsilon) {
      format_number(magnitude, number, sizeof(number));
      if (!append_format(output, output_size, used, "%s", number)) {
        return false;
      }
    }

    const int dependent_var = variable_order[col];
    if (!append_format(output, output_size, used, "%c", kSolveVariables[dependent_var])) {
      return false;
    }
    wrote_value = true;
  }

  if (!wrote_value && !append_format(output, output_size, used, "0")) {
    return false;
  }
  return true;
}

bool append_linear_term(char* output,
                        size_t output_size,
                        size_t& used,
                        double coefficient,
                        char variable,
                        bool& wrote_any) {
  if (nearly_zero(coefficient)) {
    return true;
  }

  const bool negative = coefficient < 0.0;
  const double magnitude = std::fabs(coefficient);

  if (!wrote_any) {
    if (negative && !append_format(output, output_size, used, "-")) {
      return false;
    }
  } else if (!append_format(output, output_size, used, negative ? "-" : "+")) {
    return false;
  }

  if (std::fabs(magnitude - 1.0) >= kSolveEpsilon) {
    char number[24] {};
    format_number(magnitude, number, sizeof(number));
    if (!append_format(output, output_size, used, "%s", number)) {
      return false;
    }
  }

  if (!append_format(output, output_size, used, "%c", variable)) {
    return false;
  }

  wrote_any = true;
  return true;
}

bool format_linear_expression(const LinearExpr& expr, char* output, size_t output_size) {
  size_t used = 0;
  bool wrote_any = false;

  for (size_t i = 0; i < kSolveMaxVars; ++i) {
    if (!append_linear_term(output, output_size, used, expr.coeff[i], kSolveVariables[i],
                            wrote_any)) {
      return false;
    }
  }

  if (!nearly_zero(expr.constant) || !wrote_any) {
    char number[24] {};
    format_number(std::fabs(expr.constant), number, sizeof(number));
    if (!wrote_any) {
      if (expr.constant < 0.0 && !append_format(output, output_size, used, "-")) {
        return false;
      }
    } else if (!append_format(output, output_size, used, expr.constant < 0.0 ? "-" : "+")) {
      return false;
    }
    if (!append_format(output, output_size, used, "%s", number)) {
      return false;
    }
  }

  return true;
}

/* Linear system solving with reduced row echelon form.
 * 1. Apply any remaining known variable columns to the right-hand side.
 * 2. Choose matrix columns: requested solve variables first, parameters next, then other variables.
 *    Single-equation x/y relations prefer y as the dependent variable, so y is solved from x.
 * 3. Run Gaussian elimination with partial pivoting and normalize each pivot row.
 * 4. Eliminate above and below each pivot to produce reduced row echelon form.
 * 5. Detect inconsistent zero-coefficient rows as "NO SOLUTION".
 * 6. Format pivot variables as solved expressions.
 * 7. Format non-pivot variables once as free variables.
 */
bool solve_linear_system(const LinearSystem& source,
                         const CalcSolveOptions& options,
                         char* output,
                         size_t output_size) {
  LinearSystem system = source;

  for (size_t row = 0; row < system.row_count; ++row) {
    for (size_t var = 0; var < kSolveMaxVars; ++var) {
      const uint8_t bit = variable_mask_for_index(var);
      if ((options.known_mask & bit) == 0) {
        continue;
      }
      system.rhs[row] -= system.coeff[row][var] * options.known_values[var];
      system.coeff[row][var] = 0.0;
      clean_near_zero(system.rhs[row]);
    }
  }

  uint8_t requested_mask = options.solve_mask & ~options.known_mask;
  const bool explicit_solve_mask = options.solve_mask != 0;
  if (!explicit_solve_mask) {
    requested_mask = system.variable_mask & ~options.known_mask;
  }

  uint8_t relevant_mask =
      (system.variable_mask | requested_mask | options.parameter_mask) & ~options.known_mask;

  int variable_order[kSolveMaxVars] {};
  size_t variable_count = 0;
  uint8_t ordered_mask = 0;
  if (explicit_solve_mask) {
    add_variable_order(requested_mask, variable_order, variable_count, ordered_mask);
  } else {
    add_default_solve_order(requested_mask,
                            system.row_count,
                            variable_order,
                            variable_count,
                            ordered_mask);
  }
  add_variable_order(options.parameter_mask & relevant_mask, variable_order, variable_count,
                     ordered_mask);
  add_variable_order(relevant_mask, variable_order, variable_count, ordered_mask);

  double matrix[kSolveMaxEquations][kSolveMaxVars + 1] {};
  for (size_t row = 0; row < system.row_count; ++row) {
    for (size_t col = 0; col < variable_count; ++col) {
      matrix[row][col] = system.coeff[row][variable_order[col]];
      clean_near_zero(matrix[row][col]);
    }
    matrix[row][variable_count] = system.rhs[row];
    clean_near_zero(matrix[row][variable_count]);
  }

  int pivot_row_by_col[kSolveMaxVars] {};
  for (size_t i = 0; i < kSolveMaxVars; ++i) {
    pivot_row_by_col[i] = -1;
  }

  size_t pivot_row = 0;
  for (size_t col = 0; col < variable_count && pivot_row < system.row_count; ++col) {
    size_t best_row = pivot_row;
    double best_abs = std::fabs(matrix[pivot_row][col]);
    for (size_t row = pivot_row + 1; row < system.row_count; ++row) {
      const double candidate_abs = std::fabs(matrix[row][col]);
      if (candidate_abs > best_abs) {
        best_abs = candidate_abs;
        best_row = row;
      }
    }

    if (best_abs < kSolveEpsilon) {
      continue;
    }

    if (best_row != pivot_row) {
      for (size_t c = 0; c <= variable_count; ++c) {
        const double tmp = matrix[pivot_row][c];
        matrix[pivot_row][c] = matrix[best_row][c];
        matrix[best_row][c] = tmp;
      }
    }

    const double divisor = matrix[pivot_row][col];
    for (size_t c = col; c <= variable_count; ++c) {
      matrix[pivot_row][c] /= divisor;
      clean_near_zero(matrix[pivot_row][c]);
    }

    for (size_t row = 0; row < system.row_count; ++row) {
      if (row == pivot_row || nearly_zero(matrix[row][col])) {
        continue;
      }
      const double factor = matrix[row][col];
      for (size_t c = col; c <= variable_count; ++c) {
        matrix[row][c] -= factor * matrix[pivot_row][c];
        clean_near_zero(matrix[row][c]);
      }
    }

    pivot_row_by_col[col] = static_cast<int>(pivot_row);
    ++pivot_row;
  }

  for (size_t row = 0; row < system.row_count; ++row) {
    bool all_zero = true;
    for (size_t col = 0; col < variable_count; ++col) {
      if (!nearly_zero(matrix[row][col])) {
        all_zero = false;
        break;
      }
    }
    if (all_zero && !nearly_zero(matrix[row][variable_count])) {
      std::snprintf(output, output_size, "NO SOLUTION");
      return true;
    }
  }

  size_t used = 0;
  bool wrote_any = false;

  for (size_t var = 0; var < kSolveMaxVars; ++var) {
    const uint8_t bit = variable_mask_for_index(var);
    if ((options.solve_mask & bit) == 0 || (options.known_mask & bit) == 0) {
      continue;
    }

    char number[24] {};
    format_number(options.known_values[var], number, sizeof(number));
    if (wrote_any && !append_format(output, output_size, used, "; ")) {
      return false;
    }
    if (!append_format(output, output_size, used, "%c=%s", kSolveVariables[var], number)) {
      return false;
    }
    wrote_any = true;
  }

  for (size_t order_index = 0; order_index < variable_count; ++order_index) {
    const size_t var = static_cast<size_t>(variable_order[order_index]);
    const uint8_t bit = variable_mask_for_index(var);
    if ((requested_mask & bit) == 0) {
      continue;
    }

    const int col = static_cast<int>(order_index);
    const int row = pivot_row_by_col[col];
    if (wrote_any && !append_format(output, output_size, used, "; ")) {
      return false;
    }

    if (row >= 0) {
      if (!append_solution_expression(output,
                                      output_size,
                                      used,
                                      kSolveVariables[var],
                                      matrix[row],
                                      variable_count,
                                      variable_order,
                                      static_cast<size_t>(col),
                                      matrix[row][variable_count])) {
        return false;
      }
    } else if (!append_format(output, output_size, used, "%c=free", kSolveVariables[var])) {
      return false;
    }
    wrote_any = true;
  }

  if (!wrote_any) {
    std::snprintf(output, output_size, "TRUE");
    return true;
  }
  return true;
}

CalcResult* make_text_result(const char* text, bool is_error) {
  CalcResult* result = new (std::nothrow) CalcResult();
  if (result == nullptr) {
    return nullptr;
  }

  result->is_error_ = is_error;
  result->display_text = duplicate_text(text);
  if (result->display_text == nullptr) {
    destroy_calc_result(result);
    return nullptr;
  }
  return result;
}

CalcResult* make_numeric_result(const char* expr) {
  double value = 0.0;
  const NumericParseStatus status = evaluate_numeric_expression(expr, value);
  if (status != NumericParseStatus::Ok) {
    return make_text_result(numeric_error_text(status), true);
  }

  if (std::fabs(value) < 0.0000000001) {
    value = 0.0;
  }

  const symbolic::TextResult rational = symbolic::try_rational_result(expr);
  if (rational.handled()) {
    return make_text_result(rational.text, rational.is_error());
  }

  char text[32] {};
  std::snprintf(text, sizeof(text), "%.12g", value);
  return make_text_result(text, false);
}

CalcResult* make_symbolic_result(const char* expr) {
  const symbolic::TextResult symbolic_result = symbolic::try_symbolic_result(expr);
  if (symbolic_result.handled()) {
    return make_text_result(symbolic_result.text, symbolic_result.is_error());
  }

  CalcSolveOptions options {};
  LinearExpr parsed {};
  const LinearParseStatus status = parse_linear_expression(expr, options, parsed);
  if (status != LinearParseStatus::Ok) {
    return make_text_result(linear_error_text(status), true);
  }

  char text[128] {};
  if (!format_linear_expression(parsed, text, sizeof(text))) {
    return make_text_result("RESULT TOO LONG", true);
  }

  return make_text_result(text, false);
}

/* Solver result pipeline.
 * 1. Parse the input text into a linear system.
 * 2. Solve the system using the requested variables, parameters, and known values.
 * 3. Convert solver failures into displayable calculator errors.
 * 4. Return "NO SOLUTION" as an error result and valid solution text otherwise.
 */
CalcResult* make_solve_result(const char* expr, const CalcSolveOptions& options) {
  LinearSystem system {};
  const LinearParseStatus status = parse_linear_system(expr, options, system);
  if (status != LinearParseStatus::Ok) {
    if (status == LinearParseStatus::NonLinear) {
      const symbolic::TextResult polynomial = symbolic::try_polynomial_equation_result(expr);
      if (polynomial.handled()) {
        return make_text_result(polynomial.text, polynomial.is_error());
      }
    }
    return make_text_result(linear_error_text(status), true);
  }

  char text[192] {};
  if (!solve_linear_system(system, options, text, sizeof(text))) {
    return make_text_result("RESULT TOO LONG", true);
  }

  return make_text_result(text, std::strcmp(text, "NO SOLUTION") == 0);
}

CalcResult* make_placeholder_result(const char* expr, CalcExpressionKind kind) {
  CalcResult* result = new (std::nothrow) CalcResult();
  if (result == nullptr) {
    return nullptr;
  }

  if (kind == CalcExpressionKind::Invalid) {
    result->is_error_ = true;
    result->display_text = duplicate_text("INVALID EXPRESSION");
    if (result->display_text == nullptr) {
      destroy_calc_result(result);
      return nullptr;
    }
    return result;
  }

  const char* kind_text = expression_kind_label(kind);
  const size_t len = std::strlen(kind_text) + std::strlen(expr) + 9;
  result->display_text = new (std::nothrow) char[len];
  if (result->display_text == nullptr) {
    destroy_calc_result(result);
    return nullptr;
  }

  std::snprintf(result->display_text, len, "%s TODO: %s", kind_text, expr);
  return result;
}

CalcResult* make_graph_result(const char* expr) {
  CalcResult* result = new (std::nothrow) CalcResult();
  if (result == nullptr) {
    return nullptr;
  }

  result->action = CalcResultAction::OpenGraph;
  result->graph_expression = duplicate_text(expr);
  if (result->graph_expression == nullptr) {
    destroy_calc_result(result);
    return nullptr;
  }

  const char* prefix = "GRAPH ";
  const size_t len = std::strlen(prefix) + std::strlen(expr) + 1;
  result->display_text = new (std::nothrow) char[len];
  if (result->display_text == nullptr) {
    destroy_calc_result(result);
    return nullptr;
  }
  std::snprintf(result->display_text, len, "%s%s", prefix, expr);
  return result;
}
}  // namespace

esp_err_t CalcEngine::start(QueueHandle_t app_events) {
  app_events_ = app_events;

  if (requests_queue_ == nullptr) {
    requests_queue_ = xQueueCreate(kCalcQueueDepth, sizeof(CalcRequest));
    if (requests_queue_ == nullptr) {
      return ESP_ERR_NO_MEM;
    }
  }

  BaseType_t ok = xTaskCreatePinnedToCore(
      &CalcEngine::task_trampoline,
      "calc",
      6144,
      this,
      5,
      nullptr,
      config::kCoreServicesCore);
  if (ok != pdPASS) {
    vQueueDelete(requests_queue_);
    requests_queue_ = nullptr;
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

bool CalcEngine::submit_expression(const char* expr) {
  return submit_request(CalcRequestKind::Evaluate, expr);
}

bool CalcEngine::submit_solve_expression(const char* expr, CalcSolveOptions options) {
  return submit_request(CalcRequestKind::Solve, expr, options);
}

bool CalcEngine::submit_graph_expression(const char* expr) {
  return submit_request(CalcRequestKind::Graph, expr);
}

bool CalcEngine::submit_request(CalcRequestKind kind,
                                const char* expr,
                                const CalcSolveOptions& options) {
  if (requests_queue_ == nullptr || expr == nullptr) {
    return false;
  }

  const size_t len = std::strlen(expr) + 1;
  CalcRequest req {};
  req.kind = kind;
  req.solve_options = options;
  req.dynamic_expression = new (std::nothrow) char[len];
  if (req.dynamic_expression == nullptr) {
    return false;
  }
  std::memcpy(req.dynamic_expression, expr, len);

  if (xQueueSend(requests_queue_, &req, 0) != pdTRUE) {
    delete[] req.dynamic_expression;
    return false;
  }
  return true;
}

void CalcEngine::task_trampoline(void* arg) {
  static_cast<CalcEngine*>(arg)->task();
}

void CalcEngine::task() {
  ESP_LOGI(TAG, "symbolic/numeric engine placeholder ready");
  CalcRequest req {};
  while (true) {
    if (xQueueReceive(requests_queue_, &req, portMAX_DELAY) == pdTRUE) {
      ESP_LOGI(TAG, "Processing: %s", req.dynamic_expression);
      ParsedExpression parsed {};
      parsed.request_kind = req.kind;
      parsed.expression_kind = expression_identifier(req.dynamic_expression);
      parsed.expression = req.dynamic_expression;

      if (parsed.request_kind == CalcRequestKind::Graph) {
        publish_result(make_graph_result(parsed.expression));
      } else if (parsed.request_kind == CalcRequestKind::Solve ||
                 parsed.expression_kind == CalcExpressionKind::Equation) {
        publish_result(make_solve_result(parsed.expression, req.solve_options));
      } else if (parsed.expression_kind == CalcExpressionKind::Numeric) {
        publish_result(make_numeric_result(parsed.expression));
      } else if (parsed.expression_kind == CalcExpressionKind::Symbolic) {
        publish_result(make_symbolic_result(parsed.expression));
      } else {
        publish_result(make_placeholder_result(parsed.expression, parsed.expression_kind));
      }
      delete[] req.dynamic_expression;
      req.dynamic_expression = nullptr;
    }
  }
}

CalcExpressionKind CalcEngine::expression_identifier(const char* expr) {
  if (expr == nullptr || expr[0] == '\0') {
    return CalcExpressionKind::Invalid;
  }

  bool has_variable = false;
  for (const char* p = expr; *p != '\0'; ++p) {
    if (*p == '=') {
      return CalcExpressionKind::Equation;
    }
    if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')) {
      if (is_numeric_exponent_marker(expr, p)) {
        continue;
      }
      has_variable = true;
    }
    if (std::iscntrl(static_cast<unsigned char>(*p))) {
      return CalcExpressionKind::Invalid;
    }
  }

  return has_variable ? CalcExpressionKind::Symbolic : CalcExpressionKind::Numeric;
}

void CalcEngine::publish_result(CalcResult* result) {
  if (result == nullptr) {
    ESP_LOGW(TAG, "failed to allocate calc result");
    return;
  }

  if (app_events_ == nullptr) {
    destroy_calc_result(result);
    return;
  }

  AppEvent event {};
  event.type = AppEventType::CalcResult;
  event.calc_result = result;

  if (xQueueSend(app_events_, &event, 0) != pdTRUE) {
    ESP_LOGW(TAG, "failed to publish calc result");
    destroy_calc_result(result);
  }
}

}  // namespace esp32calc
