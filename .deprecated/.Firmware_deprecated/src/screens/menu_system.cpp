#include "menu_system.h"
#include "calc_screen.h"
#include "settings_screen.h"
#include "battery_screen.h"
#include "about_screen.h"
#include "display/gui.h"
#include "hal/battery.h"
#include <stdio.h>
#include <string.h>

// ── Stack ───────────────────────────────────────────────────────────
#define MENU_STACK_MAX 8

static screen_t *stack[MENU_STACK_MAX];
static int        stack_depth = 0;

void menu_push(screen_t *s) {
    if (stack_depth >= MENU_STACK_MAX) return;
    stack[stack_depth++] = s;
    if (s->on_enter) s->on_enter(s);
    s->dirty = true;
    battery_force_refresh();
}

void menu_pop(void) {
    if (stack_depth <= 1) return;
    stack_depth--;
    screen_t *cur = menu_current();
    if (cur && cur->on_enter) cur->on_enter(cur);
    if (cur) cur->dirty = true;
}

void menu_pop_to_root(void) {
    if (stack_depth <= 1) return;
    stack_depth = 1;
    screen_t *cur = menu_current();
    if (cur && cur->on_enter) cur->on_enter(cur);
    if (cur) cur->dirty = true;
}

screen_t *menu_current(void) {
    return stack_depth > 0 ? stack[stack_depth - 1] : NULL;
}

bool menu_draw(void) {
    screen_t *cur = menu_current();
    if (cur && cur->on_draw && cur->dirty) {
        cur->on_draw(cur);
        cur->dirty = false;
        return true;
    }
    return false;
}

void menu_handle_key(key_code_t k) {
    screen_t *cur = menu_current();
    if (cur && cur->on_key) {
        cur->on_key(cur, k);
        cur->dirty = true;
    }
}

// ── Mode Menu ───────────────────────────────────────────────────────
// Each entry: name + screen to push when selected (NULL = placeholder)

typedef struct {
    const char *name;
    screen_t   *target;
} mode_entry_t;

static mode_entry_t modes[] = {
    { "Calculator", screen_calc_create()     },
    { "Converter",  NULL                     },  // TODO
    { "Settings",   screen_settings_create() },
    { "Battery",    screen_battery_create()  },
    { "About",      screen_about_create()    },
};
#define MODE_COUNT  (sizeof(modes) / sizeof(modes[0]))

static int mode_sel = 0;

static void menu_enter(screen_t *self) {
    (void)self;
    mode_sel = 0;
}

static void menu_draw_fn(screen_t *self) {
    (void)self;
    gui_clear(GUI_COLOR_WHITE);

    // header bar (24px tall — taller than sub-screen headers)
    gui_rect(0, 0, GUI_WIDTH, 24, GUI_COLOR_BLACK, true);
    gui_text(6, 4, self->title, GUI_FONT_SMALL, GUI_COLOR_WHITE);
    battery_draw_indicator(GUI_WIDTH - 28, 4);

    // mode list
    int y = 34;
    for (int i = 0; i < (int)MODE_COUNT; i++) {
        if (i == mode_sel) {
            gui_rect(4, y - 2, GUI_WIDTH - 8, 16, GUI_COLOR_BLACK, true);
            char line[32];
            snprintf(line, sizeof(line), "> %s", modes[i].name);
            gui_text(10, y, line, GUI_FONT_SMALL, GUI_COLOR_WHITE);
        } else {
            gui_text(16, y, modes[i].name, GUI_FONT_SMALL, GUI_COLOR_BLACK);
        }
        y += 18;
    }

    gui_text(4, GUI_HEIGHT - 14, "UP/DN select  = enter  MODE home",
             GUI_FONT_SMALL, GUI_COLOR_BLACK);
}

static void menu_key_fn(screen_t *self, key_code_t k) {
    (void)self;
    switch (k) {
    case KEY_UP:
        mode_sel = (mode_sel + MODE_COUNT - 1) % MODE_COUNT;
        break;
    case KEY_DOWN:
        mode_sel = (mode_sel + 1) % MODE_COUNT;
        break;
    case KEY_EQUALS:
        if (modes[mode_sel].target) menu_push(modes[mode_sel].target);
        break;
    default:
        break;
    }
}

static screen_t mode_menu_screen = {
    .title = "ESP32-Calc",
    .on_enter = menu_enter, .on_draw = menu_draw_fn, .on_key = menu_key_fn,
};

// ── Public API ──────────────────────────────────────────────────────

void menu_init(void) {
    stack_depth = 0;
    menu_push(&mode_menu_screen);
}
