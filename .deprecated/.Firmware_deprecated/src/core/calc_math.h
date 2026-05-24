#ifndef CALC_MATH_H
#define CALC_MATH_H

#include <stdint.h>
#include <math.h>

#ifndef PI
#define PI  3.14159265358979323846
#endif
#ifndef E
#define E   2.71828182845904523536
#endif
#ifndef INF
#define INF __builtin_inf()
#endif

typedef enum {
    ANGLE_DEG,
    ANGLE_RAD
} angle_mode_t;

// a + b
double calc_add(double a, double b);
// a - b
double calc_sub(double a, double b);
// a * b
double calc_mul(double a, double b);
// a / b, returns INF on divide-by-zero
double calc_div(double a, double b);
// base ^ exp
double calc_pow(double base, double exp);
// sqrt(x), returns -sqrt(-x) for negative input (complex-safe)
double calc_sqrt(double x);
// sin(x), converts to rad if mode == ANGLE_DEG
double calc_sin(double x, angle_mode_t mode);
// cos(x), converts to rad if mode == ANGLE_DEG
double calc_cos(double x, angle_mode_t mode);
// tan(x), converts to rad; returns INF at poles
double calc_tan(double x, angle_mode_t mode);
// log10(x), returns -INF for x <= 0
double calc_log(double x);
// ln(x), returns -INF for x <= 0
double calc_ln(double x);
// x! (factorial, integer only), returns -INF for negative
double calc_fact(double x);
// a % b (modulo), returns INF for b == 0
double calc_mod(double a, double b);
// |x|
double calc_abs(double x);
// -x
double calc_neg(double x);

#endif
