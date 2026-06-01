#include "math/solve/linear_solver.h"

#include <cctype>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace esp32calc_alt::solve {
namespace {

constexpr size_t kSolveMaxEquations = 8;
constexpr size_t kSolveMaxVars = kSolveVariableCount;
constexpr double kSolveEpsilon = 0.000000001;

struct LinearSystem {
  size_t row_count = 0;
  double coeff[kSolveMaxEquations][kSolveMaxVars] {};
  double rhs[kSolveMaxEquations] {};
  uint8_t variable_mask = 0;
};

void skip_spaces(const char*& p) {
  while (std::isspace(static_cast<unsigned char>(*p))) {
    ++p;
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

void scale_expr(LinearExpr& expr, double factor) {
  expr.constant *= factor;
  clean_near_zero(expr.constant);
  for (size_t i = 0; i < kSolveMaxVars; ++i) {
    expr.coeff[i] *= factor;
    clean_near_zero(expr.coeff[i]);
  }
}

LinearStatus multiply_expr(const LinearExpr& lhs, const LinearExpr& rhs, LinearExpr& out) {
  const bool lhs_has_vars = has_variables(lhs);
  const bool rhs_has_vars = has_variables(rhs);
  if (lhs_has_vars && rhs_has_vars) {
    return LinearStatus::NonLinear;
  }

  out = lhs_has_vars ? lhs : rhs;
  scale_expr(out, lhs_has_vars ? rhs.constant : lhs.constant);
  return LinearStatus::Ok;
}

LinearStatus divide_expr(const LinearExpr& lhs, const LinearExpr& rhs, LinearExpr& out) {
  if (has_variables(rhs)) {
    return LinearStatus::NonLinear;
  }
  if (nearly_zero(rhs.constant)) {
    return LinearStatus::DivideByZero;
  }

  out = lhs;
  scale_expr(out, 1.0 / rhs.constant);
  return LinearStatus::Ok;
}

struct LinearParser {
  const char* cursor = nullptr;
  const SolveOptions* solve_options = nullptr;

  explicit LinearParser(const char* expr, const SolveOptions* options = nullptr)
      : cursor(expr), solve_options(options) {}

  LinearStatus parse_expression(LinearExpr& expr) {
    return parse_sum(expr);
  }

  LinearStatus parse_sum(LinearExpr& expr) {
    LinearStatus status = parse_product(expr);
    if (status != LinearStatus::Ok) {
      return status;
    }

    while (true) {
      skip_spaces(cursor);
      const char op = *cursor;
      if (op != '+' && op != '-') {
        return LinearStatus::Ok;
      }
      ++cursor;

      LinearExpr rhs {};
      status = parse_product(rhs);
      if (status != LinearStatus::Ok) {
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

  LinearStatus parse_product(LinearExpr& expr) {
    LinearStatus status = parse_unary(expr);
    if (status != LinearStatus::Ok) {
      return status;
    }

    while (true) {
      skip_spaces(cursor);
      const char op = *cursor;
      if (op == '*' || op == '/') {
        ++cursor;

        LinearExpr rhs {};
        status = parse_unary(rhs);
        if (status != LinearStatus::Ok) {
          return status;
        }

        LinearExpr combined {};
        status = op == '*' ? multiply_expr(expr, rhs, combined) : divide_expr(expr, rhs, combined);
        if (status != LinearStatus::Ok) {
          return status;
        }
        expr = combined;
      } else if (starts_primary()) {
        LinearExpr rhs {};
        status = parse_unary(rhs);
        if (status != LinearStatus::Ok) {
          return status;
        }

        LinearExpr combined {};
        status = multiply_expr(expr, rhs, combined);
        if (status != LinearStatus::Ok) {
          return status;
        }
        expr = combined;
      } else {
        return LinearStatus::Ok;
      }
    }
  }

  LinearStatus parse_power(LinearExpr& expr) {
    LinearStatus status = parse_primary(expr);
    if (status != LinearStatus::Ok) {
      return status;
    }

    skip_spaces(cursor);
    if (*cursor != '^') {
      return LinearStatus::Ok;
    }
    ++cursor;

    LinearExpr exponent {};
    status = parse_unary(exponent);
    if (status != LinearStatus::Ok) {
      return status;
    }
    if (has_variables(exponent)) {
      return LinearStatus::NonLinear;
    }

    if (has_variables(expr)) {
      if (nearly_zero(exponent.constant)) {
        expr = make_constant_expr(1.0);
        return LinearStatus::Ok;
      }
      if (std::fabs(exponent.constant - 1.0) < kSolveEpsilon) {
        return LinearStatus::Ok;
      }
      return LinearStatus::NonLinear;
    }

    expr.constant = std::pow(expr.constant, exponent.constant);
    if (!std::isfinite(expr.constant)) {
      return LinearStatus::BadNumber;
    }
    clean_near_zero(expr.constant);
    return LinearStatus::Ok;
  }

  LinearStatus parse_unary(LinearExpr& expr) {
    skip_spaces(cursor);
    if (*cursor == '+') {
      ++cursor;
      return parse_unary(expr);
    }
    if (*cursor == '-') {
      ++cursor;
      const LinearStatus status = parse_unary(expr);
      if (status == LinearStatus::Ok) {
        scale_expr(expr, -1.0);
      }
      return status;
    }
    return parse_power(expr);
  }

  LinearStatus parse_primary(LinearExpr& expr) {
    skip_spaces(cursor);
    if (*cursor == '\0' || *cursor == '=' || *cursor == ';' || *cursor == ',') {
      return LinearStatus::MissingNumber;
    }

    if (*cursor == '(') {
      ++cursor;
      const LinearStatus status = parse_sum(expr);
      if (status != LinearStatus::Ok) {
        return status;
      }
      skip_spaces(cursor);
      if (*cursor != ')') {
        return LinearStatus::MissingClosingParen;
      }
      ++cursor;
      return LinearStatus::Ok;
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
        return LinearStatus::UnsupportedOperator;
      }

      const int index = variable_index(identifier[0]);
      if (index < 0) {
        return LinearStatus::UnsupportedVariable;
      }

      const uint8_t bit = variable_mask_for_index(static_cast<size_t>(index));
      if (solve_options != nullptr && (solve_options->known_mask & bit) != 0) {
        expr = make_constant_expr(solve_options->known_values[index]);
        return LinearStatus::Ok;
      }

      expr = make_variable_expr(static_cast<size_t>(index));
      return LinearStatus::Ok;
    }

    char* end = nullptr;
    const double value = std::strtod(cursor, &end);
    if (end == cursor) {
      return LinearStatus::BadNumber;
    }
    if (!std::isfinite(value)) {
      return LinearStatus::BadNumber;
    }

    cursor = end;
    expr = make_constant_expr(value);
    return LinearStatus::Ok;
  }
};

LinearStatus append_equation(LinearSystem& system, const LinearExpr& left,
                             const LinearExpr& right) {
  if (system.row_count >= kSolveMaxEquations) {
    return LinearStatus::TooManyEquations;
  }

  LinearExpr normalized = left;
  add_expr(normalized, right, -1.0);

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
  return LinearStatus::Ok;
}

LinearStatus parse_linear_system(const char* expr,
                                 const SolveOptions& options,
                                 LinearSystem& system) {
  if (expr == nullptr) {
    return LinearStatus::Empty;
  }

  const char* cursor = expr;
  skip_spaces(cursor);
  if (*cursor == '\0') {
    return LinearStatus::Empty;
  }

  while (*cursor != '\0') {
    LinearParser parser(cursor, &options);
    LinearExpr left {};
    LinearStatus status = parser.parse_expression(left);
    if (status != LinearStatus::Ok) {
      return status;
    }

    cursor = parser.cursor;
    skip_spaces(cursor);
    if (*cursor != '=') {
      return LinearStatus::MissingEquals;
    }
    ++cursor;

    LinearParser rhs_parser(cursor, &options);
    LinearExpr right {};
    status = rhs_parser.parse_expression(right);
    if (status != LinearStatus::Ok) {
      return status;
    }

    status = append_equation(system, left, right);
    if (status != LinearStatus::Ok) {
      return status;
    }

    cursor = rhs_parser.cursor;
    skip_spaces(cursor);
    if (*cursor == ';' || *cursor == ',') {
      ++cursor;
      skip_spaces(cursor);
      if (*cursor == '\0') {
        return LinearStatus::MissingNumber;
      }
      continue;
    }
    if (*cursor != '\0') {
      return LinearStatus::UnsupportedOperator;
    }
  }

  return system.row_count == 0 ? LinearStatus::Empty : LinearStatus::Ok;
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

bool solve_linear_system(const LinearSystem& source,
                         const SolveOptions& options,
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
    add_default_solve_order(requested_mask, system.row_count, variable_order, variable_count,
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

}  // namespace

LinearStatus parse_linear_expression(const char* expr,
                                     const SolveOptions& options,
                                     LinearExpr& output) {
  if (expr == nullptr) {
    return LinearStatus::Empty;
  }

  LinearParser parser(expr, &options);
  LinearStatus status = parser.parse_expression(output);
  if (status != LinearStatus::Ok) {
    return status;
  }

  skip_spaces(parser.cursor);
  return *parser.cursor == '\0' ? LinearStatus::Ok : LinearStatus::UnsupportedOperator;
}

LinearStatus solve_linear_expression(const char* expr,
                                     const SolveOptions& options,
                                     char* output,
                                     size_t output_size) {
  LinearExpr parsed {};
  const LinearStatus status = parse_linear_expression(expr, options, parsed);
  if (status != LinearStatus::Ok) {
    return status;
  }
  return format_linear_expression(parsed, output, output_size) ? LinearStatus::Ok
                                                              : LinearStatus::ResultTooLong;
}

LinearStatus solve_linear_system_text(const char* expr,
                                      const SolveOptions& options,
                                      char* output,
                                      size_t output_size) {
  LinearSystem system {};
  const LinearStatus status = parse_linear_system(expr, options, system);
  if (status != LinearStatus::Ok) {
    return status;
  }

  if (!solve_linear_system(system, options, output, output_size)) {
    return LinearStatus::ResultTooLong;
  }
  return LinearStatus::Ok;
}

const char* linear_error_text(LinearStatus status) {
  switch (status) {
    case LinearStatus::Empty:
      return "EMPTY EXPRESSION";
    case LinearStatus::MissingEquals:
      return "EXPECT =";
    case LinearStatus::MissingClosingParen:
      return "MISSING )";
    case LinearStatus::DivideByZero:
      return "DIV BY ZERO";
    case LinearStatus::UnsupportedVariable:
      return "BAD VARIABLE";
    case LinearStatus::NonLinear:
      return "NONLINEAR";
    case LinearStatus::TooManyEquations:
      return "TOO MANY EQUATIONS";
    case LinearStatus::UnsupportedOperator:
      return "UNSUPPORTED OP";
    case LinearStatus::ResultTooLong:
      return "RESULT TOO LONG";
    case LinearStatus::BadNumber:
    case LinearStatus::MissingNumber:
    default:
      return "SOLVE ERROR";
  }
}

}  // namespace esp32calc_alt::solve

