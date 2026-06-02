#include "math/giac/giac_bridge.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "app_config.h"

#ifndef ESP32CALC_GIAC_COMPILED
#define ESP32CALC_GIAC_COMPILED 0
#endif

#if ESP32CALC_USE_GIAC && ESP32CALC_GIAC_COMPILED
#include <exception>
#include <new>
#include <string>

#include "derive.h"
#include "desolve.h"
#include "gen.h"
#include "global.h"
#include "prog.h"
#include "series.h"
#include "subst.h"
#include "sym2poly.h"
#include "usual.h"

namespace giac {
void check_browser_functions();
void lexer_localization(int lang, const context* contextptr);
}  // namespace giac
#endif

namespace esp32calc_alt::giac {
namespace {

bool has_text(const char* text) {
  return text != nullptr && text[0] != '\0';
}

void copy_text(char* output, size_t output_size, const char* text) {
  if (output == nullptr || output_size == 0) {
    return;
  }
  std::snprintf(output, output_size, "%s", text == nullptr ? "" : text);
}

void append_char(char* output, size_t output_size, size_t& used, char value) {
  if (used + 1 >= output_size) {
    return;
  }
  output[used++] = value;
  output[used] = '\0';
}

void append_text(char* output, size_t output_size, size_t& used, const char* text) {
  if (!has_text(text)) {
    return;
  }
  const int written = std::snprintf(output + used, output_size - used, "%s", text);
  if (written <= 0) {
    return;
  }
  const size_t add = static_cast<size_t>(written);
  used = used + add >= output_size ? output_size - 1 : used + add;
  output[used] = '\0';
}

bool contains_char(const char* text, char target) {
  return text != nullptr && std::strchr(text, target) != nullptr;
}

#if ESP32CALC_USE_GIAC && ESP32CALC_GIAC_COMPILED

::giac::context* as_context(void* context) {
  return static_cast<::giac::context*>(context);
}

std::string trim_copy(const std::string& input) {
  size_t begin = 0;
  while (begin < input.size() && std::isspace(static_cast<unsigned char>(input[begin])) != 0) {
    ++begin;
  }
  size_t end = input.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
    --end;
  }
  return input.substr(begin, end - begin);
}

bool starts_with_ignore_case(const std::string& value, const char* prefix) {
  const size_t prefix_len = std::strlen(prefix);
  if (value.size() < prefix_len) {
    return false;
  }
  for (size_t i = 0; i < prefix_len; ++i) {
    const unsigned char left = static_cast<unsigned char>(value[i]);
    const unsigned char right = static_cast<unsigned char>(prefix[i]);
    if (std::tolower(left) != std::tolower(right)) {
      return false;
    }
  }
  return true;
}

std::string map_command_aliases(const std::string& expression) {
  std::string trimmed = trim_copy(expression);
  if (!trimmed.empty() && trimmed[0] == ':') {
    trimmed = trim_copy(trimmed.substr(1));
  }

  static constexpr const char* kTrigSimplifyAliases[] = {
      "trigsimplify(",
      "trigsimp(",
      "trig_simplify(",
      "simplifytrig(",
  };
  for (const char* alias : kTrigSimplifyAliases) {
    const size_t prefix_len = std::strlen(alias);
    if (starts_with_ignore_case(trimmed, alias) && trimmed.size() > prefix_len &&
        trimmed.back() == ')') {
      const std::string inner = trimmed.substr(prefix_len, trimmed.size() - prefix_len - 1);
      return "simplify(texpand(" + inner + "))";
    }
  }

  if (starts_with_ignore_case(trimmed, "trigexpand(")) {
    return "texpand(" + trimmed.substr(std::strlen("trigexpand("));
  }
  return trimmed;
}

size_t top_level_equals(const std::string& expression) {
  int paren_depth = 0;
  int bracket_depth = 0;
  int brace_depth = 0;
  for (size_t i = 0; i < expression.size(); ++i) {
    switch (expression[i]) {
      case '(':
        ++paren_depth;
        break;
      case ')':
        if (paren_depth > 0) {
          --paren_depth;
        }
        break;
      case '[':
        ++bracket_depth;
        break;
      case ']':
        if (bracket_depth > 0) {
          --bracket_depth;
        }
        break;
      case '{':
        ++brace_depth;
        break;
      case '}':
        if (brace_depth > 0) {
          --brace_depth;
        }
        break;
      case '=':
        if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
          return i;
        }
        break;
      default:
        break;
    }
  }
  return std::string::npos;
}

