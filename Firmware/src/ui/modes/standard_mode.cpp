#include "ui/modes/mode_registry.h"

#include "ui/modes/mode_common.h"

namespace {

constexpr esp32calc::ModeMenuItem kItems[] = {
    {"CALCULATE", "READY"},
    {"HISTORY", "EMPTY"},
    {"CONSTANTS", "LIST"},
};
constexpr const char* kMessages[] = {
    "EXPRESSION ENTRY READY",
    "NO HISTORY YET",
    "CONSTANTS MENU",
};
constexpr size_t kItemCount = sizeof(kItems) / sizeof(kItems[0]);

}  // namespace

namespace esp32calc {

class StandardMode final : public UiMode {
 public:
  const char* name() const override;
  void on_open() override;
  ModeResult handle_key(const KeyEvent& key, const KeyDef& def) override;
  void render(MonoCanvas& canvas) override;

 private:
  uint8_t selected_ = 0;
  bool detail_open_ = false;
};

const ModeDescriptor& standard_mode_descriptor() {
  static constexpr ModeDescriptor kDescriptor = make_mode_descriptor<StandardMode>("STANDARD");
  return kDescriptor;
}

const char* StandardMode::name() const {
  return "STANDARD";
}

void StandardMode::on_open() {
  detail_open_ = false;
}

ModeResult StandardMode::handle_key(const KeyEvent& key, const KeyDef& def) {
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

void StandardMode::render(MonoCanvas& canvas) {
  if (detail_open_) {
    render_mode_message(canvas, name(), kMessages[selected_], "CLR RETURNS MODE MENU");
    return;
  }

  render_mode_menu(canvas, name(), kItems, kItemCount, selected_);
}

}  // namespace esp32calc
