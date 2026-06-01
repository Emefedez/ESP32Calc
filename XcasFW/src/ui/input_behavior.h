#pragma once

namespace esp32calc_alt::ui_behavior {

inline constexpr bool kShiftEqualsInsertsEquals = true;
inline constexpr bool kAlphaEqualsOpensGraph = true;
inline constexpr bool kShiftVariableOpensSelector = true;
inline constexpr bool kShiftVariableSquareOpensSelector = true;

inline constexpr const char* kShiftEqualsToken = "=";
inline constexpr const char* kGraphModeLabel = "GRAPH";

}  // namespace esp32calc_alt::ui_behavior
