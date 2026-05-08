#!/usr/bin/env bash
# Full clean rebuild — embedded_inference (links llama.cpp).
# Also regenerates compile_commands.json (CMake exports it natively).
# Output: ../build-ragbot/ragbot
set -euo pipefail

SOURCE_DIR=$(pwd)
BUILD_DIR="$SOURCE_DIR/../build-ragbot"

rm -rf "$BUILD_DIR"
cmake -B "$BUILD_DIR" -S "$SOURCE_DIR" \
    -DRAGBOT_EMBEDDED_INFERENCE=ON
cmake --build "$BUILD_DIR" -j"$(nproc)"
