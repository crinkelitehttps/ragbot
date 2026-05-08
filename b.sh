#!/usr/bin/env bash
# Fast incremental build — network-only (no llama.cpp).
# Output: ../build-ragbot/ragbot
set -euo pipefail

SOURCE_DIR=$(pwd)
BUILD_DIR="$SOURCE_DIR/../build-ragbot"

cmake -B "$BUILD_DIR" -S "$SOURCE_DIR"
cmake --build "$BUILD_DIR" -j"$(nproc)"

cp "$SOURCE_DIR/conversations.db" "$BUILD_DIR/" 2>/dev/null || true
