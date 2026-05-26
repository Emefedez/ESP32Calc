#include "ui/modes/mode_registry.h"

namespace esp32calc {

const ModeDescriptor& standard_mode_descriptor();
const ModeDescriptor& graph_mode_descriptor();
const ModeDescriptor& symbolic_mode_descriptor();
const ModeDescriptor& programs_mode_descriptor();
const ModeDescriptor& files_mode_descriptor();
const ModeDescriptor& wireless_mode_descriptor();
const ModeDescriptor& settings_mode_descriptor();

const ModeDescriptorList& builtin_modes() {
  static const ModeDescriptorList kModes {{
      standard_mode_descriptor(),
      graph_mode_descriptor(),
      symbolic_mode_descriptor(),
      programs_mode_descriptor(),
      files_mode_descriptor(),
      wireless_mode_descriptor(),
      settings_mode_descriptor(),
  }};
  return kModes;
}

}  // namespace esp32calc
