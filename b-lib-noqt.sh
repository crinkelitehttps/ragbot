#!/usr/bin/env bash
# Library build — Qt-FREE libragbot.a (embedded_inference, libcurl path).
# Wired up incrementally as phases land; currently configures but cannot link
# until the compat layer + dual HTTP impl are in place.
# Output: ../build-ragbot-lib-noqt/libragbot.a
set -euo pipefail

SOURCE_DIR=$(pwd)
BUILD_DIR="$SOURCE_DIR/../build-ragbot-lib-noqt"

rm -rf "$BUILD_DIR"
cmake -B "$BUILD_DIR" -S "$SOURCE_DIR" \
    -DRAGBOT_USE_QT=OFF \
    -DRAGBOT_LIBRARY=ON \
    -DRAGBOT_EMBEDDED_INFERENCE=ON
cmake --build "$BUILD_DIR" -j"$(nproc)"
