#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t micropython_runtime_start(void* heap,
                                    size_t heap_size,
                                    const char* source,
                                    const char* import_path,
                                    char* output,
                                    size_t output_size,
                                    bool* script_ok);
void micropython_runtime_stop(void);
bool micropython_runtime_active(void);
void micropython_runtime_on_key(const char* token);

#ifdef __cplusplus
}
#endif
