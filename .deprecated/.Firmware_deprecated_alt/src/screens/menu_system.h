#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include "core/screen.h"

void menu_init(void);
void menu_push(screen_t *screen);
void menu_pop(void);
void menu_pop_to_root(void);
screen_t *menu_current(void);
bool menu_draw(void);
void menu_handle_key(key_code_t k);

#endif
