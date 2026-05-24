#include "core/calc_math.h"

double calc_add(double a, double b) { return a + b; }
double calc_sub(double a, double b) { return a - b; }
double calc_mul(double a, double b) { return a * b; }
double calc_div(double a, double b) { return (b == 0.0) ? INF : a / b; }
double calc_pow(double base, double exp) { return pow(base, exp); }
double calc_sqrt(double x) { return (x < 0) ? -sqrt(-x) : sqrt(x); }

double calc_sin(double x, angle_mode_t mode) {
    return sin((mode == ANGLE_DEG) ? x * PI / 180.0 : x);
}
double calc_cos(double x, angle_mode_t mode) {
    return cos((mode == ANGLE_DEG) ? x * PI / 180.0 : x);
}
double calc_tan(double x, angle_mode_t mode) {
    double rad = (mode == ANGLE_DEG) ? x * PI / 180.0 : x;
    double c = cos(rad);
    return (c == 0.0) ? INF : sin(rad) / c;
}

double calc_log(double x) { return (x <= 0) ? -INF : log10(x); }
double calc_ln(double x) { return (x <= 0) ? -INF : log(x); }
double calc_fact(double x) {
    int n = (int)x;
    if (n < 0) return -INF;
    double r = 1;
    for (int i = 2; i <= n; i++) r *= i;
    return r;
}
double calc_mod(double a, double b) { return (b == 0) ? INF : fmod(a, b); }
double calc_abs(double x) { return (x < 0) ? -x : x; }
double calc_neg(double x) { return -x; }

