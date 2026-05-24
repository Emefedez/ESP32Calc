#pragma once

#include <array>

#include "driver/gpio.h"
#include "driver/spi_common.h"

namespace esp32calc::pins {

// WeAct 2.13 inch B/W e-paper.
constexpr gpio_num_t kDisplayRst = GPIO_NUM_4;
constexpr gpio_num_t kDisplayBusy = GPIO_NUM_5;
constexpr gpio_num_t kDisplayCs = GPIO_NUM_6;
constexpr gpio_num_t kDisplayDc = GPIO_NUM_7;
constexpr gpio_num_t kDisplayMosi = GPIO_NUM_15;
constexpr gpio_num_t kDisplaySclk = GPIO_NUM_16;
constexpr gpio_num_t kDisplayPower = GPIO_NUM_17;
constexpr spi_host_device_t kDisplaySpiHost = SPI3_HOST;

// Battery sense.
constexpr gpio_num_t kBatterySense = GPIO_NUM_3;

// FTDI flashing hardware notes. UART0 TX/RX use the ESP32-S3 default pins.
// GPIO8 is listed as FTDI RTS in the hardware note and as matrix column 1
// after boot, so avoid holding that key column during reset/flashing.
constexpr gpio_num_t kFtdiRtsShared = GPIO_NUM_8;
constexpr gpio_num_t kFtdiVout = GPIO_NUM_18;

// SDSPI card.
constexpr gpio_num_t kSdCs = GPIO_NUM_42;
constexpr gpio_num_t kSdMosi = GPIO_NUM_41;
constexpr gpio_num_t kSdSclk = GPIO_NUM_40;
constexpr gpio_num_t kSdMiso = GPIO_NUM_39;
constexpr spi_host_device_t kSdSpiHost = SPI2_HOST;

// Keyboard matrix from Firmware/.ignoredList.md.
constexpr std::array<gpio_num_t, 9> kMatrixRows = {
    GPIO_NUM_38, GPIO_NUM_37, GPIO_NUM_36, GPIO_NUM_35, GPIO_NUM_0,
    GPIO_NUM_9,  GPIO_NUM_10, GPIO_NUM_11, GPIO_NUM_12,
};

constexpr std::array<gpio_num_t, 6> kMatrixCols = {
    GPIO_NUM_45, GPIO_NUM_8, GPIO_NUM_47, GPIO_NUM_21, GPIO_NUM_14, GPIO_NUM_13,
};

}  // namespace esp32calc::pins
