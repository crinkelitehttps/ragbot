# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

RAGBot is a C++/Qt5 console application implementing a three-stage RAG pipeline for querying Cataclysm: Dark Days Ahead (CDDA) game data. It embeds JSON game objects into a vector database, retrieves relevant context for a question, synthesises a factual answer (Researcher), then delivers it in-character (Roleplayer).

## Directory Layout

```
ragbot/               ← source root (this repo)
../build-ragbot/      ← executable build output (sibling, created manually)
../build-ragbot-lib/  ← library build output (sibling, created by b-lib.sh)
```

All scripts are run from the **source root**. The build artefacts, Makefile, and binary live in `../build-ragbot/`.

## Scripts

| Script | What it does |
|--------|-------------|
| `b.sh` | qmake (no embedded inference) + make — fast incremental build |
| `b-clean.sh` | make clean + qmake `CONFIG+=embedded_inference` + `bear -- make` — full rebuild with llama.cpp, also regenerates `compile_commands.json` |
| `b-lib.sh` | Builds `libragbot.a` into `../build-ragbot-lib/` with `CONFIG+="library embedded_inference"` |
| `scan.sh` | Same as `b-clean.sh` — alias used when regenerating compile_commands is the goal |
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

The project has two build modes controlled by a qmake CONFIG flag:

**Network-only (default)** — no llama.cpp dependency, builds anywhere with Qt:
```bash
qmake ../ragbot
make -j$(nproc)
```

**With embedded inference** — pulls in llama.cpp, OpenBLAS, Vulkan:
```bash
qmake CONFIG+=embedded_inference ../ragbot
make -j$(nproc)
```

**Library build** — produces `libragbot.a` instead of the console executable; the two flags compose:
```bash
qmake CONFIG+="library embedded_inference" ../ragbot
make -j$(nproc)
```

## Dependencies

**Always required:** Qt 5.15.2 at `/home/joe/Qt/5.15.2/gcc_64` (not system Qt — the system has 5.15.18 which causes a runtime crash). System libs: `-lpthread -ldl -lm -lstdc++ -lsqlite3`.

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

All config key strings are defined as `inline const QLatin1String` constants in `src/ConfigKeys.h`.

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
- `RAGBotSession` — owns the worker `QThread`, all pipeline objects (Embedder, Reranker, Researcher, Roleplayer, RoleplayDatabase), and conversation history. `ask(question, tokenSink)` is thread-safe and blocks until the answer returns. Pipeline objects are constructed and destroyed on the worker thread.
- `RAGBot` — thin stdin-loop wrapper; reads questions from stdin, calls `session.ask()`, handles "quit"/"exit".

**Generation layer** (`src/generation/`):
- `EmbeddingGenerator` — interface: `generate(QString) → QVector<float>`
- `TextGenerator` — interface: `generateText(system, stream, prompt, tokenSink) → QString`; `TokenSink` is `std::function<void(QStringView)>` — when set, streamed tokens go to the sink instead of stdout
- `GeneratorIP` — implements both; HTTP calls to OpenAI-compatible server (`/v1/embeddings`, `/v1/chat/completions`), SSE streaming
- `EmbeddedEmbeddingGenerator` — implements `EmbeddingGenerator`; in-process llama.cpp, pooling=MEAN
- `EmbeddedTextGenerator` — implements `TextGenerator`; in-process llama.cpp, ChatML prompt format
- `GeneratorFactory` — `createEmbedding(config)` / `createText(config)` read `"backend"` key and return `unique_ptr` to the right implementation

**Reranking layer** (`src/generation/`, `src/asset/`):
- `RerankGenerator` — interface: `score(query, docs) → QVector<float>`
- `RerankGeneratorIP` — network backend; Cohere `/v1/rerank` format
- `EmbeddedRerankGenerator` — llama.cpp cross-encoder; uses `LLAMA_POOLING_TYPE_RANK`; picks up model's built-in `rerank` chat template if present, otherwise falls back to EOS/SEP-separated query+document
- `Reranker` — asset class; scores all retrieved chunks, sorts descending, returns top-N; no-op if disabled or generator invalid
- `GeneratorFactory::createRerank(config)` — same embedded/network dispatch as other generators

