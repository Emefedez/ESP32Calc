#include "ui/menu_detail.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "ui/menu_constants.h"

namespace esp32calc_alt::menu_detail {
namespace {

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

int math_text_width(const char* text, size_t raw_chars) {
  size_t slash = 0;
  if (find_top_level_fraction(text, raw_chars, slash)) {
    char numerator[48] {};
    char denominator[48] {};
    copy_slice(numerator, sizeof(numerator), text, 0, slash);
    copy_slice(denominator, sizeof(denominator), text, slash + 1, raw_chars);
    return std::max(math_text_width(numerator, std::strlen(numerator)),
                    math_text_width(denominator, std::strlen(denominator))) +
           6;
  }

  int width = 0;
  int sqrt_depth = 0;
  for (size_t i = 0; text != nullptr && text[i] != '\0' && i < raw_chars;) {
    const size_t constant_len = marked_constant_length(text, i);
    if (constant_len > 0) {
      width += static_cast<int>(constant_len - 1) * 6;
      i += constant_len;
      continue;
    }
    if (starts_with_at(text, i, "sqrt(")) {
      width += 10;
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
          width += math_text_width(inner, std::strlen(inner));
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
      width += static_cast<int>(grouped ? span - 2 : span) * 6;
      i += 1 + span;
      continue;
    }
    width += 6;
    ++i;
  }
  return width;
}

void draw_math_text(MonoCanvas& canvas, int x, int y, const char* text) {
  const size_t raw_chars = text == nullptr ? 0 : std::strlen(text);
  size_t slash = 0;
  if (find_top_level_fraction(text, raw_chars, slash)) {
    char numerator[48] {};
    char denominator[48] {};
    copy_slice(numerator, sizeof(numerator), text, 0, slash);
    copy_slice(denominator, sizeof(denominator), text, slash + 1, raw_chars);
    const int numerator_width = math_text_width(numerator, std::strlen(numerator));
    const int denominator_width = math_text_width(denominator, std::strlen(denominator));
    const int width = std::max(numerator_width, denominator_width) + 4;
    draw_math_text(canvas, x + (width - numerator_width) / 2, y - 8, numerator);
    canvas.hline(x, y + 3, width, true);
    draw_math_text(canvas, x + (width - denominator_width) / 2, y + 6, denominator);
    return;
  }

  int cursor = x;
  int sqrt_depth = 0;
  for (size_t i = 0; text != nullptr && text[i] != '\0';) {
    const size_t constant_len = marked_constant_length(text, i);
    if (constant_len > 0) {
      for (size_t j = i + 1; j < i + constant_len; ++j) {
        canvas.draw_char(cursor, y, text[j], 1, true);
        cursor += 6;
      }
      i += constant_len;
      continue;
    }
    if (starts_with_at(text, i, "sqrt(")) {
      canvas.line(cursor, y + 5, cursor + 3, y + 9, true);
      canvas.line(cursor + 3, y + 9, cursor + 7, y - 1, true);
      canvas.hline(cursor + 7, y - 1, 7, true);
      cursor += 10;
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
        canvas.draw_char(cursor, y - 5, text[j], 1, true);
        cursor += 6;
      }
      i += 1 + span;
      continue;
    }
    canvas.draw_char(cursor, y, text[i], 1, true);
    cursor += 6;
    ++i;
  }
}

void draw_math_cursor(MonoCanvas& canvas, int x, int y, const char* text, size_t cursor) {
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
    const int numerator_width = math_text_width(numerator, std::strlen(numerator));
    const int denominator_width = math_text_width(denominator, std::strlen(denominator));
    const int width = std::max(numerator_width, denominator_width) + 4;

    size_t raw_begin = 0;
    size_t raw_end = slash;
    const char* slot_text = numerator;
    int slot_x = x + (width - numerator_width) / 2;
    int slot_y = y - 8;
    if (cursor > slash) {
      raw_begin = slash + 1;
      raw_end = raw_chars;
      slot_text = denominator;
      slot_x = x + (width - denominator_width) / 2;
      slot_y = y + 6;
    }
    if (raw_end > raw_begin + 1 && text[raw_begin] == '(' && text[raw_end - 1] == ')') {
      ++raw_begin;
      --raw_end;
    }
    const size_t slot_cursor = cursor <= raw_begin
                                   ? 0
                                   : std::min(cursor - raw_begin, std::strlen(slot_text));
    const int cursor_x = slot_x + math_text_width(slot_text, slot_cursor);
    canvas.vline(cursor_x, slot_y - 3, 13, true);
    return;
  }

  const int cursor_x = x + math_text_width(text, cursor);
  if (cursor_in_power_slot(text, cursor, raw_chars)) {
    canvas.vline(cursor_x, y - 12, 12, true);
  } else {
    canvas.vline(cursor_x, y - 4, 15, true);
  }
}

}  // namespace esp32calc_alt::menu_detail
