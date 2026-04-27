# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

RAGBot is a C++/Qt5 console application implementing a three-stage RAG pipeline for querying Cataclysm: Dark Days Ahead (CDDA) game data. It embeds JSON game objects into a vector database, retrieves relevant context for a question, synthesises a factual answer (Researcher), then delivers it in-character (Roleplayer).

## Directory Layout

```
ragbot/           ← source root (this repo)
../build-ragbot/  ← build output directory (sibling, created manually)
```

All scripts are run from the **source root**. The build artefacts, Makefile, and binary live in `../build-ragbot/`.

## Scripts

| Script | What it does |
|--------|-------------|
| `b.sh` | qmake (no embedded inference) + make — fast incremental build |
| `b-clean.sh` | make clean + qmake `CONFIG+=embedded_inference` + `bear -- make` — full rebuild with llama.cpp, also regenerates `compile_commands.json` |
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
  "enableRoleplay": true,
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
    "characterName": "Survivor",
    "characterBackground": "...",
    "generator": { "backend": "embedded", "modelPath": "/path/to/model.gguf" }
  }
}
```

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

**Generation layer** (`src/generation/`):
- `EmbeddingGenerator` — interface: `generate(QString) → QVector<float>`
- `TextGenerator` — interface: `generateText(system, stream, prompt) → QString`
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
