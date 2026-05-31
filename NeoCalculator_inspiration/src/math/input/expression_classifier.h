#pragma once

#include "math/types.h"

namespace esp32calc_alt::input {

ExpressionKind classify_expression(const char* expr);
const char* expression_kind_label(ExpressionKind kind);

}  // namespace esp32calc_alt::input

