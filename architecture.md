# RAGBot — Architectural Assessment & Re-Architecture

*A bold-redesign report. Imagines rebuilding from scratch while preserving every feature.*

---

## 1. Executive summary

RAGBot today is a single-process, single-threaded, Qt5 C++ console app whose orchestrator (`src/RAGBot.cpp`) hard-wires four pipeline stages, with parallel embedded/network class hierarchies under `src/generation/` that duplicate ~30% of their plumbing, and a hand-rolled in-memory vector index that loads every embedding into RAM at startup and brute-force scans them per query. It works. It is also one corpus-size jump (low six figures) from collapsing under its own memory footprint, one config typo from a 90-minute reindex, and one Qt point-release from a runtime crash. The features it has are good and worth keeping; the structure that delivers them is not.

The single largest architectural lever — the one change that unlocks most of the others — is **splitting indexing from serving into independent processes that share an on-disk vector store**. Once that boundary exists, the chat loop stops paying for indexing concerns, the index can be regenerated without taking the chat down, the vector store can be swapped for an ANN/quantized library without touching the pipeline, and every stage becomes independently testable. Everything in §4 is downstream of that one cut.

---

## 2. What RAGBot actually does today

### Features that must be preserved

- Three-stage pipeline: **retrieve → research → roleplay** (`RAGBot.cpp:39-73`).
- Optional **rerank** stage between retrieve and research (`RAGBot.cpp:52-53`, `Reranker::isEnabled`).
- Per-stage **embedded *or* network backend** selection via `config.json["...generator"]["backend"]` (`GeneratorFactory.cpp`).
- **CDDA `copy-from` resolution** with multiple-inheritance merging up to depth 16 (`CDDAResolver.cpp:38`).
- **Man-page parser** as an alternative source kind (`ManPageResolver`, selected via `parserType: "man_page"`).
- **Incremental re-index** keyed by per-file SHA-256 (`Embedder.cpp:115-119`); unchanged files are skipped.
- **Conversation logging** to a separate SQLite database (`RoleplayDatabase`).
- **History-aware prompts**: the last `MaxHistoryTurns = 2` turns are passed to Researcher and Roleplayer (`RAGBot.h:14`, `RAGBot.cpp:70-72`).
- **Streaming output** to stdout for the Researcher answer.
- **CLI overrides** for config path, data dir, db path, plus skip-index and load-only modes (`main.cpp` argument parsing).

### Features the system *does not* have, and shouldn't

No GUI. One user. One machine. One concurrent request. No auth. No multi-tenant anything. The redesign preserves these constraints — they make a lot of things simpler and a few things possible (e.g. SQLite as the only datastore).

---

## 3. Honest assessment of the current architecture

Grouped by theme, with file:line citations. Severity ordering is roughly highest-impact first.

### 3.1 The pipeline is a god-object with hard-wired stages

`RAGBot::processQuestion` (`src/RAGBot.cpp:39-73`) *is* the orchestration. Stages are concrete classes (`Embedder`, `Reranker`, `Researcher`, `Roleplayer`) held by reference in `RAGBot.h:25-29`, constructed by name in `main.cpp`, and called in a fixed order. To add a stage, swap one out, A/B two researchers, or run two pipelines in parallel, you edit `RAGBot.cpp` and recompile. The reranker enable/disable lives as an `if` (`RAGBot.cpp:52-53`); the roleplay enable/disable lives as another `if` (`RAGBot.cpp:63`). There is no Stage interface, no Pipeline type, no composition.

### 3.2 Generation backends duplicate plumbing instead of sharing it

`src/generation/` is ~875 LoC and the duplication between embedded and network variants is structural, not incidental:

- The QEventLoop+QTimer blocking-HTTP pattern is **identical** in `GeneratorIP.cpp:13-24` and `RerankGeneratorIP.cpp:12-24`.
- The llama.cpp backend bring-up is **identical** in `EmbeddedTextGenerator.cpp:7-14` and `EmbeddedRerankGenerator.cpp:6-13`.
- Config parsing (`basePath`/`remotePath`/`modelName`/`timeout`) is duplicated between `GeneratorIP::GeneratorIP` and `RerankGeneratorIP::RerankGeneratorIP`.
- `DefaultTimeout` is `240000ms` in `GeneratorIP` and `60000ms` in `RerankGeneratorIP` for no documented reason.
- `DefaultCtx`, `DefaultBatch`, `DefaultTemp` are redeclared per header rather than shared.

