# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

RAGBot is a C++ console application implementing a three-stage RAG pipeline for querying Cataclysm: Dark Days Ahead (CDDA) game data. It embeds JSON game objects into a vector database, retrieves relevant context for a question, synthesises a factual answer (Researcher), then delivers it in-character (Roleplayer).

## Directory Layout

```
ragbot/               ← source root (this repo)
../build-ragbot/      ← executable build output (sibling, created by CMake)
../build-ragbot-lib/  ← library build output (sibling, created by b-lib.sh)
```

All scripts are run from the **source root**. Build artefacts and the binary live in `../build-ragbot/`.

## Scripts

| Script | What it does |
|--------|-------------|
| `b.sh` | cmake configure + make — fast incremental build (network-only, no llama.cpp) |
| `b-clean.sh` | rm build dir + cmake + make — full rebuild with llama.cpp, regenerates `compile_commands.json` |
| `b-lib.sh` | Builds `libragbot.a` into `../build-ragbot-lib/` |
| `run.sh` | Copies `config.json` to `../build-ragbot/`, then launches the binary under **gdb** |
| `dump-cpp.sh` | Prints all `.h`/`.cpp` source files to stdout — useful for pasting into LLM context |

Typical workflow:
```bash
# Full rebuild with embedded inference
./b-clean.sh

# Run under gdb (copies config.json automatically)
./run.sh
```

## Build Modes

The project uses CMake. `compile_commands.json` is generated automatically (`CMAKE_EXPORT_COMPILE_COMMANDS ON`).

**Network-only (default)** — no llama.cpp dependency:
```bash
cmake -B ../build-ragbot -S .
cmake --build ../build-ragbot -j$(nproc)
```

**With embedded inference** — pulls in llama.cpp, OpenBLAS, Vulkan:
```bash
cmake -B ../build-ragbot -S . -DRAGBOT_EMBEDDED_INFERENCE=ON
cmake --build ../build-ragbot -j$(nproc)
```

**Library build** — produces `libragbot.a` instead of the console executable:
```bash
cmake -B ../build-ragbot-lib -S . -DRAGBOT_LIBRARY=ON
cmake --build ../build-ragbot-lib -j$(nproc)
```

## Dependencies

**Always required:** libcurl, nlohmann/json (v3.11.3, fetched by CMake), fmt (v10.2.1, fetched by CMake), sqlite3, pthreads.

**Embedded inference only** (pre-built static libs required):

| Library | Source | Build output |
|---------|--------|-------------|
| llama.cpp | `~/source/llama.cpp` | `~/source/build-llama.cpp/` |
| OpenBLAS | system | `-lopenblas` |
| Vulkan | system | `-lvulkan` |

Required llama.cpp libs: `libcommon.a`, `libllama.a`, `libggml.a`, `libggml-base.a`, `libggml-cpu.a`, `libggml-vulkan.a`

## Configuration

`config.json` (project root, copied to `../build-ragbot/` by `run.sh`) controls all three components. Each generator block uses `"backend": "embedded"` or `"backend": "network"`:

```json
{
  "reranker": {
    "enabled": false,
    "topN": 5,
    "generator": { "backend": "embedded", "modelPath": "/path/to/reranker.gguf" }
  },
  "embedder": {
    "name": "embeddings.db",
    "files": "/path/to/CDDA/data/json/monsters",
    "generator": { "backend": "embedded", "modelPath": "/path/to/nomic-embed.gguf" }
  },
  "researcher": {
    "instruction": "...",
    "generator": { "backend": "embedded", "modelPath": "/path/to/model.gguf" }
  },
  "roleplayer": {
    "enabled": true,
    "characterName": "Survivor",
    "characterBackground": "...",
    "assetsDir": "inputs",
    "generator": { "backend": "embedded", "modelPath": "/path/to/model.gguf" }
  }
}
```

All config key strings are defined as `inline const rb::String` constants in `src/ConfigKeys.h`.

**CLI flags** (override config at runtime):
- `-c / --config` — path to config JSON
- `-d / --data` — override `embedder.files`
- `-b / --db` — override database path
- `-s / --skip-index` — skip the embedding pass, go straight to chat
- `-l / --load` — index only, then exit

## Architecture

