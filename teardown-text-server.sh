#!/usr/bin/env bash
# Destroy the vast.ai text generation server instance and clean up local state.
# Usage: ./teardown-text-server.sh [--api-key KEY] [--id INSTANCE_ID]
set -euo pipefail

VAST="python3 /home/joe/source/vast-cli/vast.py"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STATE_FILE="$SCRIPT_DIR/.vast-text-instance-id"

API_KEY="${VASTAI_API_KEY:-}"
INSTANCE_ID=""

usage() {
    echo "Usage: $0 [--api-key KEY] [--id INSTANCE_ID]"
    echo "  --api-key  vast.ai API key (default: \$VASTAI_API_KEY)"
    echo "  --id       instance ID to destroy (default: read from $STATE_FILE)"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --api-key) API_KEY="$2"; shift 2 ;;
        --id) INSTANCE_ID="$2"; shift 2 ;;
        -h|--help) usage ;;
        *) echo "Unknown argument: $1"; usage ;;
    esac
done

if [[ -z "$INSTANCE_ID" ]]; then
    if [[ ! -f "$STATE_FILE" ]]; then
        echo "No instance ID given and $STATE_FILE not found."
        echo "Pass --id INSTANCE_ID explicitly."
        exit 1
    fi
    INSTANCE_ID=$(cat "$STATE_FILE")
fi

KEY_ARG=""
[[ -n "$API_KEY" ]] && KEY_ARG="--api-key $API_KEY"

echo "Destroying instance $INSTANCE_ID..."
$VAST $KEY_ARG destroy instance "$INSTANCE_ID"

rm -f "$STATE_FILE"
echo "Instance $INSTANCE_ID destroyed. State file removed."

NETWORK_FILE_TRACKER="$SCRIPT_DIR/.vast-text-network-file"
if [[ -f "$NETWORK_FILE_TRACKER" ]]; then
    NETWORK_FILE=$(cat "$NETWORK_FILE_TRACKER")
    if [[ -f "$NETWORK_FILE" ]]; then
        rm -f "$NETWORK_FILE"
        echo "Removed network marker: $NETWORK_FILE"
    fi
    rm -f "$NETWORK_FILE_TRACKER"
fi
