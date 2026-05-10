#!/usr/bin/env bash
set -euo pipefail

MODEL_PATH=${MODEL_PATH:-/models/text-model.gguf}
HOST=${HOST:-0.0.0.0}
PORT=${PORT:-8080}
CTX=${CTX:-8192}
PARALLEL=${PARALLEL:-4}
BATCH=${BATCH:-512}
UBATCH=${UBATCH:-512}
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
    --parallel       "$PARALLEL" \
    --n-gpu-layers   "$N_GPU_LAYERS" \
    --no-mmap >/tmp/llama.log 2>&1
