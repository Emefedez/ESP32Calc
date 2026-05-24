#ifndef SCREEN_H
#define SCREEN_H

#include "core/keypad.h"

typedef struct screen screen_t;

struct screen {
    const char *title;
    void (*on_enter)(screen_t *self);
    void (*on_draw)(screen_t *self);
    void (*on_key)(screen_t *self, key_code_t key);
    void *user_data;
    bool dirty;
};

#endif
