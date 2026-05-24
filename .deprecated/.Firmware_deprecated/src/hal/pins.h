#ifndef PINS_H
#define PINS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ===== SENSE / Battery ADC ===== */
// GPIO3 = wokwi-header "sense:1", usable as ADC1_CH2 on ESP32-S3
#define PIN_SENSE   3

/* ===== E-Paper Display (SSD1680) ===== */
#define PIN_EPD_CS     6
#define PIN_EPD_DC     7
#define PIN_EPD_RST    4
#define PIN_EPD_BUSY   5
#define PIN_EPD_SCLK   16
#define PIN_EPD_MOSI   15
#define PIN_EPD_PWR    17

/* ===== Keypad Matrix (9 rows × 6 cols) ===== */
#define KEY_ROWS 9
#define KEY_COLS 6

#define PIN_KEY_ROW0 38
#define PIN_KEY_ROW1 37
#define PIN_KEY_ROW2 36
#define PIN_KEY_ROW3 35
#define PIN_KEY_ROW4 0
#define PIN_KEY_ROW5 9
#define PIN_KEY_ROW6 10
#define PIN_KEY_ROW7 11
#define PIN_KEY_ROW8 12

#define PIN_KEY_COL0 45
#define PIN_KEY_COL1 8
#define PIN_KEY_COL2 47
#define PIN_KEY_COL3 21
#define PIN_KEY_COL4 14
#define PIN_KEY_COL5 13

/* ===== SD Card (SPI) ===== */
#define PIN_SD_CS   42
#define PIN_SD_MOSI 41
#define PIN_SD_SCLK 40
#define PIN_SD_MISO 39

/* ===== Serial / Debug ===== */
#define PIN_SERIAL_TX 43
#define PIN_SERIAL_RX 44

#ifdef __cplusplus
}
#endif

#endif
