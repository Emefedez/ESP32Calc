#ifndef SETTINGS_SCREEN_H
#define SETTINGS_SCREEN_H

#include "core/screen.h"

void settings_screen_enter(screen_t *self);
void settings_screen_draw(screen_t *self);
void settings_screen_handle_key(screen_t *self, key_code_t key);

#endif
