#!/usr/bin/env bash
# Rent a vast.ai GPU instance, run ragbot-textserver, and update config-vast.json.
# Usage: ./deploy-text-server.sh [--api-key KEY] [--max-price 0.50] [--gpu-ram 16]
#                                [--model-url URL] [--model-name NAME] [--yes]
set -euo pipefail

VAST="python3 /home/joe/source/vast-cli/vast.py"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STATE_FILE="$SCRIPT_DIR/.vast-text-instance-id"
CONFIG_OUT="$SCRIPT_DIR/config-vast.json"
IMAGE="crinkelite/ragbot-textserver:latest"
MODEL_URL_DEFAULT="https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-Q8_0.gguf"
MODEL_NAME_DEFAULT="Llama-3.2-3B-Instruct"
CONTAINER_PORT=8080
POLL_INTERVAL=15
POLL_TIMEOUT=300

API_KEY="${VASTAI_API_KEY:-}"
MAX_PRICE="0.50"
MIN_GPU_RAM="16"
SSH_KEY_FILE="${HOME}/.ssh/id_ed25519.pub"
MODEL_URL="$MODEL_URL_DEFAULT"
MODEL_NAME="$MODEL_NAME_DEFAULT"
YES=0

usage() {
    echo "Usage: $0 [--api-key KEY] [--max-price DOLLARS_PER_HR] [--gpu-ram GB]"
    echo "          [--model-url URL] [--model-name NAME] [--ssh-key PATH] [--yes]"
    echo "  --api-key     vast.ai API key (default: \$VASTAI_API_KEY)"
    echo "  --max-price   maximum price in \$/hr (default: $MAX_PRICE)"
    echo "  --gpu-ram     minimum GPU VRAM in GB (default: $MIN_GPU_RAM)"
    echo "  --model-url   HuggingFace URL for the GGUF model (default: Llama-3.2-3B Q8)"
    echo "  --model-name  model name written to config-vast.json (default: $MODEL_NAME_DEFAULT)"
    echo "  --ssh-key     path to SSH public key to attach (default: $SSH_KEY_FILE)"
    echo "  --yes         skip confirmation prompt"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --api-key) API_KEY="$2"; shift 2 ;;
        --max-price) MAX_PRICE="$2"; shift 2 ;;
        --gpu-ram) MIN_GPU_RAM="$2"; shift 2 ;;
        --model-url) MODEL_URL="$2"; shift 2 ;;
        --model-name) MODEL_NAME="$2"; shift 2 ;;
        --ssh-key) SSH_KEY_FILE="$2"; shift 2 ;;
        --yes|-y) YES=1; shift ;;
        -h|--help) usage ;;
        *) echo "Unknown argument: $1"; usage ;;
    esac
done

if [[ ! -f "$SSH_KEY_FILE" ]]; then
    echo "SSH public key not found at $SSH_KEY_FILE — pass --ssh-key PATH or generate one with ssh-keygen."
    exit 1
fi

KEY_ARG=""
[[ -n "$API_KEY" ]] && KEY_ARG="--api-key $API_KEY"

vast() { $VAST $KEY_ARG "$@"; }
vast_raw() { $VAST $KEY_ARG --raw "$@"; }

# ── 1. Find cheapest suitable offer ──────────────────────────────────────────
echo "Searching for offers (CUDA >= 12.4, VRAM >= ${MIN_GPU_RAM} GB, <= \$${MAX_PRICE}/hr)..."

OFFERS_JSON=$(vast_raw search offers \
    "cuda_vers >= 12.4 gpu_ram >= ${MIN_GPU_RAM} dph_total <= ${MAX_PRICE}" \
    --order dph_total --limit 5 --type on-demand 2>&1)

OFFER_COUNT=$(echo "$OFFERS_JSON" | python3 -c "import json,sys; d=json.load(sys.stdin); print(len(d))" 2>/dev/null || echo 0)

