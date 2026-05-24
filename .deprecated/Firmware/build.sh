#!/usr/bin/env bash
# Build the Wokwi/flashable MicroPython firmware image.
set -euo pipefail

cd "$(dirname "$0")"
exec ./tools/build_frozen_docker.sh "$@"
