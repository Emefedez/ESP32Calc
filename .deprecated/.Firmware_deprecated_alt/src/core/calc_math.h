#ifndef CALC_MATH_H
#define CALC_MATH_H

typedef enum {
    ANGLE_DEG,
    ANGLE_RAD,
} angle_mode_t;

double calc_sin(double x, angle_mode_t mode);
double calc_cos(double x, angle_mode_t mode);
double calc_tan(double x, angle_mode_t mode);
double calc_asin(double x, angle_mode_t mode);
double calc_acos(double x, angle_mode_t mode);
double calc_atan(double x, angle_mode_t mode);
double calc_log(double x);
double calc_ln(double x);
double calc_sqrt(double x);
double calc_pow(double base, double exp);

#endif