bool is_plain_y(const std::string& expression) {
  const std::string trimmed = trim_copy(expression);
  return trimmed.size() == 1 &&
         std::tolower(static_cast<unsigned char>(trimmed[0])) == 'y';
}

std::string explicit_graph_expression(const char* expression, bool& ok) {
  ok = false;
  const std::string trimmed = trim_copy(expression == nullptr ? "" : expression);
  if (trimmed.empty()) {
    return "";
  }

  const size_t equals = top_level_equals(trimmed);
  if (equals == std::string::npos) {
    ok = true;
    return trimmed;
  }

  const std::string left = trim_copy(trimmed.substr(0, equals));
  const std::string right = trim_copy(trimmed.substr(equals + 1));
  if (is_plain_y(left)) {
    ok = true;
    return right;
  }
  if (is_plain_y(right)) {
    ok = true;
    return left;
  }
  return "";
}

bool contains_rootof_text(const std::string& value) {
  return value.find("rootof(") != std::string::npos;
}

::giac::gen eval_in_context(const std::string& expression, ::giac::context* context) {
  ::giac::gen parsed(expression, context);
  return ::giac::eval(parsed, ::giac::eval_level(context), context);
}

::giac::gen prettify_rootof_if_needed(const ::giac::gen& input, ::giac::context* context) {
  std::string printed = input.print(context);
  if (!contains_rootof_text(printed)) {
    return input;
  }

  ::giac::gen best = input;
  try {
    ::giac::gen candidate = eval_in_context("normal(" + printed + ")", context);
    if (!::giac::is_undef(candidate)) {
      best = candidate;
      printed = best.print(context);
    }
  } catch (...) {
  }

  if (contains_rootof_text(printed)) {
    try {
      ::giac::gen candidate = eval_in_context("radsimp(" + printed + ")", context);
      if (!::giac::is_undef(candidate)) {
        best = candidate;
      }
    } catch (...) {
    }
  }
  return best;
}

void configure_context(::giac::context* context, const GiacBridgeConfig& config) {
  ::giac::xcas_mode(0, context);
  ::giac::approx_mode(!config.exact, context);
  ::giac::complex_mode(config.allow_complex, context);
  ::giac::complex_variables(config.allow_complex, context);
  ::giac::i_sqrt_minus1(1, context);
  ::giac::withsqrt(true, context);
  ::giac::eval_level(context) = 1;
  ::giac::step_infolevel(context) = config.keep_steps ? 1 : 0;
  ::giac::language(0, context);
  ::giac::check_browser_functions();
  ::giac::lexer_localization(0, context);
  ::giac::cas_setup(::giac::makevecteur(0, 0, 0, 1, 0), context);
}

#endif

}  // namespace

GiacBridge::~GiacBridge() {
#if ESP32CALC_USE_GIAC && ESP32CALC_GIAC_COMPILED
  delete as_context(context_);
  context_ = nullptr;
#endif
}

esp_err_t GiacBridge::begin(const GiacBridgeConfig& config) {
  config_ = config;
#if ESP32CALC_USE_GIAC && ESP32CALC_GIAC_COMPILED
  try {
    if (context_ == nullptr) {
      context_ = new (std::nothrow) ::giac::context();
      if (context_ == nullptr) {
        ready_ = false;
        status_ = ESP_ERR_NO_MEM;
        return status_;
      }
    }
    configure_context(as_context(context_), config_);
    ready_ = true;
    status_ = ESP_OK;
  } catch (...) {
    ready_ = false;
    status_ = ESP_FAIL;
  }
#elif ESP32CALC_USE_GIAC
  ready_ = false;
  status_ = ESP_ERR_NOT_FOUND;
#else
  ready_ = false;
  status_ = ESP_ERR_NOT_SUPPORTED;
#endif
  return status_;
}

bool GiacBridge::available() const {
  return ready_;
}

