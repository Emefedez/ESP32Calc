file(GLOB LIBTOMMATH_SOURCES "*.c")

idf_component_register(
  SRCS
    ${LIBTOMMATH_SOURCES}
  INCLUDE_DIRS
    "."
)
