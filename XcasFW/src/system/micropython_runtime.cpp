#include "system/micropython_runtime.h"

#include <cstring>

extern "C" {
#include "port/micropython_embed.h"
#include "py/compile.h"
#include "py/lexer.h"
#include "py/obj.h"
#include "py/objlist.h"
#include "py/parse.h"
#include "py/qstr.h"
#include "py/runtime.h"
}

#include "esp_log.h"

static bool g_runtime_active = false;
static mp_obj_t g_handle_key = mp_const_none;
static mp_stdout_cb_t g_stdout_cb = nullptr;

extern "C" void esp32calc_mpy_stdout_tx(const char* str, size_t len) {
  if (g_stdout_cb != nullptr) {
    g_stdout_cb(str, len);
  }
}

static bool exec_source(const char* source) {
  nlr_buf_t nlr;
  if (nlr_push(&nlr) == 0) {
    mp_lexer_t* lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_,
                                                source,
                                                std::strlen(source),
                                                0);
    const qstr source_name = lex->source_name;
    mp_parse_tree_t parse_tree = mp_parse(lex, MP_PARSE_FILE_INPUT);
    mp_obj_t module_fun = mp_compile(&parse_tree, source_name, true);
    mp_call_function_0(module_fun);
    nlr_pop();
    return true;
  }

  mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
  return false;
}

static void cache_handle_key(void) {
  g_handle_key = mp_const_none;
  nlr_buf_t nlr;
  if (nlr_push(&nlr) == 0) {
    mp_obj_t func = mp_load_name(qstr_from_str("handle_key"));
    if (mp_obj_is_callable(func)) {
      g_handle_key = func;
    }
    nlr_pop();
  }
}

static void call_handle_key(const char* token) {
  if (g_handle_key == mp_const_none) {
    return;
  }
  nlr_buf_t nlr;
  if (nlr_push(&nlr) == 0) {
    mp_obj_t arg = mp_obj_new_str(token, std::strlen(token));
    mp_call_function_1(g_handle_key, arg);
    nlr_pop();
  } else {
    mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
    g_handle_key = mp_const_none;
  }
}

static void add_import_path(const char* import_path) {
  if (import_path == nullptr || import_path[0] == '\0') {
    return;
  }
  if (mp_sys_path == MP_OBJ_NULL) {
    mp_sys_path = mp_obj_new_list(0, nullptr);
  }
  mp_obj_list_append(mp_sys_path, mp_obj_new_str(import_path, std::strlen(import_path)));
}

esp_err_t micropython_runtime_start(void* heap,
                                    size_t heap_size,
                                    const char* source,
                                    const char* import_path,
                                    bool* script_ok) {
  if (heap == nullptr || heap_size == 0 || source == nullptr) {
    return ESP_ERR_INVALID_ARG;
  }
  if (g_runtime_active) {
    micropython_runtime_stop();
  }

  int stack_anchor = 0;
  mp_embed_init(heap, heap_size, &stack_anchor);
  g_runtime_active = true;
  add_import_path(import_path);

  const bool ok = exec_source(source);
  if (script_ok != nullptr) {
    *script_ok = ok;
  }
  if (ok) {
    cache_handle_key();
  }
  return ESP_OK;
}

void micropython_runtime_stop(void) {
  if (!g_runtime_active) {
    return;
  }
  g_handle_key = mp_const_none;
  mp_embed_deinit();
  g_runtime_active = false;
}

bool micropython_runtime_active(void) {
  return g_runtime_active;
}

void micropython_runtime_on_key(const char* token) {
  if (!g_runtime_active || token == nullptr || token[0] == '\0') {
    return;
  }
  call_handle_key(token);
}

void micropython_runtime_set_stdout_callback(mp_stdout_cb_t cb) {
  g_stdout_cb = cb;
}
