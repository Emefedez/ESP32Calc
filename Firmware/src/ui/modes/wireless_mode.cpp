#include "ui/modes/mode_registry.h"

#include "ui/modes/mode_common.h"

namespace {

constexpr esp32calc::ModeMenuItem kItems[] = {
    {"STATUS", "LINK"},
    {"WIFI SCAN", "NETS"},
    {"BLE", "PAIR"},
    {"UPDATE", "OTA"},
};
constexpr const char* kMessages[] = {
    "WIRELESS STATUS",
    "SCAN NETWORKS",
    "BLE PAIRING",
    "OTA UPDATE",
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
  uint8_t selected_ = 0;
  bool detail_open_ = false;
};

const ModeDescriptor& wireless_mode_descriptor() {
  static constexpr ModeDescriptor kDescriptor = make_mode_descriptor<WirelessMode>("WIRELESS");
  return kDescriptor;
}

const char* WirelessMode::name() const {
  return "WIRELESS";
}

void WirelessMode::on_open() {
  detail_open_ = false;
}

ModeResult WirelessMode::handle_key(const KeyEvent& key, const KeyDef& def) {
  (void)key;

  if (detail_open_) {
    if (def.role == KeyRole::Clear) {
      detail_open_ = false;
      return ModeResult::FullRefresh;
    }
    return ModeResult::None;
  }

  const ModeResult nav = handle_menu_navigation(def, selected_, kItemCount);
  if (nav != ModeResult::None) {
    return nav;
  }

  if (def.role == KeyRole::Enter) {
    detail_open_ = true;
    return ModeResult::FullRefresh;
  }
  return ModeResult::None;
}

void WirelessMode::render(MonoCanvas& canvas) {
  if (detail_open_) {
    render_mode_message(canvas, name(), kMessages[selected_], "CLR RETURNS MODE MENU");
    return;
  }

  render_mode_menu(canvas, name(), kItems, kItemCount, selected_);
}

}  // namespace esp32calc