const char* GiacBridge::status_text() const {
  if (ready_) {
    return "GIAC READY";
  }
  if (status_ == ESP_ERR_NOT_FOUND) {
    return "GIAC NOT LINKED";
  }
  if (status_ == ESP_ERR_NOT_SUPPORTED) {
    return "GIAC DISABLED";
  }
  return "GIAC NOT READY";
}

void GiacBridge::reset_context() {
#if ESP32CALC_USE_GIAC && ESP32CALC_GIAC_COMPILED
  delete as_context(context_);
  context_ = nullptr;
#endif
  ready_ = false;
  status_ = ESP_ERR_INVALID_STATE;
  (void)begin(config_);
}

GiacResponse GiacBridge::evaluate(const char* expression) {
  return run(GiacOperation::Evaluate, expression);
}

GiacResponse GiacBridge::simplify(const char* expression) {
  char command[256] {};
  std::snprintf(command, sizeof(command), "simplify(%s)", expression == nullptr ? "" : expression);
  return run(GiacOperation::Simplify, command);
}

GiacResponse GiacBridge::solve(const char* equation, const SolveOptions& options) {
  char variables[24] {};
  format_solve_variables(options, variables, sizeof(variables));

  char command[256] {};
  if (contains_char(equation, ';')) {
    char equations[112] {};
    if (!format_system_equations(equation, equations, sizeof(equations))) {
      GiacResponse response {};
      response.operation = GiacOperation::Solve;
      copy_text(response.error, sizeof(response.error), "SYSTEM TOO LONG");
      return response;
    }
    std::snprintf(command, sizeof(command), "solve([%s],%s)", equations, variables);
  } else {
    std::snprintf(command,
                  sizeof(command),
                  "solve(%s,%s)",
                  equation == nullptr ? "" : equation,
                  variables);
  }

  return run(GiacOperation::Solve, command);
}

GiacResponse GiacBridge::matrix(const char* expression) {
  return run(GiacOperation::Matrix, expression);
}

GiacResponse GiacBridge::determinant(const char* matrix_expression) {
  char command[256] {};
  std::snprintf(command,
                sizeof(command),
                "det(%s)",
                matrix_expression == nullptr ? "" : matrix_expression);
  return run(GiacOperation::Matrix, command);
}

GiacResponse GiacBridge::inverse(const char* matrix_expression) {
  char command[256] {};
  std::snprintf(command,
                sizeof(command),
                "inv(%s)",
                matrix_expression == nullptr ? "" : matrix_expression);
  return run(GiacOperation::Matrix, command);
}

GiacResponse GiacBridge::graph_expression(const char* expression) {
  char command[256] {};
  std::snprintf(command,
                sizeof(command),
                "simplify(%s)",
                expression == nullptr ? "" : expression);
  return run(GiacOperation::Graph, command);
}

GiacGraphResponse GiacBridge::sample_graph(const char* expression,
                                           float x_min,
                                           float x_max,
                                           size_t sample_count) {
  GiacGraphResponse response {};
  response.count = sample_count > kGraphSampleCount ? kGraphSampleCount : sample_count;
  copy_text(response.command, sizeof(response.command), expression);

  if (!has_text(expression) || response.count == 0) {
    copy_text(response.error, sizeof(response.error), "EMPTY GRAPH");
    return response;
  }

#if ESP32CALC_USE_GIAC && ESP32CALC_GIAC_COMPILED
  if (!ready_ || context_ == nullptr) {
    copy_text(response.error, sizeof(response.error), status_text());
    return response;
  }

  bool explicit_ok = false;
  const std::string body = explicit_graph_expression(expression, explicit_ok);
  const std::string relation = trim_copy(expression);
  if (body.empty() && relation.empty()) {
    copy_text(response.error, sizeof(response.error), "EMPTY GRAPH");
    return response;
  }

  ::giac::context* context = as_context(context_);
  bool has_value = false;
  for (size_t i = 0; i < response.count; ++i) {
    const float t = response.count <= 1
                        ? 0.0f
                        : static_cast<float>(i) / static_cast<float>(response.count - 1);
    const float x_value = x_min + (x_max - x_min) * t;
    char command[256] {};
    if (explicit_ok) {
      std::snprintf(command,
                    sizeof(command),
                    "evalf(subst((%s),x,(%.8g)))",
                    body.c_str(),
                    static_cast<double>(x_value));
    } else {
      std::snprintf(command,
                    sizeof(command),
                    "evalf((solve(subst((%s),x,(%.8g)),y))[0])",
                    relation.c_str(),
                    static_cast<double>(x_value));
    }
    try {
      ::giac::gen result = eval_in_context(command, context);
      result = ::giac::evalf(result, 1, context);
      const double y_value = result.to_double(context);
      if (std::isfinite(y_value)) {
        response.y[i] = static_cast<float>(y_value);
        response.valid[i] = true;
        has_value = true;
      }
    } catch (...) {
      response.valid[i] = false;
    }
  }

  if (!has_value) {
    copy_text(response.error, sizeof(response.error), "GRAPH ERR");
    return response;
  }
  response.ok = true;
#else
  copy_text(response.error, sizeof(response.error), status_text());
#endif
  return response;
}

