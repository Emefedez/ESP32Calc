/*
 * math_engine.cpp basically calculates the intercepted math
 * requests from the UI and calls the appropriate GiacBridge
 * methods to evaluate them.
 */

#pragma once

#include "math/types.h"

namespace esp32calc_alt {

namespace giac {
class GiacBridge;
}

MathResult evaluate_math_request(const MathRequest& request, giac::GiacBridge& bridge);

}  // namespace esp32calc_alt
