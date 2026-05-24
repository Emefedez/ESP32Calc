#include "core/settings.h"

settings_t g_settings;

void settings_init(void) {
    g_settings.angle_mode = ANGLE_DEG;
}

void settings_toggle_angle(void) {
    g_settings.angle_mode = (g_settings.angle_mode == ANGLE_DEG) ? ANGLE_RAD : ANGLE_DEG;
}
