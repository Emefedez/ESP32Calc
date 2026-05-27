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

enum class MenuAction : uint8_t {
  None,
  SelectionChanged,
  ItemChosen,
  Exit,
};

struct SimpleMenuState {
  uint8_t selected = 0;
  bool detail_open = false;
};

uint8_t move_selection_wrapped(uint8_t selected, int delta, size_t count);
MenuAction handle_menu_action(const KeyDef& def, uint8_t& selected, size_t count);
ModeResult handle_simple_menu_key(const KeyDef& def, SimpleMenuState& state, size_t count);

void render_mode_title(MonoCanvas& canvas, const char* title);
void render_mode_menu(MonoCanvas& canvas,
                      const char* title,
                      const ModeMenuItem* items,
                      size_t count,
                      uint8_t selected);
void render_simple_menu(MonoCanvas& canvas,
                        const char* title,
                        const ModeMenuItem* items,
                        const char* const* messages,
                        size_t count,
                        const SimpleMenuState& state);
void render_mode_message(MonoCanvas& canvas,
                         const char* title,
                         const char* message,
                         const char* footer);

}  // namespace esp32calc
