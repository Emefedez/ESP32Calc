# CMake for building calc_math_c as a static user C module in MicroPython firmware.
# Place this file in the MicroPython ports/esp32 directory's makefile include,
# or reference it via USER_C_MODULES when building:
#
#   make USER_C_MODULES=/path/to/modules/calc_math_c/micropython.cmake

add_library(usermod_calc_math_c INTERFACE)

target_sources(usermod_calc_math_c INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/calc_math_c.c
)

target_include_directories(usermod_calc_math_c INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(usermod_calc_math_c INTERFACE
    usermod_base
)
