#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*mp_stdout_cb_t)(const char* str, size_t len);

esp_err_t micropython_runtime_start(void* heap,
                                    size_t heap_size,
                                    const char* source,
                                    const char* import_path,
                                    bool* script_ok);
void micropython_runtime_stop(void);
bool micropython_runtime_active(void);
void micropython_runtime_on_key(const char* token);
void micropython_runtime_set_stdout_callback(mp_stdout_cb_t cb);

#ifdef __cplusplus
}
#endif
