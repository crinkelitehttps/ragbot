# Plan: Remote Embedding Server on vast.ai

## Goal

Run a llama-server container on a rented vast.ai GPU instance serving the
nomic-embed model. Point ragbot's `config.json` at it using `backend: "network"`,
then run `ragbot --load` locally to produce `embeddings.db`.

No changes to the ragbot source are required.

---

## How it fits together

```
Local machine                         vast.ai instance
─────────────────────────────         ──────────────────────────────────
ragbot --load                  ──→    llama-server (nomic-embed.gguf)
  config: backend=network             POST /v1/embeddings
  embedder.basePath=<ip:port>
  writes embeddings.db locally
```

---

## Decisions

| Question | Options | Recommendation |
|----------|---------|----------------|
| Server software | llama-server vs llama-swap | **llama-server** for a single-model job; llama-swap if you also want to test the researcher/roleplayer remotely |
| GPU backend | CUDA vs Vulkan | **CUDA** — vast.ai is NVIDIA |
| Model delivery | Bake into image vs download on start | **Download on start** via `wget` — keeps the image small and reusable |
| Port exposure | vast.ai TCP port mapping | Expose one port (default **8080**); vast.ai shows the mapped host port in the dashboard |
| Concurrency | `--parallel N` server-side | **Not useful** — `GeneratorIP::generate()` is fully serial (one HTTP POST per object); omitted from server flags |

---

## Phase 1 — Dockerfile ✓

Two-stage build. Stage 1 compiles llama-server with CUDA against a fresh
`--depth 1` clone of llama.cpp. Stage 2 is a slim CUDA runtime image with
only the binary copied in.

File: `Dockerfile.embedserver`

Key choices:
- `nvidia/cuda:12.4.1-devel-ubuntu22.04` builder → `nvidia/cuda:12.4.1-runtime-ubuntu22.04` runtime
- `-DGGML_CUDA=ON`, tests/examples off, only `llama-server` target built
- `--no-mmap` in server flags — avoids page-fault latency on first request

---

## Phase 2 — entrypoint.sh ✓

File: `docker/entrypoint.sh`

- Downloads model from `$MODEL_URL` if not already at `$MODEL_PATH`
- Fails fast with a clear error if model is missing and no URL is set
- Starts `llama-server` in `--embedding` mode with configurable ctx/batch sizes
- `--parallel` intentionally omitted (client is serial)

---

## Phase 3 — config.json for local ragbot

Switch the embedder generator to `network` and point it at the vast.ai instance.
Researcher and roleplayer can be left as-is or disabled during the indexing run.

```json
{
  "embedder": {
    "name": "embeddings.db",
    "files": "/path/to/CDDA/data/json",
    "generator": {
      "backend": "network",
      "basePath": "http://<vast-ip>:<mapped-port>/"
    }
  },
  "researcher": {
    "generator": { "backend": "network", "basePath": "..." }
  },
  "roleplayer": {
    "enabled": false,
    "generator": { "backend": "network" }
  }
}
```

Run locally:
```bash
ragbot --load --config config-vast.json
```

---

## Phase 4 — vast.ai deployment

1. **Build and push the image:**
   ```bash
   docker build -t yourname/ragbot-embed-server:latest -f Dockerfile.embedserver .
   docker push yourname/ragbot-embed-server:latest
   ```

2. **Rent an instance** on vast.ai:
   - Filter: `cuda >= 12.4`, GPU VRAM >= 8 GB (nomic-embed Q8 is ~300 MB)
   - Set **open ports**: `8080`
   - Docker image: `yourname/ragbot-embed-server:latest`
   - Environment variable: `MODEL_URL=https://huggingface.co/nomic-ai/nomic-embed-text-v1.5-GGUF/resolve/main/nomic-embed-text-v1.5.Q8_0.gguf`

3. **Note the mapped port** from the vast.ai dashboard (will not be 8080 on the public IP).

4. **Test the endpoint** before starting the full index:
   ```bash
   curl http://<vast-ip>:<port>/v1/embeddings \
     -H "Content-Type: application/json" \
     -d '{"model":"nomic","input":"test"}'
   ```

5. **Run the local index:**
   ```bash
   ragbot --load --config config-vast.json
   ```

6. **Tear down the instance** once `embeddings.db` is written.

---

## Open questions

- **llama-swap instead of llama-server?** Worth considering if you want a single
  remote endpoint to drive the full pipeline (researcher + roleplayer) during
  development, not just indexing.
- **llama.cpp commit pinning:** The network API is stable — any recent llama-server
  will do. The `--depth 1` clone in the Dockerfile always pulls HEAD.

---

## Work order

- [x] Write `Dockerfile.embedserver` and `docker/entrypoint.sh`
- [x] Build and smoke-test locally (`--build-arg CUDA=OFF`; confirmed 768-dim response from `/v1/embeddings`)
- [x] Push to registry (`crinkelite/ragbot-embedserver:latest`)
- [ ] Rent instance, set `MODEL_URL`, confirm `/v1/embeddings` responds
- [ ] Write `config-vast.json` with vast IP + mapped port
- [ ] Run `ragbot --load --config config-vast.json`
- [ ] Tear down instance
