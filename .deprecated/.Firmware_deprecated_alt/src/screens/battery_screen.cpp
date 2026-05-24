#include "screens/battery_screen.h"
#include "display/gui.h"
#include "hal/battery.h"
#include "screens/menu_system.h"
#include <cstdio>

void battery_screen_enter(screen_t *self) { self->dirty = true; battery_force_refresh(); }

void battery_screen_draw(screen_t *self) {
    (void)self;
    gui_clear(GUI_COLOR_WHITE);
    battery_reading_t r = battery_read();
    char buf[32];
    snprintf(buf, sizeof(buf), "Voltage: %.2fV", r.voltage);
    gui_text(20, 30, buf, GUI_FONT_MEDIUM, GUI_COLOR_BLACK);
    snprintf(buf, sizeof(buf), "Charge: %d%%", r.percent);
    gui_text(20, 60, buf, GUI_FONT_MEDIUM, GUI_COLOR_BLACK);
    gui_text(20, 90, "AC to go back", GUI_FONT_SMALL, GUI_COLOR_BLACK);
}

void battery_screen_handle_key(screen_t *self, key_code_t key) {
    if (key == KEY_AC) menu_pop();
    self->dirty = true;
}
