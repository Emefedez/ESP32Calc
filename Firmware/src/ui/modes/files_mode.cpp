#include "ui/modes/mode_registry.h"

#include "ui/modes/mode_common.h"

namespace {

constexpr esp32calc::ModeMenuItem kItems[] = {
    {"BROWSE", "SD"},
    {"SAVE AS", "FILE"},
    {"LOAD", "FILE"},
    {"INFO", "CARD"},
};
constexpr const char* kMessages[] = {
    "FILE BROWSER",
    "SAVE WORKSPACE",
    "LOAD WORKSPACE",
    "STORAGE INFO",
};
constexpr size_t kItemCount = sizeof(kItems) / sizeof(kItems[0]);

}  // namespace

namespace esp32calc {

class FilesMode final : public UiMode {
 public:
  const char* name() const override;
  void on_open() override;
  ModeResult handle_key(const KeyEvent& key, const KeyDef& def) override;
  void render(MonoCanvas& canvas) override;

 private:
  SimpleMenuState menu_ {};
};

const ModeDescriptor& files_mode_descriptor() {
  static constexpr ModeDescriptor kDescriptor = make_mode_descriptor<FilesMode>("FILES");
  return kDescriptor;
}

const char* FilesMode::name() const {
  return "FILES";
}

void FilesMode::on_open() {
  menu_ = {};
}

ModeResult FilesMode::handle_key(const KeyEvent& key, const KeyDef& def) {
  (void)key;
  return handle_simple_menu_key(def, menu_, kItemCount);
}

void FilesMode::render(MonoCanvas& canvas) {
  render_simple_menu(canvas, name(), kItems, kMessages, kItemCount, menu_);
}

}  // namespace esp32calc