**Parsing / indexing layer** (`src/parsers/`, `src/asset/Embedder.cpp`):
- `CDDAResolver` — `buildRegistry(dir)` scans all JSON files and builds an `id → QJsonObject` map; `resolve(obj, registry)` walks `copy-from` chains and merges parent fields so each stored object contains complete effective stats
- `ParserJSON` — `objectToChunk(QJsonObject)` returns a `Chunk{embedText, content}` where `embedText` is flattened natural language (for the embedding model) and `content` is compact JSON (stored in DB, sent to LLM)
- `Embedder` — drives file iteration; calls `CDDAResolver` then `ParserJSON` per object; skips `"abstract": true` base templates

**Storage** (`src/db/`):
- `EmbeddingDatabase` — SQLite (`sources`/`chunks` tables) + `VectorIndex` (hand-rolled brute-force cosine similarity, replaces FAISS). `SchemaVersion` constant triggers full re-index when bumped.
- `VectorIndex` — `QVector<QVector<float>>` with dot-product search + `std::partial_sort`; 768-dimensional (nomic-embed output)

## Schema Migration

`EmbeddingDatabase::SchemaVersion` (in `src/db/EmbeddingDatabase.h`) is an integer constant. Incrementing it drops and recreates all tables on next run, forcing a full re-index. Do this whenever the stored chunk format changes.

## Linting

`.clang-tidy` is present. `compile_commands.json` is generated by `b-clean.sh` / `scan.sh` (via `bear -- make`).

```bash
clang-tidy -p ../build-ragbot src/RAGBot.cpp
```

## Input Prompt Files

`inputs/` (relative to working directory at runtime, i.e. `../build-ragbot/inputs/`):
- `roleplayPrompt.txt` — `%1` character name, `%2` research answer, `%3` question
- `survivorPrompt.txt`, `characterBackground.txt` — character context

## Library Build

RAGBot can be consumed as a static library (`libragbot.a`) by a host application (e.g. CDDA). The public surface is a pure C header with zero Qt symbols: `src/ragbot_c_api.h`.

### Building with CMake

```bash
# Qt build, network-only inference (recommended for integration)
cmake -B ../build-ragbot-lib -S . \
    -DRAGBOT_LIBRARY=ON \
    -DRAGBOT_USE_QT=ON
cmake --build ../build-ragbot-lib -j$(nproc)

# Qt build + embedded llama.cpp inference
cmake -B ../build-ragbot-lib -S . \
    -DRAGBOT_LIBRARY=ON \
    -DRAGBOT_USE_QT=ON \
    -DRAGBOT_EMBEDDED_INFERENCE=ON
cmake --build ../build-ragbot-lib -j$(nproc)

# Qt-free build (libcurl + nlohmann/json + {fmt} fetched automatically)
cmake -B ../build-ragbot-lib -S . \
    -DRAGBOT_LIBRARY=ON \
    -DRAGBOT_USE_QT=OFF
cmake --build ../build-ragbot-lib -j$(nproc)
```

### Building with qmake (embedded inference only)

```bash
./b-lib.sh    # outputs libragbot.a to ../build-ragbot-lib/
```

### Linking

```bash
g++ my_app.cpp -o my_app \
    -I path/to/ragbot/src \
    path/to/libragbot.a \
    -lQt5Core -lQt5Network -lsqlite3 -lpthread -ldl -lm -lstdc++
# Embedded inference: also add -lcommon -lllama -lggml* -lopenblas -lgomp -lvulkan
```

> **Qt version constraint:** the library must be linked with Qt 5.15.2 from `/home/joe/Qt/5.15.2/gcc_64`. The system Qt (5.15.18) causes a runtime crash. If the host application already links Qt, it must use a compatible build or arrange ABI isolation.

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
