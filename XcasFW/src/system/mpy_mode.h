#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace esp32calc_alt {

class StorageManager;
class Weact213BwDisplay;

void run_mpy_mode(StorageManager& storage,
                  Weact213BwDisplay& display,
                  QueueHandle_t app_events);

}  // namespace esp32calc_alt
