#include "ui/menu_detail.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace esp32calc_alt::menu_detail {
namespace {

bool starts_with_at(const char* text, size_t offset, const char* prefix) {
  return text != nullptr && std::strncmp(text + offset, prefix, std::strlen(prefix)) == 0;
}

}  // namespace

bool has_text(const char* text) {
  return text != nullptr && text[0] != '\0';
}

bool is_word_char(char value) {
  return std::isalpha(static_cast<unsigned char>(value)) != 0;
}

int math_text_width(const char* text, size_t raw_chars) {
  int width = 0;
  int sqrt_depth = 0;
  for (size_t i = 0; text != nullptr && text[i] != '\0' && i < raw_chars;) {
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
    if (text[i] == '^' && text[i + 1] != '\0' && i + 1 < raw_chars) {
      width += 6;
      i += 2;
      continue;
    }
    width += 6;
    ++i;
  }
  return width;
}

void draw_math_text(MonoCanvas& canvas, int x, int y, const char* text) {
  int cursor = x;
  int sqrt_depth = 0;
  for (size_t i = 0; text != nullptr && text[i] != '\0';) {
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
      canvas.draw_char(cursor, y - 5, text[i + 1], 1, true);
      cursor += 6;
      i += 2;
      continue;
    }
    canvas.draw_char(cursor, y, text[i], 1, true);
    cursor += 6;
    ++i;
  }
}

}  // namespace esp32calc_alt::menu_detail
