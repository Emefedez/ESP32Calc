#ifndef CALC_SCREEN_H
#define CALC_SCREEN_H

#include "core/screen.h"

void calc_screen_enter(screen_t *self);
void calc_screen_draw(screen_t *self);
void calc_screen_handle_key(screen_t *self, key_code_t key);

#endif
