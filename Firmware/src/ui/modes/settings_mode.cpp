#include "ui/modes/mode_registry.h"

#include "ui/modes/mode_common.h"

namespace {

constexpr esp32calc::ModeMenuItem kItems[] = {
    {"DISPLAY", "EPD"},
    {"POWER", "BATT"},
    {"KEYPAD", "MAP"},
    {"ABOUT", "INFO"},
};
constexpr const char* kMessages[] = {
    "DISPLAY SETTINGS",
    "POWER SETTINGS",
    "KEYPAD SETTINGS",
    "ESP32CALC v0.1.1",
};
constexpr size_t kItemCount = sizeof(kItems) / sizeof(kItems[0]);

}  // namespace

namespace esp32calc {

class SettingsMode final : public UiMode {
 public:
  const char* name() const override;
  void on_open() override;
  ModeResult handle_key(const KeyEvent& key, const KeyDef& def) override;
  void render(MonoCanvas& canvas) override;

 private:
  SimpleMenuState menu_ {};
};

const ModeDescriptor& settings_mode_descriptor() {
  static constexpr ModeDescriptor kDescriptor = make_mode_descriptor<SettingsMode>("SETTINGS");
  return kDescriptor;
}

const char* SettingsMode::name() const {
  return "SETTINGS";
}

void SettingsMode::on_open() {
  menu_ = {};
}

ModeResult SettingsMode::handle_key(const KeyEvent& key, const KeyDef& def) {
  (void)key;
  return handle_simple_menu_key(def, menu_, kItemCount);
}

void SettingsMode::render(MonoCanvas& canvas) {
  render_simple_menu(canvas, name(), kItems, kMessages, kItemCount, menu_);
}

}  // namespace esp32calc
