// C extension module for ESP32Calc — symbolic math (derivatives, etc.)
//
// Build as MicroPython user C module (static or .mpy native).
// With dynamic runtime (.mpy), compile:
//   $ mpy-cross -march=xtensawin calc_math_c.c -o calc_math_c.mpy
// Or bake into firmware via USER_C_MODULES.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// MicroPython runtime headers
#include "py/dynruntime.h"

// ── Simple expression parser state ──────────────────────────────────
// This is a stub — replace with a real shunting-yard / AST later.

typedef struct {
    char op;
    double val;
} Token;

// ── differentiate: symbolic diff of simple polynomial expressions ──
//   differentiate("x^2+3*x", "x") -> "2*x+3"
static mp_obj_t module_differentiate(mp_obj_t expr_in, mp_obj_t var_in) {
    const char *expr = mp_obj_str_get_str(expr_in);
    const char *var  = mp_obj_str_get_str(var_in);

    // VERY basic stub: returns a placeholder
    // TODO: implement recursive descent parser + diff rules
    char buf[128];
    snprintf(buf, sizeof(buf), "d/d%s(%s)", var, expr);

    return mp_obj_new_str(buf, strlen(buf));
}
static MP_DEFINE_CONST_FUN_OBJ_2(module_differentiate_obj, module_differentiate);

// ── integrate_numeric: Simpson's rule ───────────────────────────────
//   integrate_numeric("x^2", 0, 1) -> 0.3333
// Accepts a Python callable for the integrand.
static mp_obj_t module_integrate_numeric(mp_obj_t fn_obj, mp_obj_t a_obj, mp_obj_t b_obj) {
    mp_float_t a = mp_obj_get_float(a_obj);
    mp_float_t b = mp_obj_get_float(b_obj);
    int n = 100;
    mp_float_t h = (b - a) / n;
    mp_float_t s = 0;

    mp_obj_t args[1];
    for (int i = 0; i <= n; i++) {
        mp_float_t x = a + i * h;
        args[0] = mp_obj_new_float(x);
        mp_obj_t y = mp_call_function_n_kw(fn_obj, 1, 0, &args[0]);
        mp_float_t fy = mp_obj_get_float(y);
        if (i == 0 || i == n)
            s += fy;
        else if (i % 2 == 1)
            s += 4 * fy;
        else
            s += 2 * fy;
    }
    s *= h / 3;
    return mp_obj_new_float(s);
}
static MP_DEFINE_CONST_FUN_OBJ_3(module_integrate_numeric_obj, module_integrate_numeric);

// ── Module init ─────────────────────────────────────────────────────
mp_obj_t mpy_init(mp_obj_fun_bc_t *self, size_t n_args, size_t n_kw, mp_obj_t *args) {
    MP_DYNRUNTIME_INIT_ENTRY

    mp_store_global(qstr_from_str("differentiate"),
                    MP_OBJ_FROM_PTR(&module_differentiate_obj));
    mp_store_global(qstr_from_str("integrate_numeric"),
                    MP_OBJ_FROM_PTR(&module_integrate_numeric_obj));

    MP_DYNRUNTIME_INIT_EXIT
}