The shape implies "the embedded/network distinction is the dominant axis of variation." It isn't. The dominant axes are *embedding vs text vs rerank* (request/response shape) and *blocking vs streaming* (transport). The current taxonomy gets it wrong and pays for that with copy-paste.

### 3.3 Synchronous everything, on one thread

`RAGBot::start()` reads stdin synchronously (`RAGBot.cpp:24-26`). Every downstream call (`Embedder::search`, `Reranker::rerank`, `Researcher::research`, `Roleplayer::respond`) blocks. A misconfigured remote inference server can stall the REPL for 240 seconds (`GeneratorIP` default timeout) with no way to cancel short of SIGINT. Streaming is `QTextStream(stdout) << chunk << Qt::flush` per token (`GeneratorIP.cpp:152-158`, `EmbeddedTextGenerator.cpp:128`) — fine for a single user, but it tightly couples generation progress to terminal I/O and gives the Roleplayer no way to consume the Researcher's stream incrementally even though feature C in `bugs-and-features.md` is checked off as "streaming output" — what actually exists is *output* streaming, not *pipeline* streaming.

### 3.4 `VectorIndex` is the load-bearing fragility

`VectorIndex` is `QVector<QVector<float>>` plus brute-force cosine similarity with `std::partial_sort` (`VectorIndex.cpp:50-82`). Embeddings are 768-dim float32, so each vector is ~3 KB; 100k chunks ≈ 300 MB of resident memory; 1M chunks ≈ 3 GB. On startup, every embedding is `memcpy`'d out of its SQLite BLOB (`EmbeddingDatabase.cpp:143`) into the in-memory vector. There is no quantization (int8 would cut memory 4×; binary would cut it 32×), no ANN structure (search latency is O(N·D) per query), no memory-mapped backing, no shard. This is the single largest scalability ceiling in the system, and it is invisible until the day it isn't.

### 3.5 Schema versioning is a nuke button

`EmbeddingDatabase::SchemaVersion = 5` (`EmbeddingDatabase.h:77`). On a mismatch, all tables are dropped and recreated (`EmbeddingDatabase.cpp:80-94`). There is no migration path. Any change to the chunk format — adding a column, switching embedding model, changing flatten rules — requires reindexing the entire corpus from scratch. For a CDDA-sized corpus on an embedded model, that is a multi-hour wait per format change. The cost is so high that the system actively discourages the kind of iteration on chunk format you'd actually want to do.

### 3.6 Config is stringly-typed and validated lazily

Config keys (`embedder`, `reranker`, `reranker.generator.modelPath`, `enableRoleplay`, `parserType`, etc.) are looked up by literal string at the call site, scattered across `main.cpp`, `GeneratorFactory.cpp`, and the asset constructors. CLI flags mutate the loaded `QJsonObject` directly (`main.cpp:78-92`) rather than going through a typed setter. There is no schema, no defaults table, no validation pass. If `reranker.enabled` is true but `reranker.generator.modelPath` is empty, you find out partway through pipeline construction. If `enableRoleplay` is placed under `researcher.generator` instead of at the root (a real bug fixed in commit `2f9186d`), you find out by reading `main.cpp` source.

### 3.7 Rollback semantics are inconsistent

`Embedder::processFile` wraps each file in a SQLite SAVEPOINT *and* takes an in-memory snapshot of `VectorIndex` (`m_txIndexSnapshot`) so it can roll back both on error. These are two systems pretending to be one transaction. If the SQLite commit succeeds and the in-memory snapshot restore fails (or is skipped on an unexpected exit), the on-disk and in-memory state diverge. There is no joint commit primitive, and no recovery code to detect divergence at startup.

### 3.8 The build is a personal machine

`ragbot.pro:67-72` hard-codes `/home/joe/source/build-llama.cpp/...` as `LIBS` paths. `ragbot.pro:76-83` hard-codes `/home/joe/Qt/5.15.2/gcc_64`. The CLAUDE.md notes that system Qt 5.15.18 causes a runtime crash and must not be used. There is no CMake, no package manager, no `FetchContent` equivalent, no Dockerfile, no CI configuration anywhere. There are no tests of any kind in the repo. The build works exactly on one machine.

### 3.9 No observability

`qDebug()` to stderr is the entire telemetry surface. There is no structured logging, no per-stage timing, no token-throughput counter, no retrieval-recall measurement, no eval harness. Swapping the embedding model or the rerank model is a vibes-based decision because there is no way to measure whether it helped.

