#pragma once

#include <cstdint>

#include "app_events.h"
#include "graphics/mono_canvas.h"
#include "hardware/keymap.h"


namespace esp32calc {

enum class ModeResult : uint8_t {
  None,
  Redraw,
  FullRefresh,
  ExitToMainMenu,
};

// used to define accessed mode
class UiMode {
 public:
  virtual ~UiMode() = default;

  virtual const char* name() const = 0;
  virtual void on_open() {}
  virtual ModeResult handle_key(const KeyEvent& key, const KeyDef& def) = 0;
  virtual void render(MonoCanvas& canvas) = 0;
};

}  // namespace esp32calc
