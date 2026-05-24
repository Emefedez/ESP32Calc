#include "core/calc_math.h"
#include <math.h>

#define DEG2RAD (3.14159265358979323846 / 180.0)

static double to_rad(double x, angle_mode_t mode) {
    return (mode == ANGLE_DEG) ? x * DEG2RAD : x;
}

static double from_rad(double x, angle_mode_t mode) {
    return (mode == ANGLE_DEG) ? x / DEG2RAD : x;
}

double calc_sin(double x, angle_mode_t mode) { return sin(to_rad(x, mode)); }
double calc_cos(double x, angle_mode_t mode) { return cos(to_rad(x, mode)); }
double calc_tan(double x, angle_mode_t mode) { return tan(to_rad(x, mode)); }
double calc_asin(double x, angle_mode_t mode) { return from_rad(asin(x), mode); }
double calc_acos(double x, angle_mode_t mode) { return from_rad(acos(x), mode); }
double calc_atan(double x, angle_mode_t mode) { return from_rad(atan(x), mode); }
double calc_log(double x) { return log10(x); }
double calc_ln(double x) { return log(x); }
double calc_sqrt(double x) { return sqrt(x); }
double calc_pow(double base, double exp) { return pow(base, exp); }
