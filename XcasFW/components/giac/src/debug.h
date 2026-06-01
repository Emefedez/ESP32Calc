#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#if 0
// fxlib.h is specific to Casio platforms. Disable on ESP32 builds.
// #include "fxlib.h"
#endif

#define DEBUG 0

void debug_init(void);
void debug(const char *format, ...);
void debug_quit(void);
