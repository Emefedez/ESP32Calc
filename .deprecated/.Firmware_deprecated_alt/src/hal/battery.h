#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float voltage;
    uint8_t percent;
} battery_reading_t;

void battery_init(void);
battery_reading_t battery_read(void);
battery_reading_t battery_read_cached(void);
bool battery_is_low(void);
void battery_force_refresh(void);
void battery_draw_indicator(int x, int y);

#endif
