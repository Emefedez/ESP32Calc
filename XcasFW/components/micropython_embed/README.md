MicroPython embed component for ESP32Calc.

Generated from upstream MicroPython v1.26.0 using the embed port. The firmware
links this as a small VM that is started when an SD/internal MicroPython app is
opened and stopped when the app closes.

Regenerate from a MicroPython checkout:

```sh
make -f /path/to/micropython/ports/embed/embed.mk \
  MICROPYTHON_TOP=/path/to/micropython \
  PACKAGE_DIR=micropython_embed
```
