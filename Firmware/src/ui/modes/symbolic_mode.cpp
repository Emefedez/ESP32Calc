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
  SimpleMenuState menu_ {};
};

const ModeDescriptor& symbolic_mode_descriptor() {
  static constexpr ModeDescriptor kDescriptor = make_mode_descriptor<SymbolicMode>("SYMBOLIC");
  return kDescriptor;
}

const char* SymbolicMode::name() const {
  return "SYMBOLIC";
}

void SymbolicMode::on_open() {
  menu_ = {};
}

ModeResult SymbolicMode::handle_key(const KeyEvent& key, const KeyDef& def) {
  (void)key;
  return handle_simple_menu_key(def, menu_, kItemCount);
}

void SymbolicMode::render(MonoCanvas& canvas) {
  render_simple_menu(canvas, name(), kItems, kMessages, kItemCount, menu_);
}

}  // namespace esp32calc