### 3.10 Qt5 is the wrong dependency for what's used

The Qt surface used is essentially: `QString`, `QJsonObject`, `QVector`, `QNetworkAccessManager`, `QFile`, `QDirIterator`, `QEventLoop`, `QTimer`, `qDebug`. None of this requires Qt. `std::string`/`std::filesystem`/`nlohmann::json`/`std::vector`/`cpp-httplib`/`spdlog` cover the lot. In exchange, the project pays Qt's pinning cost (the 5.15.2 vs 5.15.18 incompatibility documented in `CLAUDE.md`), the `app_bundle`/event-loop machinery it never uses, and a multi-hundred-megabyte build dependency. This isn't a "Qt is bad" claim — it's a claim that the *parts of Qt this program uses* are fully replaceable without losing anything.

---

## 4. The redesign

Each subsection follows a fixed shape: **What it is → Why → Cost vs current → What is preserved.**

### 4.1 Process topology: split *indexer* from *server*

**What.** Two separable entry points (subcommands of one binary, or two binaries — same shape):
- `ragbot index` — batch job. Walks the corpus, parses, embeds, writes to a vector store on disk. Exits.
- `ragbot serve` — long-running daemon. Loads the vector store read-only, accepts queries (REST or local Unix socket), returns answers.
- `ragbot chat` — thin client REPL that talks to `serve` (or runs in-process when desired).

**Why.** Indexing and serving have different resource shapes (indexing is GPU/embedding-bound and writes; serving is GPU/inference-bound and reads). They have different failure modes (a bad parser shouldn't break chat; a hung generation shouldn't stall indexing). And they have different lifecycles (you reindex on corpus change; you keep chat running for days).

**Cost vs current.** New IPC surface. New start/stop story. Logs are now in two places.

**Preserved.** The single-machine, single-user, single-file-deployment shape is intact. A user can still run all three subcommands sequentially in one shell session and never know there's a daemon.

### 4.2 Pipeline as a typed DAG of stages

**What.** Define `Stage<In, Out>` as the unit of pipeline composition. Concrete stages: `Retrieve`, `Rerank`, `Research`, `Roleplay`, `LogTurn`. A `Pipeline` is a list (or DAG) of stages built from config:

```
pipeline:
  - retrieve:    { source: vector_store, top_k: 20 }
  - rerank:      { backend: embedded, top_n: 5, enabled: true }
  - research:    { backend: network, model: qwen3.5-9b, stream: true }
  - roleplay:    { enabled: true, character: survivor }
  - log_turn:    { db: conversations.db }
```

**Why.** Adding a stage = a class + a config line, no orchestrator edits. Disabling a stage = `enabled: false` and the builder skips it. Mock stages become trivial, which makes real unit tests possible. The reranker enable/disable and roleplayer enable/disable `if`s in `RAGBot.cpp` evaporate because they were always config-driven graph construction in disguise.

**Cost vs current.** A small amount of generic-programming machinery (or `std::variant`-based stage results), and the discipline to make stages truly stateless between calls.

**Preserved.** The exact stage sequence and every prompt are unchanged. The history-window logic is one stage (`InjectHistory`) sitting before Researcher/Roleplayer.

### 4.3 Generator layer: one HTTP client, one local runtime, content-typed adapters

**What.** Replace the 5-class hierarchy with two pieces:
- `OpenAICompatibleClient` — a single HTTP/SSE client that exposes `embed(req)`, `chat(req)`, `rerank(req)`. Three request builders, one transport.
- `LlamaContext` — a single RAII wrapper that owns model, context, sampler chain, and batch lifecycles. Three task adapters (`embed`, `chat`, `rerank`) sit on top — they configure pooling, prompt formatting, and decode loop, sharing the underlying llama.cpp objects.

The "embedded vs network" choice becomes a strategy attached to the stage:
```cpp
auto researcher = make_stage<Research>(config, backend == "embedded" ? llama_chat : openai_chat);
```
not a parallel class hierarchy.

**Why.** The current taxonomy treats embedded/network as the dominant axis; the actual dominant axis is request shape (embed/chat/rerank). Re-cutting the abstractions along that axis kills the three duplicated llama init paths, the two duplicated HTTP loops, and the two diverging timeout defaults.

**Cost vs current.** ~30% LoC reduction in `src/generation/` but a careful one-shot rewrite — the embedded path manipulates llama.cpp objects in three subtly different ways and the consolidation has to preserve all three.

