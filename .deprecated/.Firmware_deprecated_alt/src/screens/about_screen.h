#ifndef ABOUT_SCREEN_H
#define ABOUT_SCREEN_H

#include "core/screen.h"

void about_screen_enter(screen_t *self);
void about_screen_draw(screen_t *self);
void about_screen_handle_key(screen_t *self, key_code_t key);

#endif
