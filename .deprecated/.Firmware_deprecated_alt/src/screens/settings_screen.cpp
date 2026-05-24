#include "screens/settings_screen.h"
#include "display/gui.h"
#include "core/settings.h"
#include "screens/menu_system.h"

void settings_screen_enter(screen_t *self) { self->dirty = true; }

void settings_screen_draw(screen_t *self) {
    (void)self;
    gui_clear(GUI_COLOR_WHITE);
    const char *mode = (g_settings.angle_mode == ANGLE_DEG) ? "Degrees" : "Radians";
    gui_text(20, 30, "Angle mode:", GUI_FONT_MEDIUM, GUI_COLOR_BLACK);
    gui_text(20, 60, mode, GUI_FONT_LARGE, GUI_COLOR_BLACK);
    gui_text(20, 90, "MODE to toggle, AC back", GUI_FONT_SMALL, GUI_COLOR_BLACK);
}

void settings_screen_handle_key(screen_t *self, key_code_t key) {
    if (key == KEY_MODE) settings_toggle_angle();
    if (key == KEY_AC) menu_pop();
    self->dirty = true;
}