Four-stage pipeline (sequential — the roleplayer's prompt embeds the full research answer, so the stages cannot overlap):

```
User question
    │
    ▼
Embedder::search()          — embeds query, queries VectorIndex
    │  SearchResult[]
    ▼
Reranker::rerank()          — scores chunks, sorts descending, returns top-N
    │  SearchResult[]        (skipped if reranker disabled)
    ▼
Researcher::research()      — builds context block, calls TextGenerator (streamed)
    │  research answer
    ▼
Roleplayer::respond()       — formats prompt, calls TextGenerator in-character
    │
    ▼
Console output
```

**Session / orchestration layer** (`src/`):
- `RAGBotSession` — owns the worker thread, all pipeline objects (Embedder, Reranker, Researcher, Roleplayer, RoleplayDatabase), and conversation history. `ask(question, tokenSink)` is thread-safe and blocks until the answer returns.
- `RAGBot` — thin stdin-loop wrapper; reads questions from stdin, calls `session.ask()`, handles "quit"/"exit".

**Generation layer** (`src/generation/`):
- `EmbeddingGenerator` — interface: `generate(String) → Vector<float>`
- `TextGenerator` — interface: `generateText(system, stream, prompt, tokenSink) → String`; `TokenSink` is `std::function<void(std::string_view)>` — when set, streamed tokens go to the sink instead of stdout
- `GeneratorIP` — implements both; HTTP calls to OpenAI-compatible server (`/v1/embeddings`, `/v1/chat/completions`), SSE streaming
- `EmbeddedEmbeddingGenerator` — implements `EmbeddingGenerator`; in-process llama.cpp, pooling=MEAN
- `EmbeddedTextGenerator` — implements `TextGenerator`; in-process llama.cpp, ChatML prompt format
- `GeneratorFactory` — `createEmbedding(config)` / `createText(config)` read `"backend"` key and return `unique_ptr` to the right implementation

**Reranking layer** (`src/generation/`, `src/asset/`):
- `RerankGenerator` — interface: `score(query, docs) → Vector<float>`
- `RerankGeneratorIP` — network backend; Cohere `/v1/rerank` format
- `EmbeddedRerankGenerator` — llama.cpp cross-encoder; uses `LLAMA_POOLING_TYPE_RANK`; picks up model's built-in `rerank` chat template if present, otherwise falls back to EOS/SEP-separated query+document
- `Reranker` — asset class; scores all retrieved chunks, sorts descending, returns top-N; no-op if disabled or generator invalid
- `GeneratorFactory::createRerank(config)` — same embedded/network dispatch as other generators

**Parsing / indexing layer** (`src/parsers/`, `src/asset/Embedder.cpp`):
- `CDDAResolver` — `buildRegistry(dir)` scans all JSON files and builds an `id → rb::Json` map; `resolve(obj, registry)` walks `copy-from` chains and merges parent fields so each stored object contains complete effective stats
- `ParserJSON` — `objectToChunk(rb::Json)` returns a `Chunk{embedText, content}` where `embedText` is flattened natural language (for the embedding model) and `content` is compact JSON (stored in DB, sent to LLM)
- `Embedder` — drives file iteration; calls `CDDAResolver` then `ParserJSON` per object; skips `"abstract": true` base templates

**Storage** (`src/db/`):
- `EmbeddingDatabase` — SQLite (`sources`/`chunks` tables) + `VectorIndex` (hand-rolled brute-force cosine similarity). `SchemaVersion` constant triggers full re-index when bumped.
- `VectorIndex` — `std::vector<std::vector<float>>` with dot-product search + `std::partial_sort`; 768-dimensional (nomic-embed output)

## Schema Migration

`EmbeddingDatabase::SchemaVersion` (in `src/db/EmbeddingDatabase.h`) is an integer constant. Incrementing it drops and recreates all tables on next run, forcing a full re-index. Do this whenever the stored chunk format changes.

## Linting

`.clang-tidy` is present. `compile_commands.json` is generated automatically by CMake into `../build-ragbot/`.

```bash
clang-tidy -p ../build-ragbot src/RAGBot.cpp
```

## Input Prompt Files

`inputs/` (relative to working directory at runtime, i.e. `../build-ragbot/inputs/`):
- `roleplayPrompt.txt` — `%1` character name, `%2` research answer, `%3` question
- `survivorPrompt.txt`, `characterBackground.txt` — character context

## Library Build

RAGBot can be consumed as a static library (`libragbot.a`) by a host application (e.g. CDDA). The public surface is a pure C header: `src/ragbot_c_api.h`.

### Building with CMake

```bash
# Network-only inference (recommended for integration)
cmake -B ../build-ragbot-lib -S . -DRAGBOT_LIBRARY=ON
cmake --build ../build-ragbot-lib -j$(nproc)

# With embedded llama.cpp inference
cmake -B ../build-ragbot-lib -S . \
    -DRAGBOT_LIBRARY=ON \
    -DRAGBOT_EMBEDDED_INFERENCE=ON
cmake --build ../build-ragbot-lib -j$(nproc)
```

### Linking

```bash
g++ my_app.cpp -o my_app \
    -I path/to/ragbot/src \
    path/to/libragbot.a \
    -lcurl -lsqlite3 -lpthread -ldl -lm -lstdc++
# Embedded inference: also add -lcommon -lllama -lggml* -lopenblas -lgomp -lvulkan
# nlohmann/json and fmt are header-only / static — already inside libragbot.a
```

### API quick reference

| Function | Description |
|----------|-------------|
| `ragbot_set_log_callback(cb, user)` | Install log sink before `ragbot_create`; not thread-safe |
| `ragbot_create(config_json, assets_dir)` | Parse config, init pipeline; returns `NULL` on failure |
| `ragbot_destroy(session)` | Joins worker thread; blocks until any in-flight ask completes |
| `ragbot_set_npc_context(session, json)` | Sticky NPC context prepended to every question; thread-safe |
| `ragbot_set_world_context(session, json)` | Sticky world context prepended to every question; thread-safe |
| `ragbot_ask(session, q, on_token, on_done, user)` | Async ask; returns immediately; callbacks fire on worker thread |
| `ragbot_ask_blocking(session, q, buf, size)` | Blocking ask; returns full answer byte-length |

**Threading rule:** `on_token` and `on_done` callbacks fire on the ragbot internal worker thread. Marshal to your UI thread before touching UI state. Do not call `ragbot_ask` again from within `on_done`.

See `library-api.md` for the full API reference, JSON schemas for NPC/world context, and a minimal C example.
