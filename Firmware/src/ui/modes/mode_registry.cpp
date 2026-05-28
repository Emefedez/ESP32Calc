#include "ui/modes/mode_registry.h"

namespace esp32calc {
// descriptors are used to identify modes, which are given to the constructors
const ModeDescriptor& standard_mode_descriptor();
const ModeDescriptor& matrix_mode_descriptor();
const ModeDescriptor& programs_mode_descriptor();
const ModeDescriptor& files_mode_descriptor();
const ModeDescriptor& wireless_mode_descriptor();
const ModeDescriptor& settings_mode_descriptor();

const ModeDescriptorList& builtin_modes() {
  static const ModeDescriptorList kModes {{
      standard_mode_descriptor(),
      matrix_mode_descriptor(),
      programs_mode_descriptor(),
      files_mode_descriptor(),
      wireless_mode_descriptor(),
      settings_mode_descriptor(),
  }};
  return kModes;
}

}  // namespace esp32calc