if [[ "$OFFER_COUNT" -eq 0 ]]; then
    echo "No offers found matching criteria. Try raising --max-price or lowering --gpu-ram."
    exit 1
fi

echo ""
echo "Top offers:"
echo "$OFFERS_JSON" | python3 -c "
import json, sys
offers = json.load(sys.stdin)
print(f\"  {'ID':>10}  {'GPU':<22}  {'VRAM':>6}  {'\$/hr':>6}  {'CUDA':>5}  Country\")
print(f\"  {'-'*10}  {'-'*22}  {'-'*6}  {'-'*6}  {'-'*5}  -------\")
for o in offers:
    vram_gb = o.get('gpu_ram', 0) / 1024
    print(f\"  {o['id']:>10}  {o.get('gpu_name','?'):<22}  {vram_gb:>5.0f}G  {o.get('dph_total',0):>6.4f}  {o.get('cuda_max_good',0):>5.1f}  {o.get('geolocation','?')}\")
"
echo ""

OFFER_ID=$(echo "$OFFERS_JSON" | python3 -c "import json,sys; print(json.load(sys.stdin)[0]['id'])")
OFFER_INFO=$(echo "$OFFERS_JSON" | python3 -c "
import json,sys
o=json.load(sys.stdin)[0]
print(f\"{o.get('gpu_name','?')} @ \${o.get('dph_total',0):.4f}/hr (CUDA {o.get('cuda_max_good','?')}, {o.get('gpu_ram',0):.0f} GB VRAM)\")
")

echo "Selected offer $OFFER_ID: $OFFER_INFO"
if [[ $YES -eq 0 ]]; then
    read -r -p "Proceed? [y/N] " CONFIRM
    [[ "$CONFIRM" =~ ^[Yy]$ ]] || { echo "Aborted."; exit 0; }
fi

# ── 2. Create instance ────────────────────────────────────────────────────────
echo ""
echo "Creating instance..."
CREATE_OUT=$(vast_raw create instance "$OFFER_ID" \
    --image "$IMAGE" \
    --env "-e MODEL_URL=$MODEL_URL -p ${CONTAINER_PORT}:${CONTAINER_PORT} -p 22:22" \
    --disk 20 \
    --args 2>&1)

INSTANCE_ID=$(echo "$CREATE_OUT" | python3 -c "import json,sys; d=json.load(sys.stdin); print(d['new_contract'])" 2>/dev/null || true)
if [[ -z "$INSTANCE_ID" ]]; then
    echo "Failed to create instance. Response:"
    echo "$CREATE_OUT"
    exit 1
fi

echo "$INSTANCE_ID" > "$STATE_FILE"
echo "Instance $INSTANCE_ID created. State saved to $STATE_FILE"

# ── 3. Poll until running ─────────────────────────────────────────────────────
echo ""
echo "Waiting for instance to reach 'running' status (timeout ${POLL_TIMEOUT}s)..."
ELAPSED=0
while true; do
    INST_JSON=$(vast_raw show instance "$INSTANCE_ID" 2>/dev/null || echo "{}")
    STATUS=$(echo "$INST_JSON" | python3 -c "import json,sys; print(json.load(sys.stdin).get('actual_status','unknown'))" 2>/dev/null || echo "unknown")

    printf "\r  [%3ds] status: %-15s" "$ELAPSED" "$STATUS"

    if [[ "$STATUS" == "running" ]]; then
        echo ""
        break
    fi

    if [[ $ELAPSED -ge $POLL_TIMEOUT ]]; then
        echo ""
        echo "Timed out waiting for instance to start. Check https://cloud.vast.ai/instances/"
        echo "Instance ID: $INSTANCE_ID"
        exit 1
    fi

    sleep $POLL_INTERVAL
    ELAPSED=$((ELAPSED + POLL_INTERVAL))
done

# ── 4. Attach SSH key ─────────────────────────────────────────────────────────
echo ""
echo "Attaching SSH key from $SSH_KEY_FILE..."
vast attach ssh "$INSTANCE_ID" "$SSH_KEY_FILE" || {
    echo "Warning: vast attach ssh failed — you can retry manually:"
    echo "  vast attach ssh $INSTANCE_ID $SSH_KEY_FILE"
}

# ── 5. Extract public address ─────────────────────────────────────────────────
INST_JSON=$(vast_raw show instance "$INSTANCE_ID" 2>/dev/null)

HOST=$(echo "$INST_JSON" | python3 -c "import json,sys; d=json.load(sys.stdin); print(d.get('public_ipaddr') or d.get('ssh_host',''))")
MAPPED_PORT=$(echo "$INST_JSON" | python3 -c "
import json, sys
d = json.load(sys.stdin)
ports = d.get('ports') or {}
key = '${CONTAINER_PORT}/tcp'
entries = ports.get(key, [])
if entries:
    print(entries[0]['HostPort'])
else:
    print('')
" 2>/dev/null || true)
SSH_PORT=$(echo "$INST_JSON" | python3 -c "
import json, sys
d = json.load(sys.stdin)
ports = d.get('ports') or {}
entries = ports.get('22/tcp', [])
if entries:
    print(entries[0]['HostPort'])
else:
    print(d.get('ssh_port') or '')
" 2>/dev/null || true)
SSH_HOST_OUT=$(echo "$INST_JSON" | python3 -c "import json,sys; d=json.load(sys.stdin); print(d.get('public_ipaddr') or d.get('ssh_host',''))" 2>/dev/null || true)

if [[ -z "$HOST" || -z "$MAPPED_PORT" ]]; then
    echo "Instance is running but port mapping not yet available. Raw instance info:"
    echo "$INST_JSON" | python3 -c "import json,sys; print(json.dumps(json.load(sys.stdin), indent=2))" 2>/dev/null || echo "$INST_JSON"
    echo ""
    echo "Check https://cloud.vast.ai/instances/ for the mapped port, then update config-vast.json manually."
    exit 1
fi

BASE_PATH="http://${HOST}:${MAPPED_PORT}/"
echo "Endpoint: $BASE_PATH"

# ── 5b. Write ~/.vast network marker ─────────────────────────────────────────
NETWORK_DIR="$HOME/.vast"
mkdir -p "$NETWORK_DIR"
NETWORK_FILE="$NETWORK_DIR/ragbot-text-${HOST}:${MAPPED_PORT}.network"
echo "$BASE_PATH" > "$NETWORK_FILE"
echo "$NETWORK_FILE" > "$SCRIPT_DIR/.vast-text-network-file"
echo "Network marker: $NETWORK_FILE"

# ── 6. Merge researcher/roleplayer into config-vast.json ─────────────────────
python3 - <<PYEOF
import json, os

config_path = "$CONFIG_OUT"
config = {}
if os.path.exists(config_path):
    with open(config_path) as f:
        config = json.load(f)

gen_block = {
    "backend": "network",
    "platform": "vast.ai-text",
    "modelName": "$MODEL_NAME"
}

config.setdefault("researcher", {})["generator"] = gen_block
config.setdefault("roleplayer", {})["generator"] = gen_block

with open(config_path, "w") as f:
    json.dump(config, f, indent=2)
    f.write("\n")

print(f"Updated $CONFIG_OUT")
PYEOF

echo ""
echo "Done. To run ragbot with the text server:"
echo "  ../build-ragbot/ragbot --skip-index --config $CONFIG_OUT"
echo ""
if [[ -n "$SSH_HOST_OUT" && -n "$SSH_PORT" ]]; then
    echo "To SSH into the instance:"
    echo "  ssh -p $SSH_PORT root@$SSH_HOST_OUT"
    echo ""
fi
echo "To tear down when finished:"
echo "  ./teardown-text-server.sh"
