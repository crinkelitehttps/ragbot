#!/usr/bin/env bash
set -euo pipefail

EMBED_MODEL_PATH=${EMBED_MODEL_PATH:-/models/embed-model.gguf}
TEXT_MODEL_PATH=${TEXT_MODEL_PATH:-/models/text-model.gguf}
HOST=${HOST:-0.0.0.0}
EMBED_PORT=${EMBED_PORT:-8080}
TEXT_PORT=${TEXT_PORT:-8081}

# nomic-embed: full ctx across parallel slots; ubatch >= longest input
EMBED_CTX=${EMBED_CTX:-32768}
EMBED_PARALLEL=${EMBED_PARALLEL:-16}
EMBED_BATCH=${EMBED_BATCH:-2048}
EMBED_UBATCH=${EMBED_UBATCH:-2048}

TEXT_CTX=${TEXT_CTX:-8192}
TEXT_PARALLEL=${TEXT_PARALLEL:-4}
TEXT_BATCH=${TEXT_BATCH:-512}
TEXT_UBATCH=${TEXT_UBATCH:-512}

N_GPU_LAYERS=${N_GPU_LAYERS:-99}

if [ ! -f "$EMBED_MODEL_PATH" ]; then
    if [ -z "${EMBED_MODEL_URL:-}" ]; then
        echo "ERROR: embed model not found at $EMBED_MODEL_PATH and EMBED_MODEL_URL is not set." >&2
        exit 1
    fi
    mkdir -p "$(dirname "$EMBED_MODEL_PATH")"
    echo "Downloading embedding model..."
    wget -q --show-progress -O "$EMBED_MODEL_PATH" "$EMBED_MODEL_URL"
fi

if [ ! -f "$TEXT_MODEL_PATH" ]; then
    if [ -z "${TEXT_MODEL_URL:-}" ]; then
        echo "ERROR: text model not found at $TEXT_MODEL_PATH and TEXT_MODEL_URL is not set." >&2
        exit 1
    fi
    mkdir -p "$(dirname "$TEXT_MODEL_PATH")"
    echo "Downloading text model..."
    wget -q --show-progress -O "$TEXT_MODEL_PATH" "$TEXT_MODEL_URL"
fi

if command -v sshd >/dev/null 2>&1; then
    ssh-keygen -A >/dev/null 2>&1 || true
    /usr/sbin/sshd
    echo "sshd started on port 22"
fi

echo "Starting embedding server on port $EMBED_PORT..."
llama-server \
    --model        "$EMBED_MODEL_PATH" \
    --host         "$HOST" \
    --port         "$EMBED_PORT" \
    --ctx-size     "$EMBED_CTX" \
    --batch-size   "$EMBED_BATCH" \
    --ubatch-size  "$EMBED_UBATCH" \
    --parallel     "$EMBED_PARALLEL" \
    --n-gpu-layers "$N_GPU_LAYERS" \
    --embedding \
    --no-mmap >/tmp/llama-embed.log 2>&1 &
EMBED_PID=$!

echo "Starting text server on port $TEXT_PORT..."
llama-server \
    --model        "$TEXT_MODEL_PATH" \
    --host         "$HOST" \
    --port         "$TEXT_PORT" \
    --ctx-size     "$TEXT_CTX" \
    --batch-size   "$TEXT_BATCH" \
    --ubatch-size  "$TEXT_UBATCH" \
    --parallel     "$TEXT_PARALLEL" \
    --n-gpu-layers "$N_GPU_LAYERS" \
    --no-mmap >/tmp/llama-text.log 2>&1 &
TEXT_PID=$!

echo "Both servers running. Logs: /tmp/llama-embed.log  /tmp/llama-text.log"

# Exit when either server crashes so the container restarts or can be inspected
wait -n "$EMBED_PID" "$TEXT_PID"
echo "A server exited unexpectedly. Check /tmp/llama-embed.log and /tmp/llama-text.log"
sleep 60
exit 1
