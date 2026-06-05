file(GLOB_RECURSE MICROPYTHON_EMBED_SRCS
  "micropython_embed/extmod/*.c"
  "micropython_embed/port/*.c"
  "micropython_embed/py/*.c"
  "micropython_embed/shared/*.c"
)

idf_component_register(
  SRCS ${MICROPYTHON_EMBED_SRCS}
  INCLUDE_DIRS
    "."
    "micropython_embed"
    "micropython_embed/port"
)

target_compile_options(${COMPONENT_LIB}
  PRIVATE
    -Wno-error
    -Wno-unused-parameter
    -Wno-missing-field-initializers
    -Wno-sign-compare
)

target_compile_definitions(${COMPONENT_LIB}
  PRIVATE
    # libtommath also exports mp_init(). Giac must bind to libtommath, while
    # MicroPython can keep its init private behind the embed API.
    mp_init=micropython_mp_init
)
