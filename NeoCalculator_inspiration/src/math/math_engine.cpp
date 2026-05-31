#include "math/math_engine.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "math/core/numeric_eval.h"
#include "math/core/rational_eval.h"
#include "math/input/expression_classifier.h"
#include "math/solve/linear_solver.h"

namespace esp32calc_alt {
namespace {

void set_text(MathResult& result, const char* text, bool ok) {
  result.ok = ok;
  std::snprintf(result.text, sizeof(result.text), "%s", text == nullptr ? "" : text);
}

MathResult make_numeric_result(const MathRequest& request, ExpressionKind expression_kind) {
  MathResult result {};
  result.kind = request.kind;
  result.expression_kind = expression_kind;

  double value = 0.0;
  const core::NumericStatus status = core::evaluate_numeric_expression(request.expression, value);
  if (status != core::NumericStatus::Ok) {
    set_text(result, core::numeric_error_text(status), false);
    return result;
  }

  if (std::fabs(value) < 0.0000000001) {
    value = 0.0;
  }

  char rational[48] {};
  if (core::try_rational_result(request.expression, rational, sizeof(rational))) {
    set_text(result, rational, true);
    return result;
  }

  char text[32] {};
  std::snprintf(text, sizeof(text), "%.12g", value);
  set_text(result, text, true);
  return result;
}

MathResult make_symbolic_result(const MathRequest& request, ExpressionKind expression_kind) {
  MathResult result {};
  result.kind = request.kind;
  result.expression_kind = expression_kind;

  const solve::LinearStatus status =
      solve::solve_linear_expression(request.expression,
                                     request.solve_options,
                                     result.text,
                                     sizeof(result.text));
  result.ok = status == solve::LinearStatus::Ok;
  if (!result.ok) {
    set_text(result, solve::linear_error_text(status), false);
  }
  return result;
}

MathResult make_solve_result(const MathRequest& request, ExpressionKind expression_kind) {
  MathResult result {};
  result.kind = request.kind;
  result.expression_kind = expression_kind;

  const solve::LinearStatus status =
      solve::solve_linear_system_text(request.expression,
                                      request.solve_options,
                                      result.text,
                                      sizeof(result.text));
  result.ok = status == solve::LinearStatus::Ok && std::strcmp(result.text, "NO SOLUTION") != 0;
  if (status != solve::LinearStatus::Ok) {
    set_text(result, solve::linear_error_text(status), false);
  }
  return result;
}

MathResult make_graph_result(const MathRequest& request, ExpressionKind expression_kind) {
  MathResult result {};
  result.kind = request.kind;
  result.expression_kind = expression_kind;
  result.ok = true;
  std::snprintf(result.text, sizeof(result.text), "GRAPH %s", request.expression);
  return result;
}

MathResult make_placeholder_result(const MathRequest& request,
                                   ExpressionKind expression_kind,
                                   const char* label) {
  MathResult result {};
  result.kind = request.kind;
  result.expression_kind = expression_kind;
  result.ok = false;
  std::snprintf(result.text, sizeof(result.text), "%s TODO: %s", label, request.expression);
  return result;
}

}  // namespace

MathResult evaluate_math_request(const MathRequest& request) {
  const ExpressionKind expression_kind = input::classify_expression(request.expression);

  if (request.kind == MathJobKind::Graph) {
    return make_graph_result(request, expression_kind);
  }
  if (request.kind == MathJobKind::Script) {
    return make_placeholder_result(request, expression_kind, "SCRIPT");
  }
  if (request.kind == MathJobKind::Solve || expression_kind == ExpressionKind::Equation) {
    return make_solve_result(request, expression_kind);
  }
  if (expression_kind == ExpressionKind::Numeric) {
    return make_numeric_result(request, expression_kind);
  }
  if (expression_kind == ExpressionKind::Symbolic) {
    return make_symbolic_result(request, expression_kind);
  }

  MathResult result {};
  result.kind = request.kind;
  result.expression_kind = expression_kind;
  set_text(result, "INVALID EXPRESSION", false);
  return result;
}

}  // namespace esp32calc_alt

