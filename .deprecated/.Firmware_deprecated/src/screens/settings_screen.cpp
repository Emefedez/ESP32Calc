#include "settings_screen.h"
#include "display/gui.h"
#include "core/settings.h"
#include "menu_system.h"
#include <stdio.h>

#define SETTING_COUNT 1

static int setting_sel = 0;

static void draw(screen_t *self) {
    (void)self;
    gui_clear(GUI_COLOR_WHITE);
    gui_draw_header(self->title, true);

    int y = 30;
    char buf[32];

    // Angle mode
    snprintf(buf, sizeof(buf), "Angle:  %s",
             g_settings.angle_mode == ANGLE_DEG ? "DEG" : "RAD");
    if (setting_sel == 0) {
        gui_rect(4, y - 2, GUI_WIDTH - 8, 14, GUI_COLOR_BLACK, true);
        gui_text(10, y, buf, GUI_FONT_SMALL, GUI_COLOR_WHITE);
    } else {
        gui_text(10, y, buf, GUI_FONT_SMALL, GUI_COLOR_BLACK);
    }
    y += 20;

    gui_text(4, GUI_HEIGHT - 14, "UP/DN sel  CALC toggle  MODE home  AC back",
             GUI_FONT_SMALL, GUI_COLOR_BLACK);
}

static void key(screen_t *self, key_code_t k) {
    (void)self;
    switch (k) {
    case KEY_MODE:
        menu_pop_to_root();
        break;
    case KEY_AC:
        menu_pop();
        break;
    case KEY_UP:
        setting_sel = (setting_sel + SETTING_COUNT - 1) % SETTING_COUNT;
        break;
    case KEY_DOWN:
        setting_sel = (setting_sel + 1) % SETTING_COUNT;
        break;
    case KEY_CALC:
        if (setting_sel == 0) settings_toggle_angle();
        break;
    default:
        break;
    }
}

static screen_t screen = {
    .title = "Settings",
    .on_enter = NULL, .on_draw = draw, .on_key = key,
};

screen_t *screen_settings_create(void) { return &screen; }
