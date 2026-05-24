"""PlatformIO wrapper that replaces the stub with frozen MicroPython."""
import os
import shutil
import subprocess

Import("env")  # noqa: F821 - SCons magic

PROJECT_DIR = env.subst("$PROJECT_DIR")  # noqa: F821
BUILD_SCRIPT = os.path.join(PROJECT_DIR, "build.sh")
FROZEN_DIR = os.path.join(PROJECT_DIR, "build", "frozen")
PIO_BIN = os.path.join(env.subst("$BUILD_DIR"), "firmware.bin")
PIO_ELF = os.path.join(env.subst("$BUILD_DIR"), "firmware.elf")

# PlatformIO first builds a tiny Arduino placeholder. Removing the old output
# makes SCons rebuild that placeholder so the post-action below always runs.
for artifact in (PIO_BIN, PIO_ELF):
    if os.path.exists(artifact):
        os.remove(artifact)


def replace_with_micropython(target, source, env):
    print("Building frozen MicroPython firmware with Docker...")
    result = subprocess.call([BUILD_SCRIPT], cwd=PROJECT_DIR)
    if result:
        return result

    firmware_bin = os.path.join(FROZEN_DIR, "firmware.bin")
    firmware_elf = os.path.join(FROZEN_DIR, "firmware.elf")

    shutil.copyfile(firmware_bin, PIO_BIN)
    if os.path.exists(firmware_elf):
        shutil.copyfile(firmware_elf, PIO_ELF)

    print("PlatformIO artifact replaced with frozen MicroPython firmware:")
    print("  {}".format(PIO_BIN))
    return 0


env.AddPostAction(PIO_BIN, replace_with_micropython)  # noqa: F821
