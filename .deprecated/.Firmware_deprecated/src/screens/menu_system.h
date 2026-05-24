#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include "core/screen.h"

void menu_init(void);

void menu_push(screen_t *screen);

// Pop top screen. Home screen (depth 1) is never popped.
void menu_pop(void);
// Pop all screens until only the root (depth 1) remains
void menu_pop_to_root(void);

screen_t *menu_current(void);
bool menu_draw(void);
void menu_handle_key(key_code_t k);

#endif
