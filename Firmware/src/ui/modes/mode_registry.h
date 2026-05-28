#pragma once

#include <array>
#include <cstddef>
#include <new>

#include "ui/modes/ui_mode.h"

namespace esp32calc {

using ModeConstructFn = UiMode* (*)(void* storage);
using ModeDestroyFn = void (*)(UiMode* mode);

struct ModeDescriptor {
  const char* label;
  ModeConstructFn construct;
  ModeDestroyFn destroy;
  size_t storage_size;
  size_t storage_align;
};

inline constexpr size_t kModeCount = 6;
inline constexpr size_t kModeStorageSize = 256;
inline constexpr size_t kModeStorageAlign = alignof(std::max_align_t);

using ModeDescriptorList = std::array<ModeDescriptor, kModeCount>;

template <typename Mode>
UiMode* construct_mode(void* storage) {
  static_assert(sizeof(Mode) <= kModeStorageSize, "Mode is too large for mode storage");
  static_assert(alignof(Mode) <= kModeStorageAlign, "Mode alignment exceeds mode storage");
  return new (storage) Mode();
}

template <typename Mode>
void destroy_mode(UiMode* mode) {
  static_cast<Mode*>(mode)->~Mode();
}

template <typename Mode>
constexpr ModeDescriptor make_mode_descriptor(const char* label) {
  static_assert(sizeof(Mode) <= kModeStorageSize, "Mode is too large for mode storage");
  static_assert(alignof(Mode) <= kModeStorageAlign, "Mode alignment exceeds mode storage");
  return {
      label,
      &construct_mode<Mode>,
      &destroy_mode<Mode>,
      sizeof(Mode),
      alignof(Mode),
  };
}

const ModeDescriptorList& builtin_modes();

}  // namespace esp32calc