**Preserved.** Per-stage backend selection in config. ChatML formatting. Streaming. Cohere-style rerank request shape (already OpenAI-adjacent).

### 4.4 Async by default

**What.** Stages return `std::future<Out>` (or a coroutine `task<Out>` in C++20). The pipeline driver `co_await`s each stage. The REPL is non-blocking; in-flight generations are cancellable via a `stop_token` that propagates into the HTTP client (close the connection) and the llama context (set the abort callback).

**Why.** A 240-second timeout currently means a 240-second frozen REPL with no way to cancel. Async + cancellation makes Ctrl-C interrupt the *current stage*, not the *current process*. It also opens the door to genuine pipeline streaming — the Roleplayer can begin formatting tokens as they leave the Researcher, instead of waiting for the full research answer.

**Cost vs current.** C++20 coroutine machinery has a learning curve; alternative is `std::future` + a small executor.

**Preserved.** Single-user shape — there is no thread pool, no concurrent-request handling needed. Async here is for cancellability and incremental streaming, not throughput.

### 4.5 Vector store: pluggable, on-disk, ANN-capable

**What.** Define `VectorStore` as an interface (`add`, `search`, `delete`, `commit`). Provide one concrete implementation backed by **`sqlite-vec`** (the SQLite extension for vector search) or **`usearch`** (single-header C++, on-disk, supports int8/binary quantization). The choice between them is a runtime config option; both keep the single-file deployment property.

**Why.** Sub-linear search (HNSW or IVF), int8 quantization for ~4× memory reduction, no "load every vector into RAM at startup" failure mode. `sqlite-vec` lets the metadata table and the vector index live in the same `.db` file, which preserves what the current SQLite-based design does well.

**Cost vs current.** Adding one of these as a vendored single-header dependency (or, for sqlite-vec, an extension load at runtime). A one-time migration job to convert the existing BLOB-stored vectors into the new index.

**Preserved.** SQLite metadata (sources, chunks, content). Single-file shipping. Schema-lock semantics (the new tables get migration entries).

### 4.6 Schema migrations, not schema nuking

**What.** Replace `SchemaVersion = 5` with a directory of migration files: `migrations/0001_init.sql`, `migrations/0002_add_source_kind.sql`, etc. On startup, query `PRAGMA user_version`, run all migrations newer than that, set `user_version` to the latest. Embedding-format changes get a `chunks_v{n}` table and a backfill job that reindexes only the changed-format chunks; old chunks remain queryable.

**Why.** Format changes today cost a multi-hour reindex. With migrations they cost a config edit and a backfill that runs in the background. This is what enables genuine iteration on the chunking and embedding strategy.

**Cost vs current.** Migration discipline. A small migration runner (~100 LoC) or a vendored library (e.g. `goose`-style for C/C++ — there are several).

**Preserved.** SQLite as the metadata store. Drop-and-recreate is still available for genuinely incompatible changes — but it's one specific migration, not the default.

### 4.7 Typed configuration with a real schema

**What.** Define a `Config` struct (POD, with named fields and defaults). Parse JSON once at startup into the struct using a library like `nlohmann::json`'s `from_json` adapters or `json-schema-validator`. Validate fully *before* any stage is constructed. CLI flags map to typed setters on the struct, not `QJsonObject` mutation.

```cpp
struct RerankConfig {
    bool enabled = false;
    int top_n = 5;
    GeneratorConfig generator;
};
```

Misconfiguration produces a usable error like:
```
config error: reranker.enabled = true but reranker.generator.modelPath is empty
            : run with --reranker.enabled=false to disable reranking
```
not a deep null-pointer or a silent fallback.

**Why.** The `enableRoleplay`-placement bug (commit `2f9186d`) was a direct consequence of stringly-typed lookups against a free-form JSON tree. A real schema would have rejected the misplacement at startup with a clear error.

**Cost vs current.** A schema definition and one parser pass. Boilerplate that mostly writes itself.

**Preserved.** Config file format is still JSON. CLI flag set is still `-c -d -b -s -l`. Backwards-compatibility migration at parse time can accept the old-schema files for one release.

### 4.8 Drop Qt; CMake the build

**What.** Replace Qt with a standard-library + small-vendored-libs stack:

