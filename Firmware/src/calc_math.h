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

double calc_add(double a, double b);
double calc_sub(double a, double b);
double calc_mul(double a, double b);
double calc_div(double a, double b);
double calc_pow(double base, double exp);
double calc_sqrt(double x);
double calc_sin(double x, angle_mode_t mode);
double calc_cos(double x, angle_mode_t mode);
double calc_tan(double x, angle_mode_t mode);
double calc_log(double x);
double calc_ln(double x);
double calc_fact(double x);
double calc_mod(double a, double b);
double calc_abs(double x);
double calc_neg(double x);

#endif
