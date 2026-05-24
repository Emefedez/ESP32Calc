#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float voltage;     // Measured battery voltage (V)
    uint8_t percent;   // 0–100 charge level estimate
} battery_reading_t;

// Init ADC1_CH2 (GPIO3) for battery voltage sensing
void battery_init(void);

// Read raw ADC, convert to voltage, estimate %
battery_reading_t battery_read(void);

// Cached read — re-reads ADC at most once per BATTERY_CACHE_MS
battery_reading_t battery_read_cached(void);

// True if voltage < 3.3V (threshold for LiPo warning)
bool battery_is_low(void);

// Force next battery_read_cached() to re-read ADC
void battery_force_refresh(void);

// Draw battery indicator at (x,y) — e.g. "85%" — to the current gui framebuffer
void battery_draw_indicator(int x, int y);

#endif
