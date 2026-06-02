// Deprecated local graph parser/evaluator. Move back only if graph sampling
// leaves GiacBridge and returns to a UI-local parser.

#include "graph_expression.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "ui/menu_constants.h"

namespace esp32calc_alt::graph_expression {
namespace {

namespace constants = menu_constants;

const char* graph_expression_body(const char* expression) {
  if (expression == nullptr) {
    return "";
  }
  const char* equals = std::strchr(expression, '=');
  return equals == nullptr ? expression : equals + 1;
}

class GraphExpressionEvaluator {
 public:
  GraphExpressionEvaluator(const char* expression, float x, float y, bool allow_y)
      : input_(expression), x_(x), y_(y), allow_y_(allow_y) {}

  float evaluate(bool& ok) {
    const float value = parse_expression();
    skip_spaces();
    ok = ok_ && *input_ == '\0' && std::isfinite(value);
    return value;
  }

 private:
  const char* input_;
  float x_;
  float y_;
  bool allow_y_;
  bool ok_ = true;

  void skip_spaces() {
    while (*input_ == ' ') {
      ++input_;
    }
  }

  bool match(char expected) {
    skip_spaces();
    if (*input_ != expected) {
      return false;
    }
    ++input_;
    return true;
  }

  bool starts_primary() {
    skip_spaces();
    const unsigned char c = static_cast<unsigned char>(*input_);
    return *input_ == '(' || *input_ == '.' || std::isdigit(c) || std::isalpha(c);
  }

  float parse_expression() {
    float value = parse_term();
    while (ok_) {
      if (match('+')) {
        value += parse_term();
      } else if (match('-')) {
        value -= parse_term();
      } else {
        break;
      }
    }
    return value;
  }

  float parse_term() {
    float value = parse_unary();
    while (ok_) {
      if (match('*')) {
        value *= parse_unary();
      } else if (match('/')) {
        const float denominator = parse_unary();
        if (denominator == 0.0f) {
          ok_ = false;
          return 0.0f;
        }
        value /= denominator;
      } else if (starts_primary()) {
        value *= parse_unary();
      } else {
        break;
      }
    }
    return value;
  }

  float parse_power() {
    float value = parse_primary();
    if (ok_ && match('^')) {
      value = std::pow(value, parse_unary());
      if (!std::isfinite(value)) {
        ok_ = false;
      }
    }
    return value;
  }

  float parse_unary() {
    if (match('+')) {
      return parse_unary();
    }
    if (match('-')) {
      return -parse_unary();
    }
    return parse_power();
  }

  float parse_number() {
    char* end = nullptr;
    const float value = std::strtof(input_, &end);
    if (end == input_) {
      ok_ = false;
      return 0.0f;
    }
    input_ = end;
    return value;
  }

  bool parse_identifier(char (&identifier)[8]) {
    skip_spaces();
    size_t used = 0;
    bool has_identifier = false;
    while (std::isalpha(static_cast<unsigned char>(*input_))) {
      has_identifier = true;
      if (used + 1 < sizeof(identifier)) {
        identifier[used++] =
            static_cast<char>(std::tolower(static_cast<unsigned char>(*input_)));
      }
      ++input_;
    }
    identifier[used] = '\0';
    return has_identifier;
  }

  float parse_function_argument() {
    if (!match('(')) {
      ok_ = false;
      return 0.0f;
    }
    const float argument = parse_expression();
    if (!match(')')) {
      ok_ = false;
    }
    return argument;
  }

