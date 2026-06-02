#include "ui/modes/mode_registry.h"

#include "ui/modes/mode_common.h"

namespace {

constexpr esp32calc::ModeMenuItem kItems[] = {
    {"STATUS", "LINK"},
    {"WIFI SCAN", "NETS"},
    {"BLE", "PAIR"},
};
constexpr const char* kMessages[] = {
    "WIRELESS STATUS",
    "SCAN NETWORKS",
    "BLE PAIRING",
};
constexpr size_t kItemCount = sizeof(kItems) / sizeof(kItems[0]);

}  // namespace

namespace esp32calc {

class WirelessMode final : public UiMode {
 public:
  const char* name() const override;
  void on_open() override;
  ModeResult handle_key(const KeyEvent& key, const KeyDef& def) override;
  void render(MonoCanvas& canvas) override;

 private:
  SimpleMenuState menu_ {};
};

const ModeDescriptor& wireless_mode_descriptor() {
  static constexpr ModeDescriptor kDescriptor = make_mode_descriptor<WirelessMode>("WIRELESS");
  return kDescriptor;
}

const char* WirelessMode::name() const {
  return "WIRELESS";
}

void WirelessMode::on_open() {
  menu_ = {};
}

ModeResult WirelessMode::handle_key(const KeyEvent& key, const KeyDef& def) {
  (void)key;
  return handle_simple_menu_key(def, menu_, kItemCount);
}

void WirelessMode::render(MonoCanvas& canvas) {
  render_simple_menu(canvas, name(), kItems, kMessages, kItemCount, menu_);
}

}  // namespace esp32calc
