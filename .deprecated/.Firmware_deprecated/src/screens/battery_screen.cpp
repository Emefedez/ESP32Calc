#include "battery_screen.h"
#include "display/gui.h"
#include "hal/battery.h"
#include "menu_system.h"
#include <stdio.h>

static void draw(screen_t *self) {
    (void)self;
    gui_clear(GUI_COLOR_WHITE);
    gui_draw_header(self->title, true);

    battery_reading_t bat = battery_read();
    char buf[48];
    snprintf(buf, sizeof(buf), "Voltage:  %.2f V", bat.voltage);
    gui_text(4, 30, buf, GUI_FONT_SMALL, GUI_COLOR_BLACK);
    snprintf(buf, sizeof(buf), "Charge:   %d %%", bat.percent);
    gui_text(4, 48, buf, GUI_FONT_SMALL, GUI_COLOR_BLACK);

    gui_text(4, 74, "Sense pin: GPIO3 (ADC1_CH2)", GUI_FONT_SMALL, GUI_COLOR_BLACK);
    gui_text(4, 84, "[AC] back", GUI_FONT_SMALL, GUI_COLOR_BLACK);
}

static void key(screen_t *self, key_code_t k) {
    (void)self;
    if (k == KEY_MODE)  menu_pop_to_root();
    else if (k == KEY_AC) menu_pop();
}

static screen_t screen = {
    .title = "Battery",
    .on_enter = NULL, .on_draw = draw, .on_key = key,
};

screen_t *screen_battery_create(void) { return &screen; }
