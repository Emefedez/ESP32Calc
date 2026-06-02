#include "ui/menu.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <strings.h>

#include "esp_log.h"
#include "hardware/keymap.h"
#include "ui/input_behavior.h"
#include "ui/menu_constants.h"
#include "ui/menu_detail.h"
#include "ui/menu_integrals.h"

namespace esp32calc_alt {

namespace {

namespace constants = menu_constants;
namespace integrals = menu_integrals;

bool is_solve_variable(char value) {
  for (size_t i = 0; i < kSolveVariableCount; ++i) {
    if (kSolveVariables[i] == value) {
      return true;
    }
  }
  return false;
}

uint8_t solve_variable_bit(char value) {
  for (size_t i = 0; i < kSolveVariableCount; ++i) {
    if (kSolveVariables[i] == value) {
      return static_cast<uint8_t>(1u << i);
    }
  }
  return 0;
}

bool is_known_function_name(const char* begin, size_t length) {
  static constexpr const char* kNames[] = {
      "abs", "acos", "asin", "atan", "cos", "det", "evalf", "exp", "graph",
      "int", "inv", "inverse", "ln", "log", "matrix", "sin", "solve", "sqrt", "tan",
  };
  for (const char* name : kNames) {
    if (std::strlen(name) != length) {
      continue;
    }
    bool same = true;
    for (size_t i = 0; i < length; ++i) {
      if (std::tolower(static_cast<unsigned char>(begin[i])) !=
          std::tolower(static_cast<unsigned char>(name[i]))) {
        same = false;
        break;
      }
    }
    if (same) {
      return true;
    }
  }
  return false;
}

bool function_name_before(const char* expression, size_t cursor, size_t& begin) {
  if (expression == nullptr || cursor == 0) {
    return false;
  }
  begin = cursor;
  while (begin > 0 && menu_detail::is_word_char(expression[begin - 1])) {
    --begin;
  }
  return begin < cursor && is_known_function_name(expression + begin, cursor - begin);
}

bool append_output(char* output, size_t output_size, size_t& used, char value) {
  if (used + 1 >= output_size) {
    return false;
  }
  output[used++] = value;
  output[used] = '\0';
  return true;
}

bool append_output(char* output, size_t output_size, size_t& used, const char* text, size_t length) {
  if (used + length >= output_size) {
    return false;
  }
  std::memcpy(output + used, text, length);
  used += length;
  output[used] = '\0';
  return true;
}

bool parse_number_span(const char* input, size_t& offset) {
  const size_t begin = offset;
  bool saw_digit = false;
  while (std::isdigit(static_cast<unsigned char>(input[offset])) != 0) {
    saw_digit = true;
    ++offset;
  }
  if (input[offset] == '.') {
    ++offset;
    while (std::isdigit(static_cast<unsigned char>(input[offset])) != 0) {
      saw_digit = true;
      ++offset;
    }
  }
  if (saw_digit && (input[offset] == 'E' || input[offset] == 'e')) {
    const size_t exponent = offset;
    ++offset;
    if (input[offset] == '+' || input[offset] == '-') {
      ++offset;
    }
    bool exponent_digit = false;
    while (std::isdigit(static_cast<unsigned char>(input[offset])) != 0) {
      exponent_digit = true;
      ++offset;
    }
    if (!exponent_digit) {
      offset = exponent;
    }
  }
  return offset > begin && saw_digit;
}

bool insert_implicit_multiplication(const char* input, char* output, size_t output_size) {
  if (output == nullptr || output_size == 0) {
    return false;
  }
  output[0] = '\0';
  if (input == nullptr) {
    return true;
  }

  size_t used = 0;
  bool previous_value = false;
  for (size_t i = 0; input[i] != '\0';) {
    const char ch = input[i];
    if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
      ++i;
      continue;
    }

    if (std::isdigit(static_cast<unsigned char>(ch)) != 0 || ch == '.') {
      const size_t begin = i;
      if (!parse_number_span(input, i)) {
        return false;
      }
      if (previous_value && !append_output(output, output_size, used, '*')) {
        return false;
      }
      if (!append_output(output, output_size, used, input + begin, i - begin)) {
        return false;
      }
      previous_value = true;
      continue;
    }

    if (std::isalpha(static_cast<unsigned char>(ch)) != 0) {
      const size_t begin = i;
      while (std::isalpha(static_cast<unsigned char>(input[i])) != 0) {
        ++i;
      }
      const size_t length = i - begin;
      const bool function_call = input[i] == '(' && is_known_function_name(input + begin, length);
      const bool split_variables =
          !function_call &&
          std::all_of(input + begin, input + i, [](char value) {
            return is_solve_variable(static_cast<char>(
                std::tolower(static_cast<unsigned char>(value))));
          });

      if (previous_value && !append_output(output, output_size, used, '*')) {
        return false;
      }
      if (split_variables) {
        for (size_t j = begin; j < i; ++j) {
          if (j > begin && !append_output(output, output_size, used, '*')) {
            return false;
          }
          const char variable =
              static_cast<char>(std::tolower(static_cast<unsigned char>(input[j])));
          if (!append_output(output, output_size, used, variable)) {
            return false;
          }
        }
      } else if (!append_output(output, output_size, used, input + begin, length)) {
        return false;
      }
      previous_value = !function_call;
      continue;
    }

    if (ch == '(' || ch == '[' || ch == '{') {
      if (previous_value && !append_output(output, output_size, used, '*')) {
        return false;
      }
      if (!append_output(output, output_size, used, ch)) {
        return false;
      }
      previous_value = false;
      ++i;
      continue;
    }

    if (!append_output(output, output_size, used, ch)) {
      return false;
    }
    previous_value = ch == ')' || ch == ']' || ch == '}';
    if (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '^' ||
        ch == '=' || ch == ',' || ch == ';') {
      previous_value = false;
    }
    ++i;
  }
  return true;
}

bool remove_empty_power_slots(const char* input, char* output, size_t output_size) {
  if (output == nullptr || output_size == 0) {
    return false;
  }
  output[0] = '\0';
  if (input == nullptr) {
    return true;
  }

  size_t used = 0;
  for (size_t i = 0; input[i] != '\0';) {
    if (input[i] == '^' && input[i + 1] == '(' && input[i + 2] == ')') {
      i += 3;
      continue;
    }
    if (input[i] == '^' && input[i + 1] == '[' && input[i + 2] == ']') {
      i += 3;
      continue;
    }
    if (!append_output(output, output_size, used, input[i])) {
      return false;
    }
    ++i;
  }
  return true;
}

bool expand_for_math(const char* input, char* output, size_t output_size) {
  char without_empty_slots[menu_constants::kExpandedExpressionCapacity] {};
  char constants_expanded[menu_constants::kExpandedExpressionCapacity] {};
  return remove_empty_power_slots(input,
                                  without_empty_slots,
                                  sizeof(without_empty_slots)) &&
         constants::expand_scientific_constants(without_empty_slots,
                                                constants_expanded,
                                                sizeof(constants_expanded)) &&
         insert_implicit_multiplication(constants_expanded, output, output_size);
}

void trim_span(const char*& begin, const char*& end) {
  while (begin < end && std::isspace(static_cast<unsigned char>(*begin)) != 0) {
    ++begin;
  }
  while (end > begin && std::isspace(static_cast<unsigned char>(*(end - 1))) != 0) {
    --end;
  }
}

bool copy_span(char* output, size_t output_size, const char* begin, const char* end) {
  if (output == nullptr || output_size == 0 || begin == nullptr || end < begin) {
    return false;
  }

  const size_t length = static_cast<size_t>(end - begin);
  if (length >= output_size) {
    return false;
  }
  std::memcpy(output, begin, length);
  output[length] = '\0';
  return true;
}

bool extract_named_call(const char* input, const char* name, char* body, size_t body_size) {
  if (!menu_detail::has_text(input) || !menu_detail::has_text(name)) {
    return false;
  }

  const char* begin = input;
  const char* end = input + std::strlen(input);
  trim_span(begin, end);

  const size_t name_len = std::strlen(name);
  if (static_cast<size_t>(end - begin) < name_len + 2 ||
      strncasecmp(begin, name, name_len) != 0 || begin[name_len] != '(' ||
      *(end - 1) != ')') {
    return false;
  }

  int depth = 0;
  for (const char* p = begin + name_len; p < end; ++p) {
    if (*p == '(' || *p == '[' || *p == '{') {
      ++depth;
    } else if (*p == ')' || *p == ']' || *p == '}') {
      --depth;
      if (depth == 0 && p != end - 1) {
        return false;
      }
      if (depth < 0) {
        return false;
      }
    }
  }

  const char* body_begin = begin + name_len + 1;
  const char* body_end = end - 1;
  trim_span(body_begin, body_end);
  return copy_span(body, body_size, body_begin, body_end);
}

bool split_first_top_level_arg(const char* body,
                               char* first,
                               size_t first_size,
                               char* second,
                               size_t second_size) {
  if (!menu_detail::has_text(body)) {
    return false;
  }

  const char* begin = body;
  const char* end = body + std::strlen(body);
  int paren_depth = 0;
  int bracket_depth = 0;
  int brace_depth = 0;

  for (const char* p = begin; p < end; ++p) {
    switch (*p) {
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
      case ',':
        if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
          const char* first_begin = begin;
          const char* first_end = p;
          const char* second_begin = p + 1;
          const char* second_end = end;
          trim_span(first_begin, first_end);
          trim_span(second_begin, second_end);
          return copy_span(first, first_size, first_begin, first_end) &&
                 copy_span(second, second_size, second_begin, second_end);
        }
        break;
      default:
        break;
    }
  }

