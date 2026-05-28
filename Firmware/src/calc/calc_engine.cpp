#include "calc/calc_engine.h"

#include "app_events.h"
#include "app_config.h"
#include "esp_log.h"
#include "freertos/task.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace esp32calc {
namespace {
constexpr const char* TAG = "calc";
constexpr UBaseType_t kCalcQueueDepth = 10;

struct ParsedExpression {
  CalcRequestKind request_kind = CalcRequestKind::Evaluate;
  CalcExpressionKind expression_kind = CalcExpressionKind::Invalid;
  const char* expression = nullptr;
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
  while (*p == ' ') {
    ++p;
  }
}

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

struct NumericParser {
  const char* cursor = nullptr;

  NumericParseStatus evaluate(const char* expr, double& value) {
    if (expr == nullptr) {
      return NumericParseStatus::Empty;
    }

    cursor = expr;
    skip_spaces(cursor);
    if (*cursor == '\0') {
      return NumericParseStatus::Empty;
    }

    const NumericParseStatus status = parse_sum(value);
    if (status != NumericParseStatus::Ok) {
      return status;
    }

    skip_spaces(cursor);
    return *cursor == '\0' ? NumericParseStatus::Ok : NumericParseStatus::UnsupportedOperator;
  }

  NumericParseStatus parse_sum(double& value) {
    NumericParseStatus status = parse_product(value);
    if (status != NumericParseStatus::Ok) {
      return status;
    }

    while (true) {
      skip_spaces(cursor);
      const char op = *cursor;
      if (op != '+' && op != '-') {
        return NumericParseStatus::Ok;
      }
      ++cursor;

      double rhs = 0.0;
      status = parse_product(rhs);
      if (status != NumericParseStatus::Ok) {
        return status;
      }

      value = op == '+' ? value + rhs : value - rhs;
      if (!std::isfinite(value)) {
        return NumericParseStatus::BadNumber;
      }
    }
  }

  NumericParseStatus parse_product(double& value) {
    NumericParseStatus status = parse_factor(value);
    if (status != NumericParseStatus::Ok) {
      return status;
    }

    while (true) {
      skip_spaces(cursor);
      const char op = *cursor;
      if (op != '*' && op != '/') {
        return NumericParseStatus::Ok;
      }
      ++cursor;

      double rhs = 0.0;
      status = parse_factor(rhs);
      if (status != NumericParseStatus::Ok) {
        return status;
      }

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

  NumericParseStatus parse_factor(double& value) {
    skip_spaces(cursor);
    if (*cursor == '\0') {
      return NumericParseStatus::MissingNumber;
    }
    if (*cursor == '+') {
      ++cursor;
      return parse_factor(value);
    }
    if (*cursor == '-') {
      ++cursor;
      const NumericParseStatus status = parse_factor(value);
      if (status == NumericParseStatus::Ok) {
        value = -value;
      }
      return status;
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
      return "ONLY +-*/() READY";
    case NumericParseStatus::BadNumber:
    case NumericParseStatus::MissingNumber:
    default:
      return "NUMERIC ERROR";
  }
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

  char text[32] {};
  std::snprintf(text, sizeof(text), "%.12g", value);
  return make_text_result(text, false);
}

CalcResult* make_symbolic_result(const char* expr) {
  double value = 0.0;
  const NumericParseStatus status = evaluate_numeric_expression(expr, value);
  if (status != NumericParseStatus::Ok) {
    return make_text_result(numeric_error_text(status), true);
  }

  if (std::fabs(value) < 0.0000000001) {
    value = 0.0;
  }

  char text[32] {};
  std::snprintf(text, sizeof(text), "%.12g", value);
  return make_text_result(text, false);
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

bool CalcEngine::submit_graph_expression(const char* expr) {
  return submit_request(CalcRequestKind::Graph, expr);
}

bool CalcEngine::submit_request(CalcRequestKind kind, const char* expr) {
  if (requests_queue_ == nullptr || expr == nullptr) {
    return false;
  }

  const size_t len = std::strlen(expr) + 1;
  CalcRequest req {};
  req.kind = kind;
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
