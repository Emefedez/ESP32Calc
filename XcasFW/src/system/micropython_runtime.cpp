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

static char* g_output = nullptr;
static size_t g_output_size = 0;
static size_t g_output_used = 0;
static bool g_output_truncated = false;
static bool g_runtime_active = false;
static mp_obj_t g_handle_key = mp_const_none;

static void output_append_char(char ch) {
  if (g_output == nullptr || g_output_size == 0) {
    return;
  }
  if (g_output_used + 1 >= g_output_size) {
    g_output_truncated = true;
    return;
  }
  g_output[g_output_used++] = ch;
  g_output[g_output_used] = '\0';
}

static void output_append_text(const char* text) {
  while (text != nullptr && *text != '\0') {
    output_append_char(*text++);
  }
}

static void output_reset(char* output, size_t output_size) {
  g_output = output;
  g_output_size = output_size;
  g_output_used = 0;
  g_output_truncated = false;
  if (g_output != nullptr && g_output_size > 0) {
    g_output[0] = '\0';
  }
}

static void output_trim(void) {
  if (g_output == nullptr || g_output_size == 0) {
    return;
  }
  while (g_output_used > 0 &&
         (g_output[g_output_used - 1] == ' ' || g_output[g_output_used - 1] == '\n' ||
          g_output[g_output_used - 1] == '\r' || g_output[g_output_used - 1] == '\t')) {
    --g_output_used;
  }
  g_output[g_output_used] = '\0';
}

extern "C" void esp32calc_mpy_stdout_tx(const char* str, size_t len) {
  bool last_was_space = g_output_used > 0 && g_output[g_output_used - 1] == ' ';
  for (size_t i = 0; i < len; ++i) {
    const unsigned char ch = (unsigned char)str[i];
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n' || ch == '\t') {
      if (!last_was_space && g_output_used > 0) {
        output_append_char(' ');
        last_was_space = true;
      }
      continue;
    }
    output_append_char((ch >= 32 && ch <= 126) ? (char)ch : '?');
    last_was_space = ch == ' ';
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
                                    char* output,
                                    size_t output_size,
                                    bool* script_ok) {
  if (heap == nullptr || heap_size == 0 || source == nullptr) {
    return ESP_ERR_INVALID_ARG;
  }
  if (g_runtime_active) {
    micropython_runtime_stop();
  }

  output_reset(output, output_size);
  int stack_anchor = 0;
  mp_embed_init(heap, heap_size, &stack_anchor);
  g_runtime_active = true;
  add_import_path(import_path);

  const bool ok = exec_source(source);
  output_trim();
  if (g_output_truncated) {
    output_append_text("...");
  }
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
  output_reset(nullptr, 0);
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
