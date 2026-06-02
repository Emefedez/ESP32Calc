/*
 * menu_detail.cpp contains some helper functions for drawing math but prettier.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "graphics/mono_canvas.h"

namespace esp32calc_alt::menu_detail {

bool has_text(const char* text);
bool is_word_char(char value);
int math_text_width(const char* text, size_t raw_chars, uint8_t scale = 1);
void draw_math_text(MonoCanvas& canvas, int x, int y, const char* text, uint8_t scale = 1);
void draw_math_cursor(MonoCanvas& canvas,
                      int x,
                      int y,
                      const char* text,
                      size_t cursor,
                      uint8_t scale = 1);

}  // namespace esp32calc_alt::menu_detail
