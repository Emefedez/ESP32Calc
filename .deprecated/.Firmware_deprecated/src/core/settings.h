#ifndef SETTINGS_H
#define SETTINGS_H

#include "core/calc_math.h"

typedef struct {
    angle_mode_t  angle_mode;
} settings_t;

extern settings_t g_settings;

void settings_init(void);
void settings_toggle_angle(void);

#endif
