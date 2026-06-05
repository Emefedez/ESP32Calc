#include "ui/menu_detail.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "math/types.h"
#include "ui/menu_constants.h"

namespace esp32calc_alt::menu_detail {
namespace {

bool is_solve_variable(char value) {
  for (size_t i = 0; i < kSolveVariableCount; ++i) {
    if (kSolveVariables[i] == value) {
      return true;
    }
  }
  return false;
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

bool starts_with_at(const char* text, size_t offset, const char* prefix) {
  return text != nullptr && std::strncmp(text + offset, prefix, std::strlen(prefix)) == 0;
}

size_t marked_constant_length(const char* text, size_t offset) {
  if (text == nullptr || text[offset] != menu_constants::kConstantMarker) {
    return 0;
  }
  size_t end = offset + 1;
  while (std::isalnum(static_cast<unsigned char>(text[end])) != 0) {
    ++end;
  }
  return menu_constants::find_scientific_constant(text + offset + 1, end - offset - 1) == nullptr
             ? 0
             : end - offset;
}

size_t grouped_span_length(const char* text, size_t offset, size_t raw_chars) {
  if (text == nullptr || offset >= raw_chars) {
    return 0;
  }
  const char open = text[offset];
  const char close = open == '(' ? ')' : (open == '[' ? ']' : '\0');
  if (close == '\0') {
    return 0;
  }
  int depth = 0;
  for (size_t i = offset; text[i] != '\0' && i < raw_chars; ++i) {
    if (text[i] == open) {
      ++depth;
    } else if (text[i] == close) {
      --depth;
      if (depth == 0) {
        return i - offset + 1;
      }
    }
  }
  return raw_chars - offset;
}

size_t power_span_length(const char* text, size_t offset, size_t raw_chars) {
  if (text == nullptr || offset >= raw_chars) {
    return 0;
  }
  if (text[offset] == '(' || text[offset] == '[') {
    return grouped_span_length(text, offset, raw_chars);
  }

  const size_t constant_len = marked_constant_length(text, offset);
  if (constant_len > 0) {
    return constant_len;
  }

  size_t end = offset;
  while (text[end] != '\0' && end < raw_chars &&
         (std::isalnum(static_cast<unsigned char>(text[end])) != 0 || text[end] == '.')) {
    ++end;
  }
  return end > offset ? end - offset : 1;
}

bool cursor_in_power_slot(const char* text, size_t cursor, size_t raw_chars) {
  if (text == nullptr) {
    return false;
  }

  for (size_t i = 0; text[i] != '\0' && i < raw_chars; ++i) {
    if (text[i] != '^' || i + 1 >= raw_chars) {
      continue;
    }

    const size_t span = power_span_length(text, i + 1, raw_chars);
    if (span == 0) {
      continue;
    }

    size_t begin = i + 1;
    size_t end = begin + span;
    if ((text[begin] == '(' || text[begin] == '[') && end > begin + 1 &&
        (text[end - 1] == ')' || text[end - 1] == ']')) {
      ++begin;
      --end;
    }
    if (cursor >= begin && cursor <= end) {
      return true;
    }
  }
  return false;
}

bool find_top_level_fraction(const char* text, size_t raw_chars, size_t& slash) {
  int paren_depth = 0;
  int bracket_depth = 0;
  for (size_t i = 0; text != nullptr && text[i] != '\0' && i < raw_chars; ++i) {
    switch (text[i]) {
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
      case '/':
        if (paren_depth == 0 && bracket_depth == 0 && i > 0 && i + 1 < raw_chars) {
          slash = i;
          return true;
        }
        break;
      default:
        break;
    }
  }
  return false;
}

void copy_slice(char* output, size_t output_size, const char* text, size_t begin, size_t end) {
  if (output == nullptr || output_size == 0 || text == nullptr || end < begin) {
    return;
  }
  if (end > begin + 1 && text[begin] == '(' && text[end - 1] == ')' &&
      grouped_span_length(text, begin, end) == end - begin) {
    ++begin;
    --end;
  }
  const size_t len = std::min(end - begin, output_size - 1);
  std::memcpy(output, text + begin, len);
  output[len] = '\0';
}

}  // namespace

bool has_text(const char* text) {
  return text != nullptr && text[0] != '\0';
}

bool is_word_char(char value) {
  return std::isalpha(static_cast<unsigned char>(value)) != 0;
}

bool is_known_function_name(const char* begin, size_t length) {
  static constexpr const char* kNames[] = {
      "abs", "acos", "asin", "atan", "cos", "deriv", "derive", "derivative",
      "det", "diff", "evalf", "exp", "factor", "fsolve", "graph", "int", "integrate",
      "inv", "inverse", "linsolve", "ln", "log", "matrix", "poly1", "sin", "solve", "sqrt", "sys", "system", "systems", "tan", "transpose",
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

bool expand_for_math(const char* input, char* output, size_t output_size) {
  // Shared pre-CAS normalizer for Standard/Graph. Keeps UI entry flexible while
  // sending Giac explicit multiplication, expanded constants, no empty powers.
  char without_empty_slots[menu_constants::kExpandedExpressionCapacity] {};
  char constants_expanded[menu_constants::kExpandedExpressionCapacity] {};
  return remove_empty_power_slots(input,
                                  without_empty_slots,
                                  sizeof(without_empty_slots)) &&
         menu_constants::expand_scientific_constants(without_empty_slots,
                                                     constants_expanded,
                                                     sizeof(constants_expanded)) &&
         insert_implicit_multiplication(constants_expanded, output, output_size);
}

int math_text_width(const char* text, size_t raw_chars, uint8_t scale) {
  const int char_width = 6 * static_cast<int>(std::max<uint8_t>(scale, 1));
  size_t slash = 0;
  if (find_top_level_fraction(text, raw_chars, slash)) {
    char numerator[48] {};
    char denominator[48] {};
    copy_slice(numerator, sizeof(numerator), text, 0, slash);
    copy_slice(denominator, sizeof(denominator), text, slash + 1, raw_chars);
    return std::max(math_text_width(numerator, std::strlen(numerator), scale),
                    math_text_width(denominator, std::strlen(denominator), scale)) +
           6 * static_cast<int>(scale);
  }

  int width = 0;
  int sqrt_depth = 0;
  for (size_t i = 0; text != nullptr && text[i] != '\0' && i < raw_chars;) {
    const size_t constant_len = marked_constant_length(text, i);
    if (constant_len > 0) {
      width += static_cast<int>(constant_len - 1) * char_width;
      i += constant_len;
      continue;
    }
    if (starts_with_at(text, i, "sqrt(")) {
      width += 10 * static_cast<int>(scale);
      i += 5;
      ++sqrt_depth;
      continue;
    }
    if (text[i] == ')' && sqrt_depth > 0) {
      --sqrt_depth;
      ++i;
      continue;
    }
    if (text[i] == '^') {
      if (text[i + 1] == '\0' || i + 1 >= raw_chars) {
        break;
      }
      if (text[i + 1] == '(' || text[i + 1] == '[') {
        const size_t group_begin = i + 1;
        const size_t span = grouped_span_length(text, group_begin, std::strlen(text));
        const size_t group_end = group_begin + span;
        const size_t inner_begin = group_begin + 1;
        size_t inner_end = raw_chars < group_end ? raw_chars : group_end;
        if (inner_end > inner_begin && inner_end == group_end &&
            (text[inner_end - 1] == ')' || text[inner_end - 1] == ']')) {
          --inner_end;
        }
        if (inner_end > inner_begin) {
          char inner[48] {};
          copy_slice(inner, sizeof(inner), text, inner_begin, inner_end);
          width += math_text_width(inner, std::strlen(inner), scale);
        }
        if (raw_chars <= group_end) {
          break;
        }
        i = group_end;
        continue;
      }
      const size_t span = power_span_length(text, i + 1, raw_chars);
      const bool grouped =
          (text[i + 1] == '(' || text[i + 1] == '[') && span > 2;
      width += static_cast<int>(grouped ? span - 2 : span) * char_width;
      i += 1 + span;
      continue;
    }
    width += char_width;
    ++i;
  }
  return width;
}

void draw_math_text(MonoCanvas& canvas, int x, int y, const char* text, uint8_t scale) {
  scale = std::max<uint8_t>(scale, 1);
  const int char_width = 6 * static_cast<int>(scale);
  const size_t raw_chars = text == nullptr ? 0 : std::strlen(text);
  size_t slash = 0;
  if (find_top_level_fraction(text, raw_chars, slash)) {
    char numerator[48] {};
    char denominator[48] {};
    copy_slice(numerator, sizeof(numerator), text, 0, slash);
    copy_slice(denominator, sizeof(denominator), text, slash + 1, raw_chars);
    const int numerator_width = math_text_width(numerator, std::strlen(numerator), scale);
    const int denominator_width = math_text_width(denominator, std::strlen(denominator), scale);
    const int width = std::max(numerator_width, denominator_width) + 6 * static_cast<int>(scale);
    draw_math_text(canvas, x + (width - numerator_width) / 2, y - 10 * scale, numerator, scale);
    canvas.hline(x, y + 2 * scale, width, true);
    draw_math_text(canvas, x + (width - denominator_width) / 2, y + 8 * scale, denominator, scale);
    return;
  }

  int cursor = x;
  int sqrt_depth = 0;
  for (size_t i = 0; text != nullptr && text[i] != '\0';) {
    const size_t constant_len = marked_constant_length(text, i);
    if (constant_len > 0) {
      for (size_t j = i + 1; j < i + constant_len; ++j) {
        canvas.draw_char(cursor, y, text[j], scale, true);
        cursor += char_width;
      }
      i += constant_len;
      continue;
    }
    if (starts_with_at(text, i, "sqrt(")) {
      canvas.line(cursor, y + 5 * scale, cursor + 3 * scale, y + 9 * scale, true);
      canvas.line(cursor + 3 * scale, y + 9 * scale, cursor + 7 * scale, y - scale, true);
      canvas.hline(cursor + 7 * scale, y - scale, 7 * scale, true);
      cursor += 10 * scale;
      i += 5;
      ++sqrt_depth;
      continue;
    }
    if (text[i] == ')' && sqrt_depth > 0) {
      --sqrt_depth;
      ++i;
      continue;
    }
    if (text[i] == '^' && text[i + 1] != '\0') {
      const size_t span = power_span_length(text, i + 1, std::strlen(text));
      size_t begin = i + 1;
      size_t end = begin + span;
      if (text[begin] == '(' && end > begin + 1 && text[end - 1] == ')') {
        ++begin;
        --end;
      }
      for (size_t j = begin; j < end; ++j) {
        canvas.draw_char(cursor, y - 5 * scale, text[j], scale, true);
        cursor += char_width;
      }
      i += 1 + span;
      continue;
    }
    canvas.draw_char(cursor, y, text[i], scale, true);
    cursor += char_width;
    ++i;
  }
}

void draw_math_cursor(MonoCanvas& canvas,
                      int x,
                      int y,
                      const char* text,
                      size_t cursor,
                      uint8_t scale) {
  scale = std::max<uint8_t>(scale, 1);
  const size_t raw_chars = text == nullptr ? 0 : std::strlen(text);
  if (cursor > raw_chars) {
    cursor = raw_chars;
  }

  size_t slash = 0;
  if (find_top_level_fraction(text, raw_chars, slash)) {
    char numerator[48] {};
    char denominator[48] {};
    copy_slice(numerator, sizeof(numerator), text, 0, slash);
    copy_slice(denominator, sizeof(denominator), text, slash + 1, raw_chars);
    const int numerator_width = math_text_width(numerator, std::strlen(numerator), scale);
    const int denominator_width = math_text_width(denominator, std::strlen(denominator), scale);
    const int width = std::max(numerator_width, denominator_width) + 6 * static_cast<int>(scale);

    if (cursor == 0) {
      canvas.vline(x - 2 * scale, y - 4 * scale, 15 * scale, true);
      return;
    }
    if (cursor >= raw_chars) {
      canvas.vline(x + width + 2 * scale, y - 4 * scale, 15 * scale, true);
      return;
    }

    size_t raw_begin = 0;
    size_t raw_end = slash;
    const char* slot_text = numerator;
    int slot_x = x + (width - numerator_width) / 2;
    int slot_y = y - 10 * scale;
    if (cursor > slash) {
      raw_begin = slash + 1;
      raw_end = raw_chars;
      slot_text = denominator;
      slot_x = x + (width - denominator_width) / 2;
      slot_y = y + 8 * scale;
    }
    if (raw_end > raw_begin + 1 && text[raw_begin] == '(' && text[raw_end - 1] == ')') {
      ++raw_begin;
      --raw_end;
    }
    const size_t slot_cursor = cursor <= raw_begin
                                   ? 0
                                   : std::min(cursor - raw_begin, std::strlen(slot_text));
    const int cursor_x = slot_x + math_text_width(slot_text, slot_cursor, scale);
    canvas.vline(cursor_x, slot_y - 3 * scale, 13 * scale, true);
    return;
  }

  const int cursor_x = x + math_text_width(text, cursor, scale);
  if (cursor_in_power_slot(text, cursor, raw_chars)) {
    canvas.vline(cursor_x, y - 12 * scale, 12 * scale, true);
  } else {
    canvas.vline(cursor_x, y - 4 * scale, 15 * scale, true);
  }
}

}  // namespace esp32calc_alt::menu_detail