GiacResponse GiacBridge::raw(const char* command) {
  return run(GiacOperation::Raw, command);
}

GiacResponse GiacBridge::run(GiacOperation operation, const char* command) {
  GiacResponse response {};
  response.operation = operation;
  copy_text(response.command, sizeof(response.command), command);

  if (!has_text(command)) {
    copy_text(response.error, sizeof(response.error), "EMPTY GIAC COMMAND");
    return response;
  }

#if ESP32CALC_USE_GIAC && ESP32CALC_GIAC_COMPILED
  if (!ready_ || context_ == nullptr) {
    copy_text(response.error, sizeof(response.error), status_text());
    return response;
  }

  ::giac::context* context = as_context(context_);
  const std::string expression = map_command_aliases(command);
  try {
    ::giac::gen result = eval_in_context(expression, context);
    result = prettify_rootof_if_needed(result, context);

    if (::giac::is_undef(result)) {
      copy_text(response.error, sizeof(response.error), "UNDEFINED RESULT");
      return response;
    }

    std::string printed = result.print(context);
    copy_text(response.plain, sizeof(response.plain), printed.c_str());
    response.ok = true;
  } catch (const std::exception& error) {
    copy_text(response.error, sizeof(response.error), error.what());
  } catch (...) {
    copy_text(response.error, sizeof(response.error), "GIAC EXCEPTION");
  }
#else
  copy_text(response.error, sizeof(response.error), status_text());
#endif
  return response;
}

char GiacBridge::first_solve_variable(const SolveOptions& options) {
  uint8_t mask = options.solve_mask;
  if (mask == 0) {
    mask = kSolveVariableX;
  }
  for (size_t i = 0; i < kSolveVariableCount; ++i) {
    if ((mask & (1u << i)) != 0) {
      return kSolveVariables[i];
    }
  }
  return 'x';
}

void GiacBridge::format_solve_variables(const SolveOptions& options,
                                        char* output,
                                        size_t output_size) {
  if (output == nullptr || output_size == 0) {
    return;
  }

  uint8_t mask = options.solve_mask;
  if (mask == 0) {
    mask = kSolveVariableX | kSolveVariableY;
  }

  size_t count = 0;
  for (size_t i = 0; i < kSolveVariableCount; ++i) {
    if ((mask & (1u << i)) != 0) {
      ++count;
    }
  }

  if (count <= 1) {
    std::snprintf(output, output_size, "%c", first_solve_variable(options));
    return;
  }

  size_t used = 0;
  append_char(output, output_size, used, '[');
  bool first = true;
  for (size_t i = 0; i < kSolveVariableCount; ++i) {
    if ((mask & (1u << i)) == 0) {
      continue;
    }
    if (!first) {
      append_char(output, output_size, used, ',');
    }
    char variable[2] {kSolveVariables[i], '\0'};
    append_text(output, output_size, used, variable);
    first = false;
  }
  append_char(output, output_size, used, ']');
}

bool GiacBridge::format_system_equations(const char* equations, char* output, size_t output_size) {
  if (!has_text(equations) || output == nullptr || output_size == 0) {
    return false;
  }

  size_t used = 0;
  for (const char* p = equations; *p != '\0'; ++p) {
    if (used + 1 >= output_size) {
      return false;
    }
    output[used++] = *p == ';' ? ',' : *p;
  }
  output[used] = '\0';
  return true;
}

}  // namespace esp32calc_alt::giac
