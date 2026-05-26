#include "ui/modes/mode_registry.h"

#include "ui/modes/mode_common.h"

namespace {

constexpr esp32calc::ModeMenuItem kItems[] = {
    {"SOLVE", "EQ"},
    {"EXPAND", "ALG"},
    {"DERIVE", "DX"},
    {"INTEGRATE", "INT"},
};
constexpr const char* kMessages[] = {
    "SOLVER WORKSPACE",
    "EXPAND EXPRESSION",
    "DERIVATIVE SETUP",
    "INTEGRAL SETUP",
};
constexpr size_t kItemCount = sizeof(kItems) / sizeof(kItems[0]);

}  // namespace

namespace esp32calc {

class SymbolicMode final : public UiMode {
 public:
  const char* name() const override;
  void on_open() override;
  ModeResult handle_key(const KeyEvent& key, const KeyDef& def) override;
  void render(MonoCanvas& canvas) override;

 private:
  uint8_t selected_ = 0;
  bool detail_open_ = false;
};

const ModeDescriptor& symbolic_mode_descriptor() {
  static constexpr ModeDescriptor kDescriptor = make_mode_descriptor<SymbolicMode>("SYMBOLIC");
  return kDescriptor;
}

const char* SymbolicMode::name() const {
  return "SYMBOLIC";
}

void SymbolicMode::on_open() {
  detail_open_ = false;
}

ModeResult SymbolicMode::handle_key(const KeyEvent& key, const KeyDef& def) {
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

void SymbolicMode::render(MonoCanvas& canvas) {
  if (detail_open_) {
    render_mode_message(canvas, name(), kMessages[selected_], "CLR RETURNS MODE MENU");
    return;
  }

  render_mode_menu(canvas, name(), kItems, kItemCount, selected_);
}

}  // namespace esp32calc
