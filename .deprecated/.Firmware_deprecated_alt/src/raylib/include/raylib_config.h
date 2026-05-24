#ifndef RAYLIB_CONFIG_H
#define RAYLIB_CONFIG_H

// Platform: Custom (ESP32-S3 with e-paper)
#define PLATFORM_CUSTOM

// Graphics API: OpenGL 1.1 Software Renderer
#define GRAPHICS_API_OPENGL_11
#define GRAPHICS_API_OPENGL_11_SOFTWARE

// Software renderer configuration
#define SW_COLOR_BUFFER_BITS 16  // RGB565
#define SW_MALLOC malloc
#define SW_FREE free

// Disable unused modules
#define SUPPORT_MODULE_RAUDIO     0
#define SUPPORT_MODULE_RMODELS    0
#define SUPPORT_MODULE_RRAYGUI    0

// Framebuffer dimensions (matches e-paper)
#define RAYLIB_DISPLAY_WIDTH  296
#define RAYLIB_DISPLAY_HEIGHT 128

// Font support
#define SUPPORT_FILEFORMAT_TTF  0
#define SUPPORT_FILEFORMAT_FNT  1
#define SUPPORT_MODULE_RTEXT    1

// Textures
#define SUPPORT_MODULE_RTEXTURES 1

// Disable mouse/touch (e-paper has no touch)
#define SUPPORT_MOUSE_GESTURES  0
#define SUPPORT_TOUCH_SYSTEM    0

// Disable gamepad
#define SUPPORT_GAMEPADS        0

// Disable clipboard
#define SUPPORT_CLIPBOARD       0

// Disable busy wait
#define SUPPORT_BUSY_WAIT_LOOP  0

// Logging
#define SUPPORT_TRACELOG        1
#define SUPPORT_TRACELOG_DEBUG  0

#endif
