#pragma once

#include <cstddef>
#include <cstdint>

#include "graphics/mono_canvas.h"
#include "hardware/keymap.h"
#include "ui/modes/ui_mode.h"

namespace esp32calc {

struct ModeMenuItem {
  const char* label;
  const char* hint;
};

uint8_t move_selection_wrapped(uint8_t selected, int delta, size_t count);
ModeResult handle_menu_navigation(const KeyDef& def, uint8_t& selected, size_t count);

void render_mode_title(MonoCanvas& canvas, const char* title);
void render_mode_menu(MonoCanvas& canvas,
                      const char* title,
                      const ModeMenuItem* items,
                      size_t count,
                      uint8_t selected);
void render_mode_message(MonoCanvas& canvas,
                         const char* title,
                         const char* message,
                         const char* footer);

}  // namespace esp32calc
