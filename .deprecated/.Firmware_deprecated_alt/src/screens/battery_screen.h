#ifndef BATTERY_SCREEN_H
#define BATTERY_SCREEN_H

#include "core/screen.h"

void battery_screen_enter(screen_t *self);
void battery_screen_draw(screen_t *self);
void battery_screen_handle_key(screen_t *self, key_code_t key);

#endif