  float parse_identifier_value() {
    char identifier[8] {};
    if (!parse_identifier(identifier)) {
      ok_ = false;
      return 0.0f;
    }

    if (std::strcmp(identifier, "x") == 0) {
      return x_;
    }
    if (std::strcmp(identifier, "y") == 0) {
      if (!allow_y_) {
        ok_ = false;
        return 0.0f;
      }
      return y_;
    }
    if (std::strcmp(identifier, "pi") == 0) {
      return constants::kPi;
    }
    if (std::strcmp(identifier, "e") == 0) {
      return constants::kEuler;
    }

    const float argument = parse_function_argument();
    if (!ok_) {
      return 0.0f;
    }

    if (std::strcmp(identifier, "sin") == 0) {
      return std::sin(argument);
    }
    if (std::strcmp(identifier, "cos") == 0) {
      return std::cos(argument);
    }
    if (std::strcmp(identifier, "tan") == 0) {
      return std::tan(argument);
    }
    if (std::strcmp(identifier, "sqrt") == 0) {
      return argument < 0.0f ? (ok_ = false, 0.0f) : std::sqrt(argument);
    }
    if (std::strcmp(identifier, "ln") == 0) {
      return argument <= 0.0f ? (ok_ = false, 0.0f) : std::log(argument);
    }
    if (std::strcmp(identifier, "log") == 0) {
      return argument <= 0.0f ? (ok_ = false, 0.0f) : std::log10(argument);
    }

    ok_ = false;
    return 0.0f;
  }

