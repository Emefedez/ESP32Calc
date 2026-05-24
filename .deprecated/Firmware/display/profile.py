"""Screen/panel physical profile.

Plain Python (no micropython.const) — safe to import on host or device.
"""

PANEL = "WeAct Studio 2.13 B/W"
CONTROLLER = "SSD1680"

# Native SSD1680 RAM dimensions
NATIVE_WIDTH = 128
NATIVE_HEIGHT = 250
VISIBLE_WIDTH = 122

# Logical landscape framebuf dimensions
LOGICAL_WIDTH = 250
LOGICAL_HEIGHT = 128

# RAM window: 128 source outputs = 16 bytes (cols 0-127)
# Bytes 0-15 inclusive. Some pixels fall outside 122-wide glass
# but WeAct reference code uses this range.
X_START_BYTE = 0
X_END_BYTE = 15
Y_START = NATIVE_HEIGHT - 1  # 249
Y_END = 0
