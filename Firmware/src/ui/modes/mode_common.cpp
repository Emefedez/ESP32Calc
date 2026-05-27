#include "ui/modes/mode_common.h"

#include <cstdio>

namespace esp32calc {

uint8_t move_selection_wrapped(uint8_t selected, int delta, size_t count) {
  if (count == 0) {
    return 0;
  }

  int next = static_cast<int>(selected) + delta;
  const int item_count = static_cast<int>(count);
  if (next < 0) {
    next = item_count - 1;
  } else if (next >= item_count) {
    next = 0;
  }
  return static_cast<uint8_t>(next);
}

MenuAction handle_menu_action(const KeyDef& def, uint8_t& selected, size_t count) {
  const int digit = key_digit(def);
  if (digit >= 0 && static_cast<size_t>(digit) < count) {
    selected = static_cast<uint8_t>(digit);
    return MenuAction::ItemChosen;
  }

  switch (def.role) {
    case KeyRole::Up:
    case KeyRole::Left:
      selected = move_selection_wrapped(selected, -1, count);
      return MenuAction::SelectionChanged;
    case KeyRole::Down:
    case KeyRole::Right:
      selected = move_selection_wrapped(selected, 1, count);
      return MenuAction::SelectionChanged;
    case KeyRole::Enter:
      return MenuAction::ItemChosen;
    case KeyRole::Clear:
      return MenuAction::Exit;
    default:
      return MenuAction::None;
  }
}

ModeResult handle_simple_menu_key(const KeyDef& def, SimpleMenuState& state, size_t count) {
  if (state.detail_open) {
    if (def.role == KeyRole::Clear) {
      state.detail_open = false;
      return ModeResult::FullRefresh;
    }
    return ModeResult::None;
  }

  switch (handle_menu_action(def, state.selected, count)) {
    case MenuAction::SelectionChanged:
      return ModeResult::Redraw;
    case MenuAction::ItemChosen:
      state.detail_open = true;
      return ModeResult::FullRefresh;
    case MenuAction::Exit:
      return ModeResult::ExitToMainMenu;
    case MenuAction::None:
    default:
      return ModeResult::None;
  }
}

void render_mode_title(MonoCanvas& canvas, const char* title) {
  canvas.draw_text(4, 19, title, 2, true);
}

void render_mode_menu(MonoCanvas& canvas,
                      const char* title,
                      const ModeMenuItem* items,
                      size_t count,
                      uint8_t selected) {
  render_mode_title(canvas, title);

  constexpr int kX = 6;
  constexpr int kY = 43;
  constexpr int kRowHeight = 16;
  constexpr int kRowWidth = 238;
  constexpr size_t kMaxVisibleItems = 5;

  const size_t visible_count = count < kMaxVisibleItems ? count : kMaxVisibleItems;
  for (size_t i = 0; i < visible_count; ++i) {
    const int y = kY + static_cast<int>(i) * kRowHeight;
    const bool is_selected = i == selected;

    if (is_selected) {
      canvas.fill_rect(kX - 2, y - 3, kRowWidth, 14, true);
    } else {
      canvas.rect(kX - 2, y - 3, kRowWidth, 14, true);
    }

    char indexed_label[40] {};
    std::snprintf(indexed_label, sizeof(indexed_label), "%u %s",
                  static_cast<unsigned>(i), items[i].label);
    canvas.draw_text(kX + 4, y, indexed_label, 1, !is_selected);
    if (items[i].hint != nullptr) {
      canvas.draw_text(145, y, items[i].hint, 1, !is_selected);
    }
  }
}

void render_simple_menu(MonoCanvas& canvas,
                        const char* title,
                        const ModeMenuItem* items,
                        const char* const* messages,
                        size_t count,
                        const SimpleMenuState& state) {
  if (state.detail_open) {
    render_mode_message(canvas, title, messages[state.selected], "CLR RETURNS MODE MENU");
    return;
  }

  render_mode_menu(canvas, title, items, count, state.selected);
}

void render_mode_message(MonoCanvas& canvas,
                         const char* title,
                         const char* message,
                         const char* footer) {
  render_mode_title(canvas, title);
  canvas.draw_text(5, 49, message, 1, true);
  canvas.hline(5, 64, 240, true);
  canvas.draw_text(5, 76, footer, 1, true);
}

}  // namespace esp32calc