| Qt today                            | Replacement                                      |
| ----------------------------------- | ------------------------------------------------ |
| `QString`, `QStringList`            | `std::string`, `std::vector<std::string>`        |
| `QJsonObject`, `QJsonDocument`      | `nlohmann::json`                                 |
| `QVector`                           | `std::vector`                                    |
| `QNetworkAccessManager`             | `cpp-httplib` (header-only, supports SSE)        |
| `QSqlDatabase`                      | `sqlite3.h` (already a dep)                      |
| `QFile`, `QDirIterator`             | `std::filesystem`                                |
| `QEventLoop`/`QTimer`               | gone (we're async now)                           |
| `qDebug`                            | `spdlog` (structured logging)                    |

Build switches to **CMake** with `FetchContent` for the small libraries; Qt and qmake go away entirely.

**Why.** Removes the Qt 5.15.2-specific runtime-crash trap. Removes the hardcoded `/home/joe/Qt/...` path from the build. Makes the project buildable on any machine with a C++20 compiler. Reduces binary size and link time. None of the Qt features actually used (HTTP, JSON, file walk, string handling, console I/O) need a UI framework underneath them.

**Cost vs current.** A real rewrite of every file's includes and a careful pass over string-encoding (Qt is UTF-16 internally; `std::string` is byte-oriented). Several days of work, not weeks, given the modest size of the codebase.

**Preserved.** Behavior is identical. Configuration is identical. Output is identical. The user-facing surface doesn't change.

*Alternative considered:* port to Qt6. Rejected — same effort, doesn't address the "we don't use what Qt provides" problem, and keeps the heavyweight dependency.

### 4.9 Observability

**What.** Three additions:

1. **Structured logs.** Per-stage entry/exit JSON events with `stage`, `duration_ms`, `input_size`, `output_size`, `model`, `tokens_in`, `tokens_out`. Goes to stderr; pipes cleanly into `jq` or any log shipper.
2. **In-process counters.** `queries_served`, `retrieval_top_k_distribution`, `rerank_rejection_rate`, `tokens_per_second` per backend. Exposed at `/metrics` on the serve daemon (Prometheus text format — trivial to implement, free dashboard via Grafana if anyone cares).
3. **Eval mode.** `ragbot eval --corpus eval/cdda_questions.jsonl` runs a fixed set of question/expected-context pairs and prints recall@k, MRR, and answer cosine similarity against a reference. Run this on every model swap; the question "did this change help?" gets an answer.

**Why.** Today there is no way to tell whether a change to the embedding model, the rerank model, or the chunk flatten format helped or hurt retrieval. Without an eval harness, every change is faith-based.

**Cost vs current.** Modest. `spdlog` for structured logs (one library), a counter map (one file), a small eval driver (one binary).

**Preserved.** Default verbosity matches the current `qDebug` chatter. Logs can be silenced with `--log-level=warn`.

### 4.10 Tests and CI

**What.** A `tests/` directory with:

- **Parser fixtures.** A directory of input JSON → expected `Chunk` (`embedText` and `content`). Run on every commit. Catches regressions in `extractTextRecursive`.
- **CDDAResolver fixtures.** Synthetic copy-from chains (single, multi-level, cyclic, missing parent) → expected resolved object. Catches the bug 2 (relative/proportional across multiple inheritance) class of problem at PR time, not in production.
- **VectorStore golden tests.** Known query embedding → expected top-K against a fixed seeded corpus. Catches search regressions on backend swaps.
- **End-to-end smoke.** Stub `EmbeddingGenerator` and `TextGenerator` that return canned outputs. Drives the full Pipeline. Catches wiring regressions.
- **Generator contract tests.** A shared test suite that *every* `EmbeddingGenerator`/`TextGenerator`/`RerankGenerator` implementation must pass — keeps embedded and network behaviorally interchangeable.

CI: GitHub Actions running the test suite on Linux + macOS, plus `clang-tidy` on the diff. Pre-commit hook runs the fast tests locally.

**Why.** There are *zero* tests today. This is the highest-ROI change in the entire report. Every other change in §4 is safer to do once tests exist.

**Cost vs current.** Bootstrapping cost: a test framework choice (`Catch2` is a single header, fits the rest of the stack), fixture corpora, a CI config. Maybe two days. Then ongoing test-writing discipline.

**Preserved.** Everything. Tests are additive.

---

## 5. Strawman target architecture

```
                            Corpus (filesystem)
                                    │
                                    ▼
                ┌───────────────────────────────────────┐
                │           ragbot index                │
                │                                       │
                │  Walker → Parser → Embedder           │  (process boundary)
                │                       │               │
                │                       ▼               │
                │                  VectorStore          │
                │                  Writer (sqlite-vec)  │
                └───────────────────────┬───────────────┘
                                        │
                                        ▼
                               ┌──────────────────┐
                               │  data.db         │   ← single file:
                               │  (metadata +     │     metadata, chunks,
                               │   vector index)  │     vector index
                               └──────────────────┘
                                        │
                                        │ read-only mmap
                                        ▼
                ┌───────────────────────────────────────┐
                │           ragbot serve                │
                │                                       │
                │  Pipeline DAG:                        │
                │    Retrieve ─► Rerank ─► Research     │  (process boundary)
                │                            │          │
                │                            ▼          │
                │                         Roleplay      │
                │                            │          │
                │                            ▼          │
                │                         LogTurn ──► conversations.db
                │                                       │
                │  Generator backends:                  │
                │    OpenAICompatibleClient (HTTP)      │
                │    LlamaContext (in-process)          │
                │                                       │
                │  Endpoints: POST /query  GET /metrics │
                └───────────────────┬───────────────────┘
                                    │
                                    │ Unix socket / HTTP
                                    ▼
                          ┌──────────────────┐
                          │  ragbot chat     │  ← thin REPL, can
                          │  (REPL client)   │     also be a TUI/web
                          └──────────────────┘
```

Process boundaries are explicit. Everything inside a box is in-process and uses the in-process interfaces (Stage, Generator, VectorStore). Everything across boxes is a file or a socket.

---

## 6. Migration path (sketch only)

Three milestones, each independently shippable:

1. **Cover existing behavior with tests** (§4.10). No production change. Builds the safety net.
2. **Replace `VectorIndex`** with the `VectorStore` interface and one ANN-backed implementation (§4.5). Move `EmbeddingDatabase` to migration-driven schema (§4.6). Still single-process, still Qt.
3. **Split indexer from server** (§4.1). Define the Stage/Pipeline abstraction (§4.2). Collapse the generator hierarchy (§4.3). Drop Qt and switch to CMake (§4.8). Add observability (§4.9) and async (§4.4).

Milestone 1 is days. Milestone 2 is a week. Milestone 3 is the real rewrite — two to four weeks of focused work.

---

## 7. What I'd keep exactly as-is

- **`CDDAResolver` semantics.** The copy-from chain merging with `relative`/`proportional` post-pass is genuinely tricky domain logic and the current implementation works. Port it as-is, add fixture tests, don't redesign.
- **ChatML prompt format and the `inputs/` template files.** They encode product judgment, not engineering decisions.
- **Conversation history bounded at small N.** `MaxHistoryTurns = 2` is a fine default; promoting it to a config field is sufficient. Don't add unbounded memory.
- **SQLite as the only datastore.** Single-file portability is a genuine feature for this app's deployment shape.
- **The CLI shape.** `-c -d -b -s -l` are short, useful, and people have muscle memory for them.

---

## 8. What I'd explicitly *not* do

Naming the seductive bad ideas so they stay rejected:

- **No microservices.** There is one user. One.
- **No Kubernetes, no message queue, no service mesh.** Same reason.
- **No rewrite in Python "because it's the LLM language".** The embedded llama.cpp integration is a real asset; rewriting it would lose performance and add a runtime. The C++ choice is correct here.
- **No replacing SQLite with Postgres.** Single-file deployment is a feature, not a limitation.
- **No GraphQL.** The query is "ask a question, get an answer." That's a POST.
- **No vector database service** (Pinecone, Weaviate, Qdrant-as-a-server). On-process or on-disk libraries (sqlite-vec, usearch) cover the use case without adding a network hop, an account, or a bill.
- **No agent framework.** The pipeline is a fixed DAG with config-toggleable stages. That's not what agent frameworks solve. Adding one would be cargo-culting.
- **No vendor SDK lock-in.** `OpenAICompatibleClient` is the abstraction; Anthropic, Cohere, Together, vLLM, llama-server, and a dozen others speak it. Don't bind to one provider.

---

## 9. The headline, restated

The features are good. The structure is one corpus-size jump from collapse and one schema change from a half-day reindex. The single change worth making, if no others, is **§4.10 (tests)** — because every other change becomes possible once it exists. The single change with the largest leverage on the system's shape is **§4.1 (split indexer from server)** — because it forces every other boundary into the open. Everything else in §4 is downstream of those two cuts.
