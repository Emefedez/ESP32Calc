#include "ui/modes/mode_registry.h"

#include "ui/modes/mode_common.h"

namespace {

constexpr esp32calc::ModeMenuItem kItems[] = {
    {"RUN", "APP"},
    {"EDIT", "CODE"},
    {"NEW", "FILE"},
    {"DELETE", "APP"},
};
constexpr const char* kMessages[] = {
    "PROGRAM RUNNER",
    "PROGRAM EDITOR",
    "NEW PROGRAM",
    "DELETE PROGRAM",
};
constexpr size_t kItemCount = sizeof(kItems) / sizeof(kItems[0]);

}  // namespace

namespace esp32calc {

class ProgramsMode final : public UiMode {
 public:
  const char* name() const override;
  void on_open() override;
  ModeResult handle_key(const KeyEvent& key, const KeyDef& def) override;
  void render(MonoCanvas& canvas) override;

 private:
  SimpleMenuState menu_ {};
};

const ModeDescriptor& programs_mode_descriptor() {
  static constexpr ModeDescriptor kDescriptor = make_mode_descriptor<ProgramsMode>("PROGRAMS");
  return kDescriptor;
}

const char* ProgramsMode::name() const {
  return "PROGRAMS";
}

void ProgramsMode::on_open() {
  menu_ = {};
}

ModeResult ProgramsMode::handle_key(const KeyEvent& key, const KeyDef& def) {
  (void)key;
  return handle_simple_menu_key(def, menu_, kItemCount);
}

void ProgramsMode::render(MonoCanvas& canvas) {
  render_simple_menu(canvas, name(), kItems, kMessages, kItemCount, menu_);
}

}  // namespace esp32calc
