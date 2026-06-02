#!/usr/bin/env sh
set -eu

cd "$(dirname "$0")"
pio run -e esp32-s3-wokwi
