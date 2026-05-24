#include "screens/menu_system.h"
#include "display/gui.h"
#include "screens/calc_screen.h"
#include <Arduino.h>

#define MAX_DEPTH 8

static screen_t *stack[MAX_DEPTH];
static int depth = 0;
static screen_t calc_screen;

void menu_init(void) {
    depth = 0;
    calc_screen.title = "Calculator";
    calc_screen.on_enter = calc_screen_enter;
    calc_screen.on_draw = calc_screen_draw;
    calc_screen.on_key = calc_screen_handle_key;
    calc_screen.user_data = NULL;
    calc_screen.dirty = true;
    menu_push(&calc_screen);
}

void menu_push(screen_t *screen) {
    if (depth < MAX_DEPTH) { stack[depth++] = screen; if (screen->on_enter) screen->on_enter(screen); }
}

void menu_pop(void) { if (depth > 1) { depth--; stack[depth - 1]->dirty = true; } }

void menu_pop_to_root(void) { while (depth > 1) menu_pop(); }

screen_t *menu_current(void) { return (depth > 0) ? stack[depth - 1] : NULL; }

bool menu_draw(void) {
    screen_t *s = menu_current();
    if (!s || !s->dirty) return false;
    gui_begin();
    gui_draw_header(s->title, depth > 1);
    if (s->on_draw) s->on_draw(s);
    s->dirty = false;
    return true;
}

void menu_handle_key(key_code_t k) {
    screen_t *s = menu_current();
    if (s && s->on_key) s->on_key(s, k);
}