  return copy_span(first, first_size, begin, end);
}

uint8_t solve_mask_from_variable_list(const char* text) {
  if (!menu_detail::has_text(text)) {
    return 0;
  }

  uint8_t mask = 0;
  for (const char* p = text; *p != '\0'; ++p) {
    const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
    mask |= solve_variable_bit(lower);
  }
  return mask;
}

uint8_t infer_solve_mask_from_expression(const char* expression) {
  uint8_t seen_mask = 0;
  uint8_t first_seen = 0;

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
    if (used == 1 && is_solve_variable(identifier[0])) {
      const uint8_t bit = solve_variable_bit(identifier[0]);
      seen_mask |= bit;
      if (first_seen == 0) {
        first_seen = bit;
      }
    }
  }

  return seen_mask != 0 ? seen_mask : first_seen;
}

bool expression_has_top_level_equals(const char* expression) {
  int paren_depth = 0;
  int bracket_depth = 0;
  int brace_depth = 0;
  for (const char* p = expression; p != nullptr && *p != '\0'; ++p) {
    switch (*p) {
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
          return true;
        }
        break;
      default:
        break;
    }
  }
  return false;
}

int integral_selected_ordinal(uint8_t group, const char* query, uint8_t selected_index) {
  int ordinal = 0;
  for (size_t i = 0; i < integrals::template_count(); ++i) {
    const auto& item = integrals::template_at(i);
    if (!integrals::template_matches(item, group, query)) {
      continue;
    }
    if (i == selected_index) {
      return ordinal;
    }
    ++ordinal;
  }
  return -1;
}

bool integral_query_has_text(const char* query) {
  return query != nullptr && query[0] != '\0';
}

int constant_selected_ordinal(uint8_t group, const char* query, uint8_t selected_index) {
  int ordinal = 0;
  for (size_t i = 0; i < constants::kScientificConstantCount; ++i) {
    const auto& item = constants::kScientificConstants[i];
    if (!constants::scientific_constant_matches(item, i, group, query)) {
      continue;
    }
    if (i == selected_index) {
      return ordinal;
    }
    ++ordinal;
  }
  return -1;
}

bool constant_query_has_text(const char* query) {
  return query != nullptr && query[0] != '\0';
}

bool expression_uses_natural_layout(const char* expression) {
  if (expression == nullptr) {
    return false;
  }
  return std::strchr(expression, '/') != nullptr ||
         std::strchr(expression, '^') != nullptr ||
         std::strstr(expression, "sqrt(") != nullptr;
}

