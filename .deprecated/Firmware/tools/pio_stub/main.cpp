// PlatformIO wrapper target only.
// The real firmware is built by tools/pio_micro_build.py via build.sh.
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32Calc MicroPython firmware is built at build/frozen/firmware.bin");
}

void loop() {}
