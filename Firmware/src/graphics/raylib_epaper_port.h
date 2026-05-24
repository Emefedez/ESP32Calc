#pragma once

#include "app_config.h"

#if ESP32CALC_USE_RAYLIB

#include "display/weact_213_bw.h"
#include "esp_err.h"
#include "graphics/mono_canvas.h"

namespace esp32calc {

class RaylibEpaperPort {
 public:
  esp_err_t init(Weact213BwDisplay& display);
  bool ready() const { return initialized_; }
  void set_next_refresh_mode(RefreshMode mode);

  // Capture mode: EndDrawing stores the RGB565 framebuffer but skips
  // EPD flush. The caller reads pixels and blits them manually.
  void capture_begin();
  const uint16_t* capture_end();          // returns RGB565 framebuffer pointer
  static const uint16_t* framebuffer();   // direct read-only access

 private:
  bool initialized_ = false;
};

  // Render raylib content into a sub-region of a MonoCanvas.
  // draw_fn(ctx) is called with coordinates offset so (0,0) maps to (x,y)
  // on screen. Only the pixels in [x,y,w,h] are read from raylib's framebuffer
  // and converted to 1bpp. The EPD is NOT updated — caller must call
  // display.update_canvas() afterward to do a partial refresh on the dirty region.
  void draw_raylib_region(MonoCanvas& canvas, int x, int y, int w, int h,
                          void (*draw_fn)(void*), void* ctx);

}  // namespace esp32calc

#endif  // ESP32CALC_USE_RAYLIB
