# Open Source Calculator intended for Symbolic Calculus and custom programs, based on the ESP32-S3
* Main Features:
0.   Open Source Firmware
0.   Eink Screen
0.   SD Card slot
0.   Wireless Connectivity

![image](/calculator_pcb/ViewFront.jpg)
![image](/calculator_pcb/ViewBack.jpg)
![image](/CAD/exploded_view.png)

* Current 3D case: https://a360.co/4ukxaMP

> [!WARNING]
> * Using AI, `screen_color_probe.py` was created, a tool to help diagnose the simulated screen in wokwi.
> - AI was also used to port the Rust driver for the chosen display into a C++ based RTOS, interfacing so it is compatible with MonoCanvas and a custom `draw_raylib_region()` function that employs the new raylib with a software renderer that is slow enough to not be the "default choice" added onto eink.


https://github.com/user-attachments/assets/1842e5fe-03ce-4ea7-805b-3c79ca90bd16
