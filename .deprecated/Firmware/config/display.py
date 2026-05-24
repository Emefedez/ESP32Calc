"""Logical framebuffer configuration."""

try:
    from micropython import const
except ImportError:
    const = lambda value: value

# Logical landscape framebuffer (250 wide x 128 tall)
EPD_WIDTH  = const(250)
EPD_HEIGHT = const(128)

# Framebuffer byte size for MONO_VLSB (column-major, stride=WIDTH)
FB_SIZE = const(EPD_WIDTH * ((EPD_HEIGHT + 7) // 8))  # 250 * 16 = 4000

# MONO_VLSB stride = WIDTH (each 8-row group is WIDTH bytes)
FB_STRIDE = const(EPD_WIDTH)  # 250

# Refresh mode
FULL_REFRESH  = const(0)
PARTIAL_REFRESH = const(1)