  float parse_primary() {
    skip_spaces();
    if (match('(')) {
      const float value = parse_expression();
      if (!match(')')) {
        ok_ = false;
      }
      return value;
    }

    const unsigned char c = static_cast<unsigned char>(*input_);
    if (*input_ == '.' || std::isdigit(c)) {
      return parse_number();
    }
    if (std::isalpha(c)) {
      return parse_identifier_value();
    }

    ok_ = false;
    return 0.0f;
  }
};

void copy_expression_segment(char (&output)[constants::kGraphExpressionCapacity],
                             const char* begin,
                             size_t length) {
  const size_t copy_len = std::min(length, sizeof(output) - 1);
  std::memcpy(output, begin, copy_len);
  output[copy_len] = '\0';
}

float evaluate_segment(const char* expression, float x, float y, bool allow_y, bool& ok) {
  GraphExpressionEvaluator evaluator(expression, x, y, allow_y);
  return evaluator.evaluate(ok);
}

bool expression_references_y(const char* expression) {
  for (const char* p = expression; p != nullptr && *p != '\0';) {
    if (!std::isalpha(static_cast<unsigned char>(*p))) {
      ++p;
      continue;
    }

    char identifier[8] {};
    size_t used = 0;
    while (std::isalpha(static_cast<unsigned char>(*p))) {
      if (used + 1 < sizeof(identifier)) {
        identifier[used++] =
            static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
      }
      ++p;
    }
    identifier[used] = '\0';
    if (std::strcmp(identifier, "y") == 0) {
      return true;
    }
  }
  return false;
}

bool expression_is_plain_y(const char* expression) {
  if (expression == nullptr) {
    return false;
  }
  while (*expression == ' ') {
    ++expression;
  }
  if (std::tolower(static_cast<unsigned char>(*expression)) != 'y') {
    return false;
  }
  ++expression;
  while (*expression == ' ') {
    ++expression;
  }
  return *expression == '\0';
}

bool solve_relation_y(const char* expression, float x, float& y) {
  bool has_best = false;
  float best_y = 0.0f;
  bool prev_ok = false;
  float prev_y = constants::kRelationYMin;
  float prev_value = 0.0f;

  for (float sample_y = constants::kRelationYMin;
       sample_y <= constants::kRelationYMax + constants::kRelationRootEpsilon;
       sample_y += constants::kRelationStep) {
    bool ok = false;
    const float value = evaluate_relation(expression, x, sample_y, ok);
    if (!ok || !std::isfinite(value)) {
      prev_ok = false;
      continue;
    }

    if (std::fabs(value) < constants::kRelationRootEpsilon) {
      best_y = sample_y;
      has_best = true;
    }

    if (prev_ok && ((prev_value <= 0.0f && value >= 0.0f) ||
                    (prev_value >= 0.0f && value <= 0.0f))) {
      float low_y = prev_y;
      float high_y = sample_y;
      float low_value = prev_value;
      for (int i = 0; i < 24; ++i) {
        const float mid_y = 0.5f * (low_y + high_y);
        bool mid_ok = false;
        const float mid_value = evaluate_relation(expression, x, mid_y, mid_ok);
        if (!mid_ok || !std::isfinite(mid_value)) {
          break;
        }
        if ((low_value <= 0.0f && mid_value >= 0.0f) ||
            (low_value >= 0.0f && mid_value <= 0.0f)) {
          high_y = mid_y;
        } else {
          low_y = mid_y;
          low_value = mid_value;
        }
      }
      best_y = 0.5f * (low_y + high_y);
      has_best = true;
    }

    prev_ok = true;
    prev_y = sample_y;
    prev_value = value;
  }

  if (!has_best) {
    return false;
  }
  y = best_y;
  return true;
}

}  // namespace

float evaluate_relation(const char* expression, float x, float y, bool& ok) {
  const char* equals = expression == nullptr ? nullptr : std::strchr(expression, '=');
  if (equals == nullptr) {
    return evaluate_segment(expression, x, y, true, ok);
  }

  char left[constants::kGraphExpressionCapacity] {};
  char right[constants::kGraphExpressionCapacity] {};
  copy_expression_segment(left, expression, static_cast<size_t>(equals - expression));
  copy_expression_segment(right, equals + 1, std::strlen(equals + 1));

  bool left_ok = false;
  bool right_ok = false;
  const float left_value = evaluate_segment(left, x, y, true, left_ok);
  const float right_value = evaluate_segment(right, x, y, true, right_ok);
  ok = left_ok && right_ok;
  return left_value - right_value;
}

float evaluate(const char* expression, float x, bool& ok) {
  const char* equals = expression == nullptr ? nullptr : std::strchr(expression, '=');
  const bool has_equals = equals != nullptr;
  const bool has_y = expression_references_y(expression);

  if (has_equals) {
    char left[constants::kGraphExpressionCapacity] {};
    copy_expression_segment(left, expression, static_cast<size_t>(equals - expression));
    if (expression_is_plain_y(left)) {
      return evaluate_segment(equals + 1, x, 0.0f, false, ok);
    }

    char right[constants::kGraphExpressionCapacity] {};
    copy_expression_segment(right, equals + 1, std::strlen(equals + 1));
    if (expression_is_plain_y(right)) {
      return evaluate_segment(left, x, 0.0f, false, ok);
    }

    if (!has_y) {
      ok = false;
      return 0.0f;
    }

    bool y0_ok = false;
    bool y1_ok = false;
    bool y2_ok = false;
    const float y0_value = evaluate_relation(expression, x, 0.0f, y0_ok);
    const float y1_value = evaluate_relation(expression, x, 1.0f, y1_ok);
    const float y2_value = evaluate_relation(expression, x, 2.0f, y2_ok);
    const float y_slope = y1_value - y0_value;
    const float linear_error = std::fabs((y2_value - y0_value) - 2.0f * y_slope);
    if (!y0_ok || !y1_ok || !y2_ok || std::fabs(y_slope) < 0.00001f ||
        linear_error > 0.001f * (1.0f + std::fabs(y2_value))) {
      float nonlinear_y = 0.0f;
      ok = solve_relation_y(expression, x, nonlinear_y);
      return ok ? nonlinear_y : 0.0f;
    }

    const float y = -y0_value / y_slope;
    ok = std::isfinite(y);
    return y;
  }

  if (has_y) {
    ok = false;
    return 0.0f;
  }
  return evaluate_segment(graph_expression_body(expression), x, 0.0f, false, ok);
}

bool should_render_implicit(const char* expression) {
  const char* equals = expression == nullptr ? nullptr : std::strchr(expression, '=');
  if (equals == nullptr || !expression_references_y(expression)) {
    return false;
  }

  char left[constants::kGraphExpressionCapacity] {};
  char right[constants::kGraphExpressionCapacity] {};
  copy_expression_segment(left, expression, static_cast<size_t>(equals - expression));
  copy_expression_segment(right, equals + 1, std::strlen(equals + 1));
  return !expression_is_plain_y(left) && !expression_is_plain_y(right);
}

}  // namespace esp32calc_alt::graph_expression
