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

bool expand_for_math(const char* input, char* output, size_t output_size) {
  return constants::expand_scientific_constants(input, output, output_size);
}

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

  if ((seen_mask & kSolveVariableY) != 0) {
    return kSolveVariableY;
  }
  return first_seen;
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
    status_ = "S<>D TODO";
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
  status_ = result.ok ? "DONE" : "ERROR";
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
void MenuUi::delete_expression_char() {
  if (expression_len_ == 0 || cursor_ == 0) {
    return;
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
    while (delete_begin > 0 && menu_detail::is_word_char(expression_[delete_begin - 1])) {
      --delete_begin;
    }
  } else if (menu_detail::is_word_char(expression_[delete_begin])) {
    while (delete_begin > 0 && menu_detail::is_word_char(expression_[delete_begin - 1])) {
      --delete_begin;
    }
  } else if (delete_begin > 0 && expression_[delete_begin - 1] == '^') {
    --delete_begin;
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
}
void MenuUi::submit_expression() {
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
    std::snprintf(request.expression, sizeof(request.expression), "%s", expanded_expression);
    if (expression_has_top_level_equals(expanded_expression)) {
      request.solve_options.solve_mask = infer_solve_mask_from_expression(expanded_expression);
    }
  }

  if (math_.submit(request)) {
    clear_result();
    status_ = "QUEUED";
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
    std::snprintf(token, sizeof(token), "%s^2", constants::kVariableTokens[variable_selected_]);
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
  if (digit >= 0 && static_cast<size_t>(digit) < constants::kScientificConstantCount) {
    constant_selected_ = static_cast<uint8_t>(digit);
    choose_selected_constant();
    return;
  }

  switch (def.role) {
    case KeyRole::Left:
      move_constant_selection(-static_cast<int>(constants::kConstantsVisibleRows));
      status_ = "PICK CONST";
      break;
    case KeyRole::Right:
      move_constant_selection(static_cast<int>(constants::kConstantsVisibleRows));
      status_ = "PICK CONST";
      break;
    case KeyRole::Up:
      move_constant_selection(-1);
      status_ = "PICK CONST";
      break;
    case KeyRole::Down:
      move_constant_selection(1);
      status_ = "PICK CONST";
      break;
    case KeyRole::Enter:
      choose_selected_constant();
      break;
    case KeyRole::Clear:
      open_mode(ModeKind::Standard);
      break;
    default:
      break;
  }
}
void MenuUi::choose_selected_constant() {
  const auto& constant = constants::kScientificConstants[constant_selected_];
  char token[16] {};
  std::snprintf(token, sizeof(token), "%c%s", constants::kConstantMarker, constant.label);
  const bool inserted = append_expression(token);
  open_mode(ModeKind::Standard);
  status_ = inserted ? constant.label : "EXPR FULL";
}
void MenuUi::move_constant_selection(int delta) {
  const int count = static_cast<int>(constants::kScientificConstantCount);
  int next = static_cast<int>(constant_selected_) + delta;
  while (next < 0) {
    next += count;
  }
  constant_selected_ = static_cast<uint8_t>(next % count);
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
      while (next > 0 && menu_detail::is_word_char(expression_[next - 1])) {
        --next;
      }
      cursor_ = next;
      return;
    }
    if (menu_detail::is_word_char(expression_[next - 1])) {
      while (next > 0 && menu_detail::is_word_char(expression_[next - 1])) {
        --next;
      }
      cursor_ = next;
      return;
    }
    --cursor_;
  }
}
void MenuUi::move_cursor_right(bool all_the_way) {
  if (all_the_way) {
    cursor_ = expression_len_;
  } else if (cursor_ < expression_len_) {
    size_t next = cursor_;
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
      while (next < expression_len_ && menu_detail::is_word_char(expression_[next])) {
        ++next;
      }
      if (next < expression_len_ && expression_[next] == '(') {
        ++next;
      }
      cursor_ = next;
      return;
    }
    ++cursor_;
  }
}
size_t MenuUi::expression_visible_start() const {
  if (expression_len_ <= constants::kVisibleExpressionChars) {
    return 0;
  }

  const size_t half_window = constants::kVisibleExpressionChars / 2;
  if (cursor_ <= half_window) {
    return 0;
  }

  const size_t max_start = expression_len_ - constants::kVisibleExpressionChars;
  return std::min(cursor_ - half_window, max_start);
}
bool MenuUi::key_is_equals(const KeyEvent& key) const {
  const KeyDef& def = key_at(key.row, key.col);
  return std::strcmp(def.label, "=") == 0;
}
void MenuUi::render_standard() {
  canvas_.draw_text(6, 17, "CALCULATE", 1, true);
  canvas_.rect(constants::kInputX,
               constants::kInputY,
               constants::kInputWidth,
               constants::kInputHeight,
               true);

  const size_t visible_start = expression_visible_start();
  const size_t visible_count =
      expression_len_ > visible_start
          ? std::min(constants::kVisibleExpressionChars, expression_len_ - visible_start)
          : 0;
  char visible_expression[constants::kVisibleExpressionChars + 1] {};
  if (visible_count > 0) {
    std::memcpy(visible_expression, expression_ + visible_start, visible_count);
  }
  menu_detail::draw_math_text(canvas_,
                              constants::kInputTextX,
                              constants::kInputTextY,
                              expression_len_ == 0 ? "0" : visible_expression);

  const size_t cursor_relative = cursor_ > visible_start ? cursor_ - visible_start : 0;
  const size_t cursor_cells =
      expression_len_ == 0 ? 1 : std::min(cursor_relative, constants::kVisibleExpressionChars);
  const int cursor_x =
      expression_len_ == 0
          ? constants::kInputTextX + constants::kCharWidth
          : constants::kInputTextX + menu_detail::math_text_width(visible_expression, cursor_cells);
  canvas_.vline(cursor_x, constants::kInputTextY - 3, 13, true);

  if (result_visible_) {
    canvas_.draw_text(6, 66, result_is_error_ ? "ERR" : "=", 1, true);
    const size_t result_len = std::strlen(result_text_);
    for (size_t line = 0; line < 2; ++line) {
      const size_t line_offset = line * constants::kVisibleResultChars;
      if (line_offset >= result_len) {
        break;
      }
      char line_text[constants::kVisibleResultChars + 1] {};
      const size_t line_len = std::min(constants::kVisibleResultChars, result_len - line_offset);
      std::memcpy(line_text, result_text_ + line_offset, line_len);
      menu_detail::draw_math_text(canvas_, 28, 66 + static_cast<int>(line) * 12, line_text);
    }
  }

  canvas_.hline(5, 98, 240, true);
  canvas_.draw_text(6, 108, status_, 1, true);
  canvas_.draw_text(130, 108, "SHIFT+=  ALPHA+=", 1, true);

  if (variable_palette_ != VariablePalette::None) {
    render_variable_palette();
  }
}
void MenuUi::render_constants() {
  canvas_.draw_text(6, 17, "CONSTANTS", 1, true);

  size_t first = 0;
  if (constant_selected_ >= constants::kConstantsVisibleRows / 2) {
    first = constant_selected_ - constants::kConstantsVisibleRows / 2;
  }
  if (first + constants::kConstantsVisibleRows > constants::kScientificConstantCount) {
    first = constants::kScientificConstantCount - constants::kConstantsVisibleRows;
  }

  for (size_t row = 0; row < constants::kConstantsVisibleRows; ++row) {
    const size_t index = first + row;
    const auto& constant = constants::kScientificConstants[index];
    const int y = constants::kConstantsListY +
                  static_cast<int>(row) * constants::kConstantsRowHeight;
    const bool selected = index == constant_selected_;
    if (selected) {
      canvas_.fill_rect(constants::kConstantsListX - 3,
                        y - 3,
                        constants::kConstantsRowWidth,
                        12,
                        true);
    }

    char row_text[40] {};
    std::snprintf(row_text,
                  sizeof(row_text),
                  "%02u %-8s %s",
                  static_cast<unsigned>(index),
                  constant.label,
                  constant.category);
    canvas_.draw_text(constants::kConstantsListX, y, row_text, 1, !selected);
  }

  canvas_.draw_text(6, 108, "ENTER INSERTS  AC BACK", 1, true);
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
                          12,
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
    canvas_.draw_text(6, 108, "ARROWS/INDEX  ENTER", 1, true);
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
    canvas_.draw_text(6, 108, "TYPE TO FILTER  AC BACK", 1, true);
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
                        12,
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
  canvas_.draw_text(6, 108, "= COPY  DIGITS FILTER  AC BACK", 1, true);
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
