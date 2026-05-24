#include "about_screen.h"
#include "display/gui.h"
#include "menu_system.h"
#include <stddef.h>

static void draw(screen_t *self) {
    (void)self;
    gui_clear(GUI_COLOR_WHITE);
    gui_draw_header(self->title, true);

    gui_text(4, 30, "ESP32-Calc  v0.1", GUI_FONT_SMALL, GUI_COLOR_BLACK);
    gui_text(4, 48, "WeAct 2.9 B/W Waveshare", GUI_FONT_SMALL, GUI_COLOR_BLACK);
    gui_text(4, 66, "raylib compositing", GUI_FONT_SMALL, GUI_COLOR_BLACK);
    gui_text(4, 84, "[AC] back", GUI_FONT_SMALL, GUI_COLOR_BLACK);
}

static void key(screen_t *self, key_code_t k) {
    (void)self;
    if (k == KEY_MODE)  menu_pop_to_root();
    else if (k == KEY_AC) menu_pop();
}

static screen_t screen = {
    .title = "About",
    .on_enter = NULL, .on_draw = draw, .on_key = key,
};

screen_t *screen_about_create(void) { return &screen; }
