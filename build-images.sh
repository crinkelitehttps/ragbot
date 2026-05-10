#!/usr/bin/env bash
# Build and push ragbot server Docker images to Docker Hub.
# Usage: ./build-images.sh [embed|text|combo|all]  (default: all)
# Requires: docker login crinkelite (or --no-push to skip push)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REGISTRY="crinkelite"
PUSH=1
TARGET="${1:-all}"

usage() {
    echo "Usage: $0 [embed|text|combo|all] [--no-push]"
    echo "  embed     build ragbot-embedserver"
    echo "  text      build ragbot-textserver"
    echo "  combo     build ragbot-comboserver"
    echo "  all       build all three (default)"
    echo "  --no-push skip docker push"
    exit 1
}

for arg in "$@"; do
    case "$arg" in
        --no-push) PUSH=0 ;;
        embed|text|combo|all) TARGET="$arg" ;;
        -h|--help) usage ;;
    esac
done

build_and_push() {
    local name="$1"
    local dockerfile="$2"
    local tag="${REGISTRY}/${name}:latest"

    echo ""
    echo "=== Building $tag ==="
    docker build -f "$SCRIPT_DIR/$dockerfile" -t "$tag" "$SCRIPT_DIR"

    if [[ $PUSH -eq 1 ]]; then
        echo "Pushing $tag..."
        docker push "$tag"
    fi
}

case "$TARGET" in
    embed) build_and_push ragbot-embedserver Dockerfile.embedserver ;;
    text)  build_and_push ragbot-textserver  Dockerfile.textserver  ;;
    combo) build_and_push ragbot-comboserver Dockerfile.comboserver ;;
    all)
        build_and_push ragbot-embedserver Dockerfile.embedserver
        build_and_push ragbot-textserver  Dockerfile.textserver
        build_and_push ragbot-comboserver Dockerfile.comboserver
        ;;
    *) usage ;;
esac

echo ""
echo "Done."
