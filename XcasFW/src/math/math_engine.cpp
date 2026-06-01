#include "math/math_engine.h"

#include <cstdio>

#include "esp_log.h"
#include "math/giac/giac_bridge.h"
#include "math/input/expression_classifier.h"
#include "math/solve/solve_result_formatter.h"

namespace esp32calc_alt {
namespace {

constexpr const char* TAG = "math_engine";

void set_text(MathResult& result, const char* text, bool ok) {
  result.ok = ok;
  std::snprintf(result.text, sizeof(result.text), "%s", text == nullptr ? "" : text);
}

MathResult make_giac_result(const MathRequest& request,
                            ExpressionKind expression_kind,
                            giac::GiacBridge& bridge) {
  MathResult result {};
  result.kind = request.kind;
  result.expression_kind = expression_kind;

  if (!bridge.available()) {
    set_text(result, bridge.status_text(), false);
    return result;
  }

  const bool is_solve_request =
      request.kind == MathJobKind::Solve ||
      (request.kind == MathJobKind::Numeric && expression_kind == ExpressionKind::Equation);

  giac::GiacResponse response {};
  switch (request.kind) {
    case MathJobKind::Graph:
      response = bridge.graph_expression(request.expression);
      break;
    case MathJobKind::Solve:
      response = bridge.solve(request.expression, request.solve_options);
      break;
    case MathJobKind::Script:
      response = bridge.raw(request.expression);
      break;
    case MathJobKind::Symbolic:
      response = bridge.simplify(request.expression);
      break;
    case MathJobKind::Numeric:
    default:
      if (expression_kind == ExpressionKind::Equation) {
        response = bridge.solve(request.expression, request.solve_options);
      } else if (expression_kind == ExpressionKind::Symbolic) {
        response = bridge.simplify(request.expression);
      } else {
        response = bridge.evaluate(request.expression);
      }
      break;
  }

  if (response.ok) {
    if (is_solve_request) {
      const solve::SolvePresentation presentation =
          solve::present_giac_solve_result(response.plain,
                                           response.command,
                                           request.solve_options);
      ESP_LOGI(TAG, "solve command: %s", response.command);
      ESP_LOGI(TAG, "solve cas: %s", response.plain);
      ESP_LOGI(TAG,
               "solve solutions (%u): %s",
               static_cast<unsigned>(presentation.solution_count),
               presentation.display);
      set_text(result, presentation.display, true);
    } else {
      set_text(result, response.plain, true);
    }
  } else {
    if (is_solve_request) {
      ESP_LOGW(TAG, "solve error: %s -> %s", response.command, response.error);
    }
    set_text(result, response.error[0] == '\0' ? "GIAC ERROR" : response.error, false);
  }
  return result;
}

}  // namespace

MathResult evaluate_math_request(const MathRequest& request, giac::GiacBridge& bridge) {
  const ExpressionKind expression_kind = input::classify_expression(request.expression);
  if (expression_kind == ExpressionKind::Invalid) {
    MathResult result {};
    result.kind = request.kind;
    result.expression_kind = expression_kind;
    set_text(result, "INVALID EXPRESSION", false);
    return result;
  }
  return make_giac_result(request, expression_kind, bridge);
}

}  // namespace esp32calc_alt
