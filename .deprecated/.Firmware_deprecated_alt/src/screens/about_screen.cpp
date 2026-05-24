#include "screens/about_screen.h"
#include "display/gui.h"
#include "screens/menu_system.h"
#include <cstdio>

void about_screen_enter(screen_t *self) { self->dirty = true; }

void about_screen_draw(screen_t *self) {
    (void)self;
    gui_clear(GUI_COLOR_WHITE);
    gui_text(20, 30, "ESP32 Calc", GUI_FONT_LARGE, GUI_COLOR_BLACK);
    gui_text(20, 60, "v0.1", GUI_FONT_MEDIUM, GUI_COLOR_BLACK);
    gui_text(20, 90, "AC to go back", GUI_FONT_SMALL, GUI_COLOR_BLACK);
}

void about_screen_handle_key(screen_t *self, key_code_t key) {
    if (key == KEY_AC) menu_pop();
    self->dirty = true;
}
