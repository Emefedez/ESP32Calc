/*
 * menu_detail.cpp contains some helper functions for drawing math but prettier.
 */

#pragma once

#include <cstddef>

#include "graphics/mono_canvas.h"

namespace esp32calc_alt::menu_detail {

bool has_text(const char* text);
bool is_word_char(char value);
int math_text_width(const char* text, size_t raw_chars);
void draw_math_text(MonoCanvas& canvas, int x, int y, const char* text);

}  // namespace esp32calc_alt::menu_detail
