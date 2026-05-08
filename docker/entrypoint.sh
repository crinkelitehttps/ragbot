#!/usr/bin/env bash
set -euo pipefail

MODEL_PATH=${MODEL_PATH:-/models/nomic-embed-text-v1.5.Q8_0.gguf}
HOST=${HOST:-0.0.0.0}
PORT=${PORT:-8080}
# nomic-embed-text-v1.5 trains at ctx 2048; --ctx-size > 2048 is silently capped.
# --ubatch-size is the physical batch size: must be >= the longest single input or
# requests fail with "input (N tokens) is too large to process".
CTX=${CTX:-2048}
BATCH=${BATCH:-2048}
UBATCH=${UBATCH:-2048}
N_GPU_LAYERS=${N_GPU_LAYERS:-99}

if [ ! -f "$MODEL_PATH" ]; then
    if [ -z "${MODEL_URL:-}" ]; then
        echo "ERROR: model not found at $MODEL_PATH and MODEL_URL is not set." >&2
        exit 1
    fi
    mkdir -p "$(dirname "$MODEL_PATH")"
    echo "Downloading model..."
    wget -q --show-progress -O "$MODEL_PATH" "$MODEL_URL"
fi

if command -v sshd >/dev/null 2>&1; then
    ssh-keygen -A >/dev/null 2>&1 || true
    /usr/sbin/sshd
    echo "sshd started on port 22"
fi

echo "Starting llama-server — model: $MODEL_PATH  host: $HOST:$PORT"
echo "Logs: /tmp/llama.log"

exec llama-server \
    --model          "$MODEL_PATH" \
    --host           "$HOST" \
    --port           "$PORT" \
    --ctx-size       "$CTX" \
    --batch-size     "$BATCH" \
    --ubatch-size    "$UBATCH" \
    --n-gpu-layers   "$N_GPU_LAYERS" \
    --embedding \
    --no-mmap >/tmp/llama.log 2>&1
