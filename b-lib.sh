#!/usr/bin/env bash
# Library build — produces libragbot.a (Qt mode + embedded_inference).
# Output: ../build-ragbot-lib/libragbot.a
set -euo pipefail

SOURCE_DIR=$(pwd)
BUILD_DIR="$SOURCE_DIR/../build-ragbot-lib"

rm -rf "$BUILD_DIR"
cmake -B "$BUILD_DIR" -S "$SOURCE_DIR" \
    -DRAGBOT_USE_QT=ON \
    -DRAGBOT_LIBRARY=ON \
    -DRAGBOT_EMBEDDED_INFERENCE=ON
cmake --build "$BUILD_DIR" -j"$(nproc)"
