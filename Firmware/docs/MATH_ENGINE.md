# Math Engine

This document describes current calculator syntax and limits. It is intentionally
close to firmware behavior, not a wish list.

## Standard Numeric Input

Supported operators:

```text
+ unary plus
- unary minus
+ addition
- subtraction
* multiplication
/ division
^ exponent
(...) grouping
```

Examples:

```text
2+3*4       -> 14
(2+3)^2     -> 25
-2^2        -> -4
1/3         -> 1/3, then S<>D -> 0.333333333333
```

Scientific notation is accepted by the numeric parser through `strtod`, for
example `1E-3`. Exact fraction output is produced only when the rational parser
can represent the expression exactly and division is present.

## Symbolic Polynomial Input

The symbolic path supports one symbolic variable, `x`, with dynamic polynomial
terms. Multiplication may be explicit or implicit.

Examples:

```text
2x+3x       -> 5x
(x+1)^2     -> x^2+2x+1
x^-2        -> x^-2
d/dx(x^2)   -> 2x
int(x^2)    -> x^3/3
```

Exponent parsing is scoped: `x^2+1` means `(x^2)+1`; `x^(2+1)` means `x^3`.
Symbolic exponents must simplify to an integer from `-32` to `32`. Negative
powers are supported for monomial powers such as `x^-2`; division by a
non-constant polynomial is rejected.

`int(x^-1)` emits `ln(abs(x))` for the logarithmic term.

## Equations And Systems

Linear equations support variables:

```text
x,y,z,a,b,c
```

Separate equations with `;`:

```text
x+y=2;x-y=0       -> x=1; y=1
2x+3y=7           -> y=-0.6666666667x+2.333333333
```

The solver normalizes each equation into `A*x=b` and uses reduced row echelon
form. Inconsistent rows return `NO SOLUTION`; underdetermined variables are
reported as free.

Nonlinear single-variable polynomial equations in `x` are solved separately:

```text
x^2=2x       -> x=0; x=2
x^2+1=0      -> NO REAL ROOTS
```

Current polynomial equation limit is real degree 2.

## Graph Equations

Graph mode accepts:

```text
sin(x)
y=x^2
x^2=y
x^2+y^2=4
```

Explicit `y=...` and `...=y` are evaluated directly. Linear implicit equations
solve `y` analytically. Nonlinear implicit equations scan `y` from `-50` to
`50`, bisect sign changes, and choose one visible branch. When multiple roots
exist, non-negative `y` is preferred.

## Matrix Mode

Matrix text uses commas between columns and semicolons between rows:

```text
1,2;3,4
```

Mode shortcuts:

```text
ALPHA /    inserts ,
ALPHA =    inserts ;
SHIFT =    inserts =
```

Supported matrix actions:

```text
DET       det(1,2;3,4) -> -2
INVERSE   inv(1,2;3,4) -> [-2,1;1.5,-0.5]
SOLVE     x+y=2;x-y=0 -> x=1; y=1
```

Matrix determinant and inverse use dynamic storage and Gaussian elimination with
partial pivoting. Inverse requires a square non-singular matrix.

## Known Limits

- General CAS simplification beyond polynomials is not implemented.
- Trig/log symbolic differentiation is not implemented.
- Polynomial equation solving stops at real degree 2.
- Nonlinear systems are not implemented.
- Graph mode displays one branch for implicit nonlinear equations.
- UI entry fields are still bounded to keep embedded interaction predictable.
