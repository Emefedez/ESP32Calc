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

char calculus_variable_from_options(const SolveOptions& options) {
  for (size_t i = 0; i < kSolveVariableCount; ++i) {
    if ((options.solve_mask & (1u << i)) != 0) {
      return kSolveVariables[i];
    }
  }
  return 'x';
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

  if (request.kind == MathJobKind::Graph) {
    result.graph_x_min = request.graph_x_min;
    result.graph_x_max = request.graph_x_max;
    const giac::GiacGraphResponse graph =
        bridge.sample_graph(request.expression,
                            request.graph_x_min,
                            request.graph_x_max,
                            kGraphSampleCount);
    result.graph_count = graph.count;
    for (size_t i = 0; i < graph.count && i < kGraphSampleCount; ++i) {
      result.graph_y[i] = graph.y[i];
      result.graph_valid[i] = graph.valid[i];
    }
    set_text(result, graph.ok ? "GRAPH READY" : graph.error, graph.ok);
    return result;
  }

  giac::GiacResponse response {};
  switch (request.kind) {
    case MathJobKind::Solve:
      response = bridge.solve(request.expression, request.solve_options);
      break;
    case MathJobKind::Script:
      response = bridge.raw(request.expression);
      break;
    case MathJobKind::Symbolic:
      response = bridge.simplify(request.expression);
      break;
    case MathJobKind::Derivative:
      response = bridge.derivative(request.expression,
                                   calculus_variable_from_options(request.solve_options));
      break;
    case MathJobKind::Integral:
      response = bridge.integral(request.expression,
                                 calculus_variable_from_options(request.solve_options));
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
    if (request.decimal_output && is_solve_request) {
      char command[256] {};
      const int written = std::snprintf(command, sizeof(command), "evalf(%s)", response.command);
      if (written > 0 && static_cast<size_t>(written) < sizeof(command)) {
        response = bridge.raw(command);
      }
    }
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
