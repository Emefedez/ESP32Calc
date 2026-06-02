#pragma once

#include <cstddef>
#include <cstdint>

namespace esp32calc_alt {

enum class MathJobKind : uint8_t {
  Numeric,
  Symbolic,
  Solve,
  Graph,
  Script,
};

enum class ExpressionKind : uint8_t {
  Invalid,
  Numeric,
  Symbolic,
  Equation,
};

inline constexpr size_t kGraphSampleCount = 96;
// how many variables can we use and how much space do we allocate for them?
inline constexpr size_t kSolveVariableCount = 6;
inline constexpr uint8_t kSolveVariableX = 1u << 0;
inline constexpr uint8_t kSolveVariableY = 1u << 1;
inline constexpr uint8_t kSolveVariableZ = 1u << 2;
inline constexpr uint8_t kSolveVariableA = 1u << 3;
inline constexpr uint8_t kSolveVariableB = 1u << 4;
inline constexpr uint8_t kSolveVariableC = 1u << 5;
inline constexpr char kSolveVariables[kSolveVariableCount] = {'x', 'y', 'z', 'a', 'b', 'c'};

struct SolveOptions {
  uint8_t solve_mask = 0; // the solve_mask identifies what variables we want to solve for, by default, it goes in appearance order.
  uint8_t parameter_mask = 0; // parameters are unsolved variables, so it is masked sepparately
  uint8_t known_mask = 0; // this is treated as a helper, these values are already "OK", so we compare what we have to what we want to get
  double known_values[kSolveVariableCount] {};
};

struct MathRequest {
  MathJobKind kind = MathJobKind::Numeric; // default state which may be changed
  char expression[224] {};
  SolveOptions solve_options {};
  float graph_x_min = -5.0f;
  float graph_x_max = 5.0f;
};

struct MathResult {
  bool ok = false;
  MathJobKind kind = MathJobKind::Numeric;
  ExpressionKind expression_kind = ExpressionKind::Invalid;
  char text[192] {};
  float graph_y[kGraphSampleCount] {};
  bool graph_valid[kGraphSampleCount] {};
  size_t graph_count = 0;
  float graph_x_min = -5.0f;
  float graph_x_max = 5.0f;
};

}  // namespace esp32calc_alt
