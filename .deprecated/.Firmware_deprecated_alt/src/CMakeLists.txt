idf_component_register(
    SRCS "main.cpp"
         "display/gui.cpp"
         "core/keypad.cpp"
         "core/settings.cpp"
         "core/calc_math.cpp"
         "hal/battery.cpp"
         "screens/menu_system.cpp"
         "screens/calc_screen.cpp"
         "screens/about_screen.cpp"
         "screens/battery_screen.cpp"
         "screens/settings_screen.cpp"
    INCLUDE_DIRS "."
    REQUIRES Arduino
)
