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

inline constexpr size_t kSolveVariableCount = 6;
inline constexpr uint8_t kSolveVariableX = 1u << 0;
inline constexpr uint8_t kSolveVariableY = 1u << 1;
inline constexpr uint8_t kSolveVariableZ = 1u << 2;
inline constexpr uint8_t kSolveVariableA = 1u << 3;
inline constexpr uint8_t kSolveVariableB = 1u << 4;
inline constexpr uint8_t kSolveVariableC = 1u << 5;
inline constexpr char kSolveVariables[kSolveVariableCount] = {'x', 'y', 'z', 'a', 'b', 'c'};

struct SolveOptions {
  uint8_t solve_mask = 0;
  uint8_t parameter_mask = 0;
  uint8_t known_mask = 0;
  double known_values[kSolveVariableCount] {};
};

struct MathRequest {
  MathJobKind kind = MathJobKind::Numeric;
  char expression[128] {};
  SolveOptions solve_options {};
};

struct MathResult {
  bool ok = false;
  MathJobKind kind = MathJobKind::Numeric;
  ExpressionKind expression_kind = ExpressionKind::Invalid;
  char text[192] {};
};

}  // namespace esp32calc_alt

