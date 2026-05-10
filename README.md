# RAGBot

A C++ console application that answers questions about [Cataclysm: Dark Days Ahead](https://cataclysmdda.org) (CDDA) by retrieving the relevant game data and feeding it to an LLM. Two-stage generation: a **Researcher** produces a factual answer from retrieved context, then a **Roleplayer** delivers it in-character as a post-apocalyptic survivor.

## What it does

1. **Index** — walks CDDA's JSON data files, resolves `copy-from` inheritance chains, flattens each object into natural-language text, embeds it, and stores both the embedding and the original JSON in a local SQLite database.
2. **Retrieve** — embeds the user's question and finds the top-K most similar chunks via cosine similarity. Optional reranker pass for better precision.
3. **Research** — sends retrieved JSON chunks plus the question to an LLM with an instruction to answer factually from the context.
4. **Roleplay** — wraps the research answer in character voice using a configurable persona prompt.

Both LLM stages can run against either an embedded `llama.cpp` (in-process, GPU-accelerated) or any OpenAI-compatible HTTP endpoint (local llama-server, llama-swap, vast.ai, etc.).

## Quick start

```bash
# 1. Build (network-only — no llama.cpp required)
./b-clean.sh

# 2. Edit config.json — point embedder.files at your CDDA data dir,
#    set generator backends ("embedded" or "network").

# 3. Run (copies config.json into ../build-ragbot/ and launches under gdb)
./run.sh
```

On first run the indexer scans the CDDA JSON tree and populates `embeddings.db`. This takes a while (multiple hours for a full data tree on network embedders). Subsequent runs reuse the database; pass `-s` to skip the index pass entirely.

## Build modes

| Command | Result |
|---------|--------|
| `./b.sh` | Fast incremental rebuild, network-only |
| `./b-clean.sh` | Clean rebuild, network-only — regenerates `compile_commands.json` |
| `cmake -B ../build-ragbot -S . -DRAGBOT_EMBEDDED_INFERENCE=ON && cmake --build ../build-ragbot -j$(nproc)` | Build with in-process llama.cpp inference |
| `./b-lib.sh` | Build `libragbot.a` for embedding into a host application |

Build artefacts land in `../build-ragbot/` (executable) or `../build-ragbot-lib/` (library), sibling to the source root.

## Configuration

`config.json` in the project root drives all three components. Each generator block selects a backend:

```json
{
  "embedder": {
    "name": "embeddings.db",
    "files": "/path/to/CDDA/data/json",
    "topK": 3,
    "generator": { "backend": "network", "platform": "vast.ai" }
  },
  "reranker": {
    "enabled": false,
    "topN": 5,
    "generator": { "backend": "embedded", "modelPath": "/path/to/reranker.gguf" }
  },
  "researcher": {
    "instruction": "You are a helpful assistant analyzing CDDA game data. Answer using only the provided context.",
    "generator": { "backend": "network", "platform": "vast.ai-text", "modelName": "Llama-3.2-3B-Instruct" }
  },
  "roleplayer": {
    "enabled": true,
    "characterName": "Survivor",
    "characterBackground": "You are a survivor in a post-apocalyptic world...",
    "generator": { "backend": "embedded", "modelPath": "/path/to/model.gguf" }
  }
}
```

### CLI flags

| Flag | Meaning |
|------|---------|
| `-c`, `--config <path>` | Path to config JSON (default `./config.json`) |
| `-d`, `--data <dir>` | Override `embedder.files` |
| `-b`, `--db <path>` | Override database path |
| `-s`, `--skip-index` | Skip indexing — go straight to chat |
| `-l`, `--load` | Index only, then exit |

### Prompt templates

`inputs/` (resolved relative to the working directory at runtime, i.e. `../build-ragbot/inputs/` when launched via `run.sh`):

- `researchPrompt.txt` — researcher system prompt; hot-reloaded each call
- `roleplayPrompt.txt` — `%1` character name, `%2` research answer, `%3` user question
- `characterBackground.txt`, `survivorPrompt.txt` — character context

## Dependencies

**Always:** libcurl, sqlite3, pthreads. nlohmann/json and fmt are fetched and built statically by CMake.

**Embedded inference only:** a pre-built [llama.cpp](https://github.com/ggerganov/llama.cpp) at `~/source/llama.cpp` (binaries in `~/source/build-llama.cpp/`), plus OpenBLAS and Vulkan loader from the system.

## Library build

RAGBot can be linked into a host application as `libragbot.a`. The public surface is a pure C header at `src/ragbot_c_api.h`; full reference and examples live in [`library-api.md`](library-api.md).

## Schema migration

`EmbeddingDatabase::SchemaVersion` in `src/db/EmbeddingDatabase.h` is an integer constant. Bumping it forces tables to be dropped and recreated on next run, triggering a full re-index. Do this whenever the stored chunk format changes.

## Layout

```
ragbot/               source root (this repo)
├── src/              all C++ source
├── inputs/           prompt templates, copied into the runtime CWD
├── tests/            unit tests
├── config.json       runtime config
└── b*.sh, run.sh     build and launch scripts
../build-ragbot/      executable build output (created by CMake)
../build-ragbot-lib/  static library build output
```

See [`CLAUDE.md`](CLAUDE.md) for an architectural deep-dive (pipeline stages, generator interfaces, indexing layer).
