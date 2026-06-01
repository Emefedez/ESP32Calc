#pragma once

namespace esp32calc_alt::graph_expression {

float evaluate(const char* expression, float x, bool& ok);
float evaluate_relation(const char* expression, float x, float y, bool& ok);
bool should_render_implicit(const char* expression);

}  // namespace esp32calc_alt::graph_expression