size_t expression_operand_begin(const char* expression, size_t cursor) {
  if (expression == nullptr || cursor == 0) {
    return cursor;
  }

  size_t begin = cursor;
  if (expression[begin - 1] == ')') {
    int depth = 0;
    while (begin > 0) {
      --begin;
      if (expression[begin] == ')') {
        ++depth;
      } else if (expression[begin] == '(') {
        --depth;
        if (depth == 0) {
          size_t function_begin = begin;
          if (function_name_before(expression, begin, function_begin)) {
            begin = function_begin;
          }
          return begin;
        }
      }
    }
    return cursor;
  }

  while (begin > 0) {
    const char previous = expression[begin - 1];
    if (std::isalnum(static_cast<unsigned char>(previous)) == 0 && previous != '.') {
      break;
    }
    --begin;
  }
  if (begin > 0 && expression[begin - 1] == constants::kConstantMarker &&
      constants::find_scientific_constant(expression + begin, cursor - begin) != nullptr) {
    --begin;
  }
  return begin;
}

}  // namespace
void MenuUi::apply_standard_key(const KeyEvent& key) {
  const KeyDef& def = key_at(key.row, key.col);

  if (variable_palette_ != VariablePalette::None) {
    handle_variable_palette_key(key);
    return;
  }

  if (ui_behavior::kShiftVariableOpensSelector && key.shift && def.role == KeyRole::Variable) {
    open_variable_palette(VariablePalette::Plain);
    return;
  }

  if (ui_behavior::kShiftVariableSquareOpensSelector && key.shift &&
      def.role == KeyRole::VariableSquare) {
    open_variable_palette(VariablePalette::Square);
    return;
  }

  if (def.role == KeyRole::Clear) {
    if (key.shift) {
      clear_expression();
    } else {
      delete_expression_char();
    }
    status_ = expression_len_ == 0 ? "ENTER SENDS" : "EDIT";
    return;
  }

  if (def.role == KeyRole::Delete) {
    delete_expression_char();
    status_ = "EDIT";
    return;
  }

  if (def.role == KeyRole::Left) {
    move_cursor_left(key.shift);
    status_ = "EDIT";
    return;
  }

  if (def.role == KeyRole::Right) {
    move_cursor_right(key.shift);
    status_ = "EDIT";
    return;
  }

  if (def.role == KeyRole::Up) {
    cursor_ = 0;
    status_ = "EDIT";
    return;
  }

  if (def.role == KeyRole::Down) {
    cursor_ = expression_len_;
    status_ = "EDIT";
    return;
  }

  if (def.role == KeyRole::Enter) {
    if (ui_behavior::kShiftEqualsInsertsEquals && key.shift && key_is_equals(key)) {
      status_ = append_expression(ui_behavior::kShiftEqualsToken) ? "ENTER SENDS" : "EXPR FULL";
      return;
    }

    if (ui_behavior::kAlphaEqualsOpensGraph && key.alpha && key_is_equals(key)) {
      open_graph_from_expression();
      return;
    }

    submit_expression();
    return;
  }

  if (def.role == KeyRole::FractionToggle) {
    if (expression_len_ == 0) {
      status_ = "EMPTY EXPR";
    } else {
      submit_expression(!result_decimal_);
    }
    return;
  }

  if (!key.shift && !key.alpha && std::strcmp(def.label, "(/)") == 0) {
    status_ = insert_fraction_template() ? "FRACTION" : "EXPR FULL";
    return;
  }

  if (!key.shift && !key.alpha && std::strcmp(def.label, "xyz^a") == 0) {
    status_ = append_expression_at_cursor("^()", 2) ? "POWER" : "EXPR FULL";
    return;
  }

  if (!key.shift && !key.alpha && def.role == KeyRole::VariableSquare) {
    status_ = append_expression("x^(2)") ? "ENTER SENDS" : "EXPR FULL";
    return;
  }

  if (!key.shift && !key.alpha && std::strcmp(def.label, "INT") == 0) {
    open_mode(ModeKind::Integrals);
    return;
  }

  const char* token = key_input(def, key.shift, key.alpha);
  if (append_expression(token)) {
    status_ = "ENTER SENDS";
  } else if (menu_detail::has_text(token)) {
    status_ = "EXPR FULL";
  }
}
void MenuUi::apply_math_result(const MathResult& result) {
  std::snprintf(result_text_, sizeof(result_text_), "%s", result.text);
  result_visible_ = true;
  result_is_error_ = !result.ok;
  status_ = result.ok ? (result_decimal_ ? "DECIMAL" : "DONE") : "ERROR";
}
bool MenuUi::append_expression(const char* token) {
  return append_expression_at_cursor(token, token == nullptr ? 0 : std::strlen(token));
}
bool MenuUi::append_expression_at_cursor(const char* token, size_t token_cursor) {
  if (!menu_detail::has_text(token)) {
    return false;
  }

  const size_t token_len = std::strlen(token);
  if (token_cursor > token_len) {
    token_cursor = token_len;
  }
  if (expression_len_ + token_len >= sizeof(expression_)) {
    return false;
  }

  if (cursor_ > expression_len_) {
    cursor_ = expression_len_;
  }

  std::memmove(expression_ + cursor_ + token_len,
               expression_ + cursor_,
               expression_len_ - cursor_ + 1);
  std::memcpy(expression_ + cursor_, token, token_len);
  expression_len_ += token_len;
  cursor_ += token_cursor;
  expression_[expression_len_] = '\0';
  clear_result();
  return true;
}
bool MenuUi::insert_fraction_template() {
  if (cursor_ > expression_len_) {
    cursor_ = expression_len_;
  }

  const size_t numerator_begin = expression_operand_begin(expression_, cursor_);
  char numerator[48] {};
  const size_t numerator_len = cursor_ - numerator_begin;
  if (numerator_len > 0) {
    const size_t copy_len = std::min(numerator_len, sizeof(numerator) - 1);
    std::memcpy(numerator, expression_ + numerator_begin, copy_len);
    numerator[copy_len] = '\0';
  }

  char token[72] {};
  size_t new_cursor_offset = 1;
  if (numerator_len == 0) {
    std::snprintf(token, sizeof(token), "()/()");
  } else {
    std::snprintf(token, sizeof(token), "(%s)/()", numerator);
    new_cursor_offset = std::strlen(token) - 1;
  }

  const size_t token_len = std::strlen(token);
  if (expression_len_ - numerator_len + token_len >= sizeof(expression_)) {
    return false;
  }

  std::memmove(expression_ + numerator_begin + token_len,
               expression_ + cursor_,
               expression_len_ - cursor_ + 1);
  std::memcpy(expression_ + numerator_begin, token, token_len);
  expression_len_ = expression_len_ - numerator_len + token_len;
  cursor_ = numerator_begin + new_cursor_offset;
  expression_[expression_len_] = '\0';
  clear_result();
  return true;
}
void MenuUi::delete_expression_char() {
  if (expression_len_ == 0 || cursor_ == 0) {
    return;
  }

  if (cursor_ >= 2 && expression_[cursor_ - 2] == '^' &&
      (expression_[cursor_ - 1] == '(' || expression_[cursor_ - 1] == '[')) {
    const char close = expression_[cursor_ - 1] == '(' ? ')' : ']';
    if (cursor_ < expression_len_ && expression_[cursor_] != close) {
      std::memmove(expression_ + cursor_,
                   expression_ + cursor_ + 1,
                   expression_len_ - cursor_);
      --expression_len_;
      expression_[expression_len_] = '\0';
      clear_result();
      return;
    }
    if (cursor_ < expression_len_ && expression_[cursor_] == close) {
      const size_t delete_begin = cursor_ - 2;
      std::memmove(expression_ + delete_begin,
                   expression_ + cursor_ + 1,
                   expression_len_ - cursor_);
      expression_len_ -= 3;
      cursor_ = delete_begin;
      expression_[expression_len_] = '\0';
      clear_result();
      return;
    }
  }

  size_t delete_begin = cursor_ - 1;
  size_t token_begin = cursor_;
  while (token_begin > 0 &&
         std::isalnum(static_cast<unsigned char>(expression_[token_begin - 1])) != 0) {
    --token_begin;
  }
  if (token_begin > 0 && expression_[token_begin - 1] == constants::kConstantMarker &&
      constants::find_scientific_constant(expression_ + token_begin, cursor_ - token_begin) !=
          nullptr) {
    delete_begin = token_begin - 1;
  } else if (expression_[delete_begin] == '(' && delete_begin > 0 &&
      menu_detail::is_word_char(expression_[delete_begin - 1])) {
    size_t function_begin = delete_begin;
    if (function_name_before(expression_, delete_begin, function_begin)) {
      delete_begin = function_begin;
    }
  }

  const size_t delete_count = cursor_ - delete_begin;
  std::memmove(expression_ + delete_begin,
               expression_ + cursor_,
               expression_len_ - cursor_ + 1);
  expression_len_ -= delete_count;
  cursor_ = delete_begin;
  expression_[expression_len_] = '\0';
  clear_result();
}
void MenuUi::clear_expression() {
  expression_len_ = 0;
  cursor_ = 0;
  expression_[0] = '\0';
  variable_palette_ = VariablePalette::None;
  clear_result();
}
void MenuUi::clear_result() {
  result_text_[0] = '\0';
  result_visible_ = false;
  result_is_error_ = false;
  result_decimal_ = false;
}
void MenuUi::submit_expression(bool decimal_output) {
  if (expression_len_ == 0) {
    status_ = "EMPTY EXPR";
    return;
  }

  MathRequest request {};
  request.kind = MathJobKind::Numeric;
  char body[sizeof(request.expression)] {};
  char expanded_expression[sizeof(request.expression)] {};
  char first_arg[sizeof(request.expression)] {};
  char expanded_first_arg[sizeof(request.expression)] {};
  char second_arg[24] {};

  if (extract_named_call(expression_, "graph", body, sizeof(body))) {
    open_graph_expression(body);
    return;
  }

  if (!expand_for_math(expression_, expanded_expression, sizeof(expanded_expression))) {
    status_ = "EXPR FULL";
    return;
  }

  if (extract_named_call(expanded_expression, "solve", body, sizeof(body))) {
    request.kind = MathJobKind::Solve;
    if (split_first_top_level_arg(body,
                                  first_arg,
                                  sizeof(first_arg),
                                  second_arg,
                                  sizeof(second_arg))) {
      if (!expand_for_math(first_arg, expanded_first_arg, sizeof(expanded_first_arg))) {
        status_ = "EXPR FULL";
        return;
      }
      std::snprintf(request.expression, sizeof(request.expression), "%s", expanded_first_arg);
      request.solve_options.solve_mask = solve_mask_from_variable_list(second_arg);
    } else {
      std::snprintf(request.expression, sizeof(request.expression), "%s", body);
    }
    if (request.solve_options.solve_mask == 0) {
      request.solve_options.solve_mask = infer_solve_mask_from_expression(request.expression);
    }
  } else if (extract_named_call(expanded_expression, "det", body, sizeof(body)) ||
             extract_named_call(expanded_expression, "inv", body, sizeof(body)) ||
             extract_named_call(expanded_expression, "inverse", body, sizeof(body)) ||
             extract_named_call(expanded_expression, "matrix", body, sizeof(body))) {
    request.kind = MathJobKind::Script;
    std::snprintf(request.expression, sizeof(request.expression), "%s", expanded_expression);
  } else {
    const bool equation = expression_has_top_level_equals(expanded_expression);
    if (decimal_output && !equation) {
      if (std::strlen(expanded_expression) + std::strlen("evalf()") >= sizeof(request.expression)) {
        status_ = "EXPR FULL";
        return;
      }
      std::snprintf(request.expression, sizeof(request.expression), "evalf(");
      std::strncat(request.expression,
                   expanded_expression,
                   sizeof(request.expression) - std::strlen(request.expression) - 2);
      std::strncat(request.expression,
                   ")",
                   sizeof(request.expression) - std::strlen(request.expression) - 1);
    } else {
      std::snprintf(request.expression, sizeof(request.expression), "%s", expanded_expression);
    }
    if (equation) {
      request.solve_options.solve_mask = infer_solve_mask_from_expression(expanded_expression);
    }
  }

  if (math_.submit(request)) {
    result_text_[0] = '\0';
    result_visible_ = false;
    result_is_error_ = false;
    result_decimal_ = decimal_output;
    status_ = decimal_output ? "DECIMAL QUEUED" : "QUEUED";
    ESP_LOGI(constants::kLogTag, "queued expression: %s", request.expression);
  } else {
    status_ = "QUEUE FULL";
  }
}
void MenuUi::open_variable_palette(VariablePalette palette) {
  variable_palette_ = palette;
  variable_selected_ = 0;
  status_ = palette == VariablePalette::Square ? "PICK VAR^2" : "PICK VAR";
}
void MenuUi::handle_variable_palette_key(const KeyEvent& key) {
  const KeyDef& def = key_at(key.row, key.col);
  const int digit = key_digit(def);
  if (digit >= 0 && static_cast<size_t>(digit) < constants::kVariableCount) {
    variable_selected_ = static_cast<uint8_t>(digit);
    choose_selected_variable();
    return;
  }

  switch (def.role) {
    case KeyRole::Left:
      move_variable_selection(-1);
      break;
    case KeyRole::Right:
      move_variable_selection(1);
      break;
    case KeyRole::Up:
      move_variable_selection(-3);
      break;
    case KeyRole::Down:
      move_variable_selection(3);
      break;
    case KeyRole::Enter:
      choose_selected_variable();
      break;
    case KeyRole::Clear:
    case KeyRole::Mode:
      variable_palette_ = VariablePalette::None;
      status_ = "ENTER SENDS";
      break;
    default:
      break;
  }
}
void MenuUi::choose_selected_variable() {
  char token[5] {};
  if (variable_palette_ == VariablePalette::Square) {
    char power_token[8] {};
    std::snprintf(power_token,
                  sizeof(power_token),
                  "%s^(2)",
                  constants::kVariableTokens[variable_selected_]);
    variable_palette_ = VariablePalette::None;
    status_ = append_expression(power_token) ? "ENTER SENDS" : "EXPR FULL";
    return;
  } else {
    std::snprintf(token, sizeof(token), "%s", constants::kVariableTokens[variable_selected_]);
  }

  variable_palette_ = VariablePalette::None;
  status_ = append_expression(token) ? "ENTER SENDS" : "EXPR FULL";
}
void MenuUi::move_variable_selection(int delta) {
  const int count = static_cast<int>(constants::kVariableCount);
  int next = static_cast<int>(variable_selected_) + delta;
  while (next < 0) {
    next += count;
  }
  variable_selected_ = static_cast<uint8_t>(next % count);
}
void MenuUi::apply_constants_key(const KeyEvent& key) {
  const KeyDef& def = key_at(key.row, key.col);
  const int digit = key_digit(def);

  if (constant_stage_ == ConstantMenuStage::Groups) {
    if (digit >= 0 && static_cast<size_t>(digit) < constants::kScientificConstantGroupCount) {
      open_constant_group(static_cast<uint8_t>(digit));
      return;
    }

    switch (def.role) {
      case KeyRole::Left:
      case KeyRole::Up:
        move_constant_group_selection(-1);
        status_ = "PICK GROUP";
        return;
      case KeyRole::Right:
      case KeyRole::Down:
        move_constant_group_selection(1);
        status_ = "PICK GROUP";
        return;
      case KeyRole::Enter:
        open_constant_group(constant_group_selected_);
        return;
      case KeyRole::Clear:
        open_mode(ModeKind::Standard);
        return;
      default:
        return;
    }
  }

  if (def.role == KeyRole::Clear) {
    if (constant_query_has_text(constant_search_)) {
      backspace_constant_search();
    } else {
      constant_stage_ = ConstantMenuStage::Groups;
      status_ = "PICK GROUP";
    }
    return;
  }

  if (def.role == KeyRole::Delete) {
    backspace_constant_search();
    return;
  }

  switch (def.role) {
    case KeyRole::Left:
      move_constant_group_selection(-1);
      open_constant_group(constant_group_selected_);
      return;
    case KeyRole::Right:
      move_constant_group_selection(1);
      open_constant_group(constant_group_selected_);
      return;
    case KeyRole::Up:
      move_constant_item_selection(-1);
      return;
    case KeyRole::Down:
      move_constant_item_selection(1);
      return;
    case KeyRole::Enter:
      choose_selected_constant();
      return;
    default:
      break;
  }

  const char* token = key_input(def, key.shift, key.alpha);
  append_constant_search_token(token);
}
void MenuUi::open_constant_group(uint8_t group) {
  constant_group_selected_ = group;
  constant_stage_ = ConstantMenuStage::Items;
  clear_constant_search();
  const int first = constants::first_scientific_constant_for_group(constant_group_selected_);
  if (first >= 0) {
    constant_selected_ = static_cast<uint8_t>(first);
  }
  status_ = "SEARCH / = COPY";
}
void MenuUi::choose_selected_constant() {
  if (constant_selected_ >= constants::kScientificConstantCount ||
      !constants::scientific_constant_matches(constants::kScientificConstants[constant_selected_],
                                              constant_selected_,
                                              constant_group_selected_,
                                              constant_search_)) {
    status_ = "NO MATCH";
    return;
  }

  const auto& constant = constants::kScientificConstants[constant_selected_];
  char token[16] {};
  std::snprintf(token, sizeof(token), "%c%s", constants::kConstantMarker, constant.label);
  const bool inserted = append_expression(token);
  open_mode(ModeKind::Standard);
  status_ = inserted ? constant.label : "EXPR FULL";
}
void MenuUi::move_constant_group_selection(int delta) {
  const int count = static_cast<int>(constants::kScientificConstantGroupCount);
  int next = static_cast<int>(constant_group_selected_) + delta;
  while (next < 0) {
    next += count;
  }
  constant_group_selected_ = static_cast<uint8_t>(next % count);
}
void MenuUi::move_constant_item_selection(int delta) {
  const size_t count =
      constants::filtered_scientific_constant_count(constant_group_selected_, constant_search_);
  if (count == 0) {
    status_ = "NO MATCH";
    return;
  }

  int ordinal = constant_selected_ordinal(constant_group_selected_, constant_search_, constant_selected_);
  if (ordinal < 0) {
    ordinal = 0;
  } else {
    ordinal += delta;
  }
  while (ordinal < 0) {
    ordinal += static_cast<int>(count);
  }
  ordinal %= static_cast<int>(count);

  const int selected =
      constants::filtered_scientific_constant_index(constant_group_selected_, constant_search_, ordinal);
  if (selected >= 0) {
    constant_selected_ = static_cast<uint8_t>(selected);
    status_ = "SEARCH / = COPY";
  }
}
void MenuUi::clear_constant_search() {
  constant_search_[0] = '\0';
}
void MenuUi::backspace_constant_search() {
  const size_t len = std::strlen(constant_search_);
  if (len == 0) {
    status_ = "SEARCH / = COPY";
    return;
  }
  constant_search_[len - 1] = '\0';
  sync_constant_selection_to_filter();
}
void MenuUi::append_constant_search_token(const char* token) {
  char clean[8] {};
  constants::sanitize_constant_search_token(token, clean, sizeof(clean));
  if (!menu_detail::has_text(clean)) {
    return;
  }

  const size_t query_len = std::strlen(constant_search_);
  const size_t clean_len = std::strlen(clean);
  const size_t available = sizeof(constant_search_) - query_len - 1;
  if (available == 0) {
    status_ = "SEARCH FULL";
    return;
  }
  std::strncat(constant_search_, clean, available < clean_len ? available : clean_len);
  sync_constant_selection_to_filter();
}
void MenuUi::sync_constant_selection_to_filter() {
  if (constant_selected_ < constants::kScientificConstantCount &&
      constants::scientific_constant_matches(constants::kScientificConstants[constant_selected_],
                                             constant_selected_,
                                             constant_group_selected_,
                                             constant_search_)) {
    status_ = "SEARCH / = COPY";
    return;
  }

  const int first =
      constants::filtered_scientific_constant_index(constant_group_selected_, constant_search_, 0);
  if (first >= 0) {
    constant_selected_ = static_cast<uint8_t>(first);
    status_ = "SEARCH / = COPY";
  } else {
    status_ = "NO MATCH";
  }
}
void MenuUi::apply_integrals_key(const KeyEvent& key) {
  const KeyDef& def = key_at(key.row, key.col);
  const int digit = key_digit(def);

  if (integral_stage_ == IntegralMenuStage::Groups) {
    if (digit >= 0 && static_cast<size_t>(digit) < integrals::group_count()) {
      open_integral_group(static_cast<uint8_t>(digit));
      return;
    }

    switch (def.role) {
      case KeyRole::Left:
      case KeyRole::Up:
        move_integral_group_selection(-1);
        status_ = "PICK GROUP";
        return;
      case KeyRole::Right:
      case KeyRole::Down:
        move_integral_group_selection(1);
        status_ = "PICK GROUP";
        return;
      case KeyRole::Enter:
        open_integral_group(integral_group_selected_);
        return;
      case KeyRole::Clear:
        open_mode(ModeKind::Standard);
        return;
      default:
        return;
    }
  }

  if (def.role == KeyRole::Clear) {
    if (integral_query_has_text(integral_search_)) {
      backspace_integral_search();
    } else {
      integral_stage_ = IntegralMenuStage::Groups;
      status_ = "PICK GROUP";
    }
    return;
  }

  if (def.role == KeyRole::Delete) {
    backspace_integral_search();
    return;
  }

  switch (def.role) {
    case KeyRole::Left:
      move_integral_group_selection(-1);
      open_integral_group(integral_group_selected_);
      return;
    case KeyRole::Right:
      move_integral_group_selection(1);
      open_integral_group(integral_group_selected_);
      return;
    case KeyRole::Up:
      move_integral_item_selection(-1);
      return;
    case KeyRole::Down:
      move_integral_item_selection(1);
      return;
    case KeyRole::Enter:
      choose_selected_integral();
      return;
    default:
      break;
  }

  const char* token = key_input(def, key.shift, key.alpha);
  append_integral_search_token(token);
}
void MenuUi::open_integral_group(uint8_t group) {
  integral_group_selected_ = group;
  integral_stage_ = IntegralMenuStage::Items;
  clear_integral_search();
  const int first = integrals::first_template_for_group(integral_group_selected_);
  if (first >= 0) {
    integral_selected_ = static_cast<uint8_t>(first);
  }
  status_ = "SEARCH / = COPY";
}
void MenuUi::choose_selected_integral() {
  if (integral_selected_ >= integrals::template_count() ||
      !integrals::template_matches(integrals::template_at(integral_selected_),
                                   integral_group_selected_,
                                   integral_search_)) {
    status_ = "NO MATCH";
    return;
  }

  const auto& item = integrals::template_at(integral_selected_);
  const bool inserted = append_expression_at_cursor(item.token, item.cursor);
  open_mode(ModeKind::Standard);
  status_ = inserted ? item.label : "EXPR FULL";
}
void MenuUi::move_integral_group_selection(int delta) {
  const int count = static_cast<int>(integrals::group_count());
  int next = static_cast<int>(integral_group_selected_) + delta;
  while (next < 0) {
    next += count;
  }
  integral_group_selected_ = static_cast<uint8_t>(next % count);
}
void MenuUi::move_integral_item_selection(int delta) {
  const size_t count =
      integrals::filtered_template_count(integral_group_selected_, integral_search_);
  if (count == 0) {
    status_ = "NO MATCH";
    return;
  }

  int ordinal = integral_selected_ordinal(integral_group_selected_, integral_search_, integral_selected_);
  if (ordinal < 0) {
    ordinal = 0;
  } else {
    ordinal += delta;
  }
  while (ordinal < 0) {
    ordinal += static_cast<int>(count);
  }
  ordinal %= static_cast<int>(count);

  const int selected =
      integrals::filtered_template_index(integral_group_selected_, integral_search_, ordinal);
  if (selected >= 0) {
    integral_selected_ = static_cast<uint8_t>(selected);
    status_ = "SEARCH / = COPY";
  }
}
void MenuUi::clear_integral_search() {
  integral_search_[0] = '\0';
}
void MenuUi::backspace_integral_search() {
  const size_t len = std::strlen(integral_search_);
  if (len == 0) {
    status_ = "SEARCH / = COPY";
    return;
  }
  integral_search_[len - 1] = '\0';
  sync_integral_selection_to_filter();
}
void MenuUi::append_integral_search_token(const char* token) {
  char clean[8] {};
  integrals::sanitize_search_token(token, clean, sizeof(clean));
  if (!menu_detail::has_text(clean)) {
    return;
  }

  const size_t query_len = std::strlen(integral_search_);
  const size_t clean_len = std::strlen(clean);
  const size_t available = sizeof(integral_search_) - query_len - 1;
  if (available == 0) {
    status_ = "SEARCH FULL";
    return;
  }
  std::strncat(integral_search_, clean, available < clean_len ? available : clean_len);
  sync_integral_selection_to_filter();
}
void MenuUi::sync_integral_selection_to_filter() {
  if (integral_selected_ < integrals::template_count() &&
      integrals::template_matches(integrals::template_at(integral_selected_),
                                  integral_group_selected_,
                                  integral_search_)) {
    status_ = "SEARCH / = COPY";
    return;
  }

  const int first = integrals::filtered_template_index(integral_group_selected_, integral_search_, 0);
  if (first >= 0) {
    integral_selected_ = static_cast<uint8_t>(first);
    status_ = "SEARCH / = COPY";
  } else {
    status_ = "NO MATCH";
  }
}
void MenuUi::move_cursor_left(bool all_the_way) {
  if (all_the_way) {
    cursor_ = 0;
  } else if (cursor_ > 0) {
    if (cursor_ >= 2 && expression_[cursor_ - 2] == '^' &&
        (expression_[cursor_ - 1] == '(' || expression_[cursor_ - 1] == '[')) {
      cursor_ -= 2;
      return;
    }
    if (cursor_ < expression_len_ && expression_[cursor_ - 1] == '^' &&
        (expression_[cursor_] == '(' || expression_[cursor_] == '[')) {
      --cursor_;
      return;
    }
    if (cursor_ >= 3 && expression_[cursor_ - 1] == '(' &&
        expression_[cursor_ - 2] == '/' && expression_[cursor_ - 3] == ')') {
      cursor_ -= 3;
      return;
    }
    size_t next = cursor_;
    size_t token_begin = next;
    while (token_begin > 0 &&
           std::isalnum(static_cast<unsigned char>(expression_[token_begin - 1])) != 0) {
      --token_begin;
    }
    if (token_begin > 0 && expression_[token_begin - 1] == constants::kConstantMarker &&
        constants::find_scientific_constant(expression_ + token_begin, next - token_begin) !=
            nullptr) {
      cursor_ = token_begin - 1;
      return;
    }
    if (expression_[next - 1] == '(' && next > 1 && menu_detail::is_word_char(expression_[next - 2])) {
      --next;
      size_t function_begin = next;
      if (function_name_before(expression_, next, function_begin)) {
        cursor_ = function_begin;
        return;
      }
    }
    --cursor_;
  }
}
void MenuUi::move_cursor_right(bool all_the_way) {
  if (all_the_way) {
    cursor_ = expression_len_;
  } else if (cursor_ < expression_len_) {
    size_t next = cursor_;
    if (next + 1 < expression_len_ && expression_[next] == '^' &&
        (expression_[next + 1] == '(' || expression_[next + 1] == '[')) {
      cursor_ = next + 2;
      return;
    }
    if (next > 0 && (expression_[next] == '(' || expression_[next] == '[') &&
        expression_[next - 1] == '^') {
      cursor_ = next + 1;
      return;
    }
    if (next + 2 < expression_len_ && expression_[next] == ')' &&
        expression_[next + 1] == '/' && expression_[next + 2] == '(') {
      cursor_ = next + 3;
      return;
    }
    if (expression_[next] == ')' && next > 0) {
      cursor_ = next + 1;
      return;
    }
    if (expression_[next] == constants::kConstantMarker) {
      size_t token_end = next + 1;
      while (token_end < expression_len_ &&
             std::isalnum(static_cast<unsigned char>(expression_[token_end])) != 0) {
        ++token_end;
      }
      if (constants::find_scientific_constant(expression_ + next + 1, token_end - next - 1) !=
          nullptr) {
        cursor_ = token_end;
        return;
      }
    }
    if (menu_detail::is_word_char(expression_[next])) {
      const size_t begin = next;
      while (next < expression_len_ && menu_detail::is_word_char(expression_[next])) {
        ++next;
      }
      if (next < expression_len_ && expression_[next] == '(' &&
          is_known_function_name(expression_ + begin, next - begin)) {
        ++next;
        cursor_ = next;
        return;
      }
      cursor_ = begin + 1;
      return;
    }
    ++cursor_;
  }
}
size_t MenuUi::expression_visible_start() const {
  if (expression_len_ <= constants::kVisibleExpressionChars) {
    return 0;
  }

  if (cursor_ < constants::kVisibleExpressionChars) {
    return 0;
  }

  const size_t max_start = expression_len_ - constants::kVisibleExpressionChars;
  return std::min(cursor_ - constants::kVisibleExpressionChars + 1, max_start);
}
bool MenuUi::key_is_equals(const KeyEvent& key) const {
  const KeyDef& def = key_at(key.row, key.col);
  return std::strcmp(def.label, "=") == 0;
}
void MenuUi::render_standard() {
  if (result_visible_) {
    canvas_.draw_text(6, 23, result_is_error_ ? "ERR" : "=", 1, true);
    const size_t result_len = std::strlen(result_text_);
    size_t source_offset = 0;
    for (size_t line = 0; line < 3 && source_offset < result_len; ++line) {
      char line_text[constants::kVisibleResultChars + 1] {};
      size_t line_len = 0;
      while (source_offset < result_len && result_text_[source_offset] == ' ') {
        ++source_offset;
      }
      while (source_offset < result_len && line_len < constants::kVisibleResultChars) {
        const char ch = result_text_[source_offset];
        if (ch == '\n' || ch == ';') {
          ++source_offset;
          break;
        }
        line_text[line_len++] = ch;
        ++source_offset;
      }
      line_text[line_len] = '\0';
      while (source_offset < result_len && result_text_[source_offset] == ' ') {
        ++source_offset;
      }
      canvas_.draw_text(28, 30 + static_cast<int>(line) * 14, line_text, 1, true);
    }
  }

  canvas_.hline(5, constants::kInputY, 240, true);

  const size_t visible_start = expression_visible_start();
  const size_t visible_count =
      expression_len_ > visible_start
          ? std::min(constants::kVisibleExpressionChars, expression_len_ - visible_start)
          : 0;
  char visible_expression[constants::kVisibleExpressionChars + 1] {};
  if (visible_count > 0) {
    std::memcpy(visible_expression, expression_ + visible_start, visible_count);
  }
  const size_t cursor_relative = cursor_ > visible_start ? cursor_ - visible_start : 0;
  const size_t cursor_cells =
      expression_len_ == 0 ? 1 : std::min(cursor_relative, constants::kVisibleExpressionChars);
  const bool natural_layout = expression_uses_natural_layout(visible_expression);
  const char* input_text = expression_len_ == 0 ? "0" : visible_expression;
  const int input_right = constants::kInputX + constants::kInputWidth - 8;
  const int available_width = input_right - constants::kInputTextX;
  if (!natural_layout) {
    const int text_cells = static_cast<int>(std::strlen(input_text));
    const uint8_t input_scale =
        text_cells * constants::kCharWidth * 2 <= available_width ? 2 : 1;
    const int char_width = constants::kCharWidth * input_scale;
    const int text_width = text_cells * char_width;
    const int text_x = std::max(constants::kInputTextX, input_right - text_width);
    canvas_.draw_text(text_x, constants::kInputTextY, input_text, input_scale, true);
    const int cursor_x =
        expression_len_ == 0
            ? text_x + char_width
            : text_x + static_cast<int>(cursor_cells) * char_width;
    if (cursor_visible_) {
      canvas_.vline(cursor_x,
                    constants::kInputTextY - (input_scale == 2 ? 6 : 4),
                    input_scale == 2 ? 24 : 15,
                    true);
    }
  } else {
    const bool has_fraction = std::strchr(input_text, '/') != nullptr;
    const uint8_t input_scale =
        !has_fraction &&
                menu_detail::math_text_width(input_text, std::strlen(input_text), 2) <=
                    available_width
            ? 2
            : 1;
    const int text_width =
        menu_detail::math_text_width(input_text, std::strlen(input_text), input_scale);
    const int text_x = std::max(constants::kInputTextX, input_right - text_width);
    menu_detail::draw_math_text(canvas_,
                                text_x,
                                constants::kInputTextY,
                                input_text,
                                input_scale);
    if (cursor_visible_) {
      menu_detail::draw_math_cursor(canvas_,
                                    text_x,
                                    constants::kInputTextY,
                                    visible_expression,
                                    cursor_cells,
                                    input_scale);
    }
  }

  canvas_.hline(5, 108, 240, true);
  canvas_.draw_text(6, 116, status_, 1, true);

  if (variable_palette_ != VariablePalette::None) {
    render_variable_palette();
  }
}
void MenuUi::render_constants() {
  canvas_.draw_text(6, 17, "CONSTANTS", 1, true);

  if (constant_stage_ == ConstantMenuStage::Groups) {
    for (size_t i = 0; i < constants::kScientificConstantGroupCount; ++i) {
      const int y = constants::kConstantsListY +
                    static_cast<int>(i) * constants::kConstantsRowHeight;
      const bool selected = i == constant_group_selected_;
      if (selected) {
        canvas_.fill_rect(constants::kConstantsListX - 3,
                          y - 3,
                          constants::kConstantsRowWidth,
                          11,
                          true);
      }

      const auto& group = constants::constant_group_at(i);
      char row_text[40] {};
      std::snprintf(row_text,
                    sizeof(row_text),
                    "%u %-10s %s",
                    static_cast<unsigned>(i),
                    group.label,
                    group.hint);
      canvas_.draw_text(constants::kConstantsListX, y, row_text, 1, !selected);
    }
    canvas_.draw_text(6, 112, "INDEX  ARROWS  ENTER", 1, true);
    return;
  }

  const auto& group = constants::constant_group_at(constant_group_selected_);
  char title[40] {};
  std::snprintf(title,
                sizeof(title),
                "%s  FIND:%s",
                group.label,
                constant_query_has_text(constant_search_) ? constant_search_ : "-");
  canvas_.draw_text(78, 17, title, 1, true);

  const size_t match_count =
      constants::filtered_scientific_constant_count(constant_group_selected_, constant_search_);
  if (match_count == 0) {
    canvas_.draw_text(6, 54, "NO MATCH", 1, true);
    canvas_.draw_text(6, 112, "TYPE TO FILTER  AC BACK", 1, true);
    return;
  }

  int selected_ordinal =
      constant_selected_ordinal(constant_group_selected_, constant_search_, constant_selected_);
  if (selected_ordinal < 0) {
    selected_ordinal = 0;
  }

  size_t first_ordinal = 0;
  if (selected_ordinal >= static_cast<int>(constants::kConstantsVisibleRows / 2)) {
    first_ordinal =
        static_cast<size_t>(selected_ordinal) - constants::kConstantsVisibleRows / 2;
  }
  if (first_ordinal + constants::kConstantsVisibleRows > match_count) {
    first_ordinal = match_count > constants::kConstantsVisibleRows
                        ? match_count - constants::kConstantsVisibleRows
                        : 0;
  }

  for (size_t row = 0; row < constants::kConstantsVisibleRows; ++row) {
    const size_t ordinal = first_ordinal + row;
    if (ordinal >= match_count) {
      break;
    }
    const int index = constants::filtered_scientific_constant_index(constant_group_selected_,
                                                                    constant_search_,
                                                                    ordinal);
    if (index < 0) {
      break;
    }
    const auto& constant = constants::kScientificConstants[static_cast<size_t>(index)];
    const int y = constants::kConstantsListY +
                  static_cast<int>(row) * constants::kConstantsRowHeight;
    const bool selected = index == constant_selected_;
    if (selected) {
      canvas_.fill_rect(constants::kConstantsListX - 3,
                        y - 3,
                        constants::kConstantsRowWidth,
                        11,
                        true);
    }

    char code[4] {};
    constants::scientific_constant_code(constant_group_selected_,
                                        static_cast<size_t>(index),
                                        code,
                                        sizeof(code));
    char row_text[40] {};
    std::snprintf(row_text,
                  sizeof(row_text),
                  "%s %-8s %-8s",
                  code,
                  constant.label,
                  constant.value);
    canvas_.draw_text(constants::kConstantsListX, y, row_text, 1, !selected);
  }

    canvas_.draw_text(6, 112, "= COPY  FIND  AC BACK", 1, true);
}
void MenuUi::render_integrals() {
  canvas_.draw_text(6, 17, "INTEGRALS", 1, true);

  if (integral_stage_ == IntegralMenuStage::Groups) {
    for (size_t i = 0; i < integrals::group_count(); ++i) {
      const int y = constants::kConstantsListY +
                    static_cast<int>(i) * constants::kConstantsRowHeight;
      const bool selected = i == integral_group_selected_;
      if (selected) {
        canvas_.fill_rect(constants::kConstantsListX - 3,
                          y - 3,
                          constants::kConstantsRowWidth,
                          11,
                          true);
      }

      const auto& group = integrals::group_at(i);
      char row_text[40] {};
      std::snprintf(row_text,
                    sizeof(row_text),
                    "%u %-10s %s",
                    static_cast<unsigned>(i),
                    group.label,
                    group.hint);
      canvas_.draw_text(constants::kConstantsListX, y, row_text, 1, !selected);
    }
    canvas_.draw_text(6, 112, "INDEX  ARROWS  ENTER", 1, true);
    return;
  }

  const auto& group = integrals::group_at(integral_group_selected_);
  char title[40] {};
  std::snprintf(title,
                sizeof(title),
                "%s  FIND:%s",
                group.label,
                integral_query_has_text(integral_search_) ? integral_search_ : "-");
  canvas_.draw_text(72, 17, title, 1, true);

  const size_t match_count =
      integrals::filtered_template_count(integral_group_selected_, integral_search_);
  if (match_count == 0) {
    canvas_.draw_text(6, 54, "NO MATCH", 1, true);
    canvas_.draw_text(6, 112, "TYPE TO FILTER  AC BACK", 1, true);
    return;
  }

  int selected_ordinal =
      integral_selected_ordinal(integral_group_selected_, integral_search_, integral_selected_);
  if (selected_ordinal < 0) {
    selected_ordinal = 0;
  }

  size_t first_ordinal = 0;
  if (selected_ordinal >= static_cast<int>(constants::kConstantsVisibleRows / 2)) {
    first_ordinal =
        static_cast<size_t>(selected_ordinal) - constants::kConstantsVisibleRows / 2;
  }
  if (first_ordinal + constants::kConstantsVisibleRows > match_count) {
    first_ordinal = match_count > constants::kConstantsVisibleRows
                        ? match_count - constants::kConstantsVisibleRows
                        : 0;
  }

  for (size_t row = 0; row < constants::kConstantsVisibleRows; ++row) {
    const size_t ordinal = first_ordinal + row;
    if (ordinal >= match_count) {
      break;
    }
    const int index =
        integrals::filtered_template_index(integral_group_selected_, integral_search_, ordinal);
    if (index < 0) {
      break;
    }
    const auto& item = integrals::template_at(static_cast<size_t>(index));
    const int y = constants::kConstantsListY +
                  static_cast<int>(row) * constants::kConstantsRowHeight;
    const bool selected = index == integral_selected_;
    if (selected) {
      canvas_.fill_rect(constants::kConstantsListX - 3,
                        y - 3,
                        constants::kConstantsRowWidth,
                        11,
                        true);
    }

    char row_text[44] {};
    std::snprintf(row_text, sizeof(row_text), "%s %-18s", item.code, item.label);
    canvas_.draw_text(constants::kConstantsListX, y, row_text, 1, !selected);
  }

  if (integral_selected_ < integrals::template_count()) {
    const auto& item = integrals::template_at(integral_selected_);
    char token_text[42] {};
    std::snprintf(token_text, sizeof(token_text), "%s", item.token);
    canvas_.draw_text(6, 96, token_text, 1, true);
  }
  canvas_.draw_text(6, 112, "= COPY  FIND  AC BACK", 1, true);
}
void MenuUi::render_variable_palette() {
  canvas_.fill_rect(73, 31, 104, 72, false);
  canvas_.rect(72, 30, 106, 74, true);
  canvas_.draw_text(82,
                    38,
                    variable_palette_ == VariablePalette::Square ? "VAR^2" : "VAR",
                    1,
                    true);

  for (size_t i = 0; i < constants::kVariableCount; ++i) {
    const int x = 85 + static_cast<int>(i % 3) * 28;
    const int y = 57 + static_cast<int>(i / 3) * 22;
    const bool selected = i == variable_selected_;
    if (selected) {
      canvas_.fill_rect(x - 6, y - 5, 22, 17, true);
    } else {
      canvas_.rect(x - 6, y - 5, 22, 17, true);
    }
    canvas_.draw_text(x, y, constants::kVariableTokens[i], 1, !selected);
  }
}

}  // namespace esp32calc_alt
