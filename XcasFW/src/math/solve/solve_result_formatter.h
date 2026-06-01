#pragma once

#include <cstddef>
#include <cstdint>

#include "math/types.h"

namespace esp32calc_alt::solve {

struct SolvePresentation {
  char display[192] {};
  uint8_t solution_count = 0;
};

SolvePresentation present_giac_solve_result(const char* cas_expression,
                                            const char* solve_command,
                                            const SolveOptions& options);

}  // namespace esp32calc_alt::solve
