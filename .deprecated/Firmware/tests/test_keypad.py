# Quick keypad test — run on device
# $ mpremote run tests/test_keypad.py

from drivers.keypad import Keypad
from time import sleep_ms

kpd = Keypad()
print("Keypad test — press keys (CTRL+C to stop)")

while True:
    key = kpd.scan()
    if key:
        print(f"Key: {key}")
    sleep_ms(50)
