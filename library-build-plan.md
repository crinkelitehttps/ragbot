# Library Build Plan — RAGBot as an embeddable CDDA chat module

## Goal

Produce a build configuration that emits RAGBot as a linkable library (static `.a` and/or shared `.so`) with a stable, Qt-free public API. Cataclysm: Dark Days Ahead (or any host) calls into it to:

1. Pass live game data — NPC stats, traits, faction, location, time, weather, mutations, etc. — as structured input.
2. Ask a question on the player's behalf.
3. Receive a streamed in-character response, token-by-token, suitable for a chat UI.

The host owns the UI; RAGBot owns retrieval, research, and roleplay synthesis.

---

## Constraints worth pinning down up front

These are the things most likely to bite us; surface them in design before code.

- **Qt event loop.** `GeneratorIP` (network backend) uses `QNetworkAccessManager`, which requires a running Qt event loop on the same thread. CDDA is not a Qt application — the library must own a private worker thread that runs its own `QCoreApplication` + event loop. Host calls into the library are dispatched onto that thread.
- **Qt version skew.** The binary currently links against Qt 5.15.2 from `/home/joe/Qt/5.15.2/gcc_64`; the system Qt (5.15.18) crashes at runtime. The library must not export Qt types across the ABI — anything Qt stays internal. Public headers use `const char*`, `size_t`, plain structs, and function-pointer callbacks only.
- **Build modes carry through.** Both `embedded_inference` (llama.cpp in-process) and the network-only build must produce working libraries. The embedded build adds ~hundreds of MB of static libs and a Vulkan dependency — distribution implications belong in this plan, not as a surprise later.
- **stdin REPL must go.** `RAGBot::start()` reads from `stdin` in a loop. The library entrypoint replaces that loop with an externally-driven `ask()` call; `RAGBot::processQuestion()` becomes the per-turn primitive.
- **Asset paths.** `Roleplayer` reads `inputs/roleplayPrompt.txt` relative to CWD; `main.cpp` reads `config.json` similarly. Both must be configurable by the host (CDDA's CWD will not be ours).
- **Indexing cost.** Embedding the CDDA JSON corpus is slow. The library must support pre-built DBs shipped alongside the host, plus a "skip-index" path identical to the current `-s` flag.

---

## Phase 1 — Restructure for two build outputs

Split the project so `main.cpp` is the only thing unique to the executable.

### `ragbot.pro` changes

Introduce a new CONFIG flag, e.g. `CONFIG+=library`. Behaviour:

- Default (no flag): existing console executable, unchanged.
- `CONFIG+=library`: `TEMPLATE = lib`, drop `console`, exclude `src/main.cpp` from `SOURCES`, add `CONFIG+=staticlib` (or `CONFIG+=shared` for `.so`). Output name `libragbot.a` / `libragbot.so`.
- The two flags compose: `qmake CONFIG+="library embedded_inference"` is valid.

A future move to CMake might be cleaner for CDDA-side consumption, but qmake-now / CMake-later is fine — call this out as a known follow-up rather than blocking on it.

### Source rearrangement

No file moves required for Phase 1. Everything in `src/` already compiles into a clean object set; `main.cpp` is the only entry point that needs excluding.

### New build script

Add `b-lib.sh` mirroring `b-clean.sh` but invoking `qmake CONFIG+="library embedded_inference"`.

### Acceptance for Phase 1

- `./b.sh` still produces the existing console binary.
- `./b-lib.sh` produces `libragbot.a` (or `.so`) and a header set in `../build-ragbot/`.
- A trivial test driver (Phase 5) links it and constructs the pipeline.

---

## Phase 2 — Lifecycle API (Qt-internal, C++-only)

Before exposing a C ABI, refactor `main.cpp`'s setup logic into a reusable session class. This is the natural extraction point and keeps the existing executable working.

### New class: `RAGBotSession` (`src/RAGBotSession.h` / `.cpp`)

Responsibilities:

- Owns a private `QThread` running a `QCoreApplication` (created only if `QCoreApplication::instance()` is null — host might already have one).
- Owns the `Embedder`, `Reranker`, `Researcher`, `Roleplayer`, `RoleplayDatabase` instances.
- Exposes `ask(question, callback)` that marshals onto the worker thread, runs the existing `processQuestion()` logic, and emits tokens via callback.
- Holds the conversation history (`QVector<ConversationTurn>`), as `RAGBot` does today.

Construction takes a `QJsonObject` config (same shape as `config.json`), so the existing config schema stays authoritative. The C ABI in Phase 3 simply parses a JSON string into a `QJsonObject` before handing it over.

### Refactor `RAGBot` itself

`RAGBot::start()` (the stdin loop) is executable-only. Move it into `main.cpp` as a free function `runConsoleLoop(RAGBotSession&)`, or keep it on `RAGBot` and have the executable construct `RAGBot` from a `RAGBotSession`. Either works; the goal is that `RAGBotSession` has no `stdin` dependency.

`RAGBot::processQuestion()` becomes a method on `RAGBotSession` (or `RAGBot` keeps it and `RAGBotSession` owns a `RAGBot`). Pick whichever requires less churn.

### Streaming hook

`TextGenerator::generateText()` currently prints to stdout when `stream=true` (see `Roleplayer::respond()` writing `"\n%1: "` then calling `generateText` with `stream=true`). For library use, tokens must instead be delivered to the host via callback.

Two options, pick one:

- **A.** Extend `TextGenerator` with an optional `std::function<void(QStringView)>` token sink. When set, tokens go to the sink instead of `stdout`. Roleplayer/Researcher pass through the sink the session was given.
- **B.** Capture stdout via a redirector. Hacky; reject unless A turns out to be invasive.

Plan assumes A.

### Asset path resolution

Add `assetsDir` to the config schema (or pass via `RAGBotSession` constructor). `Roleplayer` builds `assetsDir + "/roleplayPrompt.txt"` instead of the bare relative path. Default to `"inputs"` for the existing executable so nothing breaks.

### Acceptance for Phase 2

- Console binary still works exactly as before.
- A C++ test (Phase 5) instantiates `RAGBotSession`, calls `ask("hello")`, receives tokens via callback, and shuts down cleanly.

---

## Phase 3 — C ABI wrapper

This is what CDDA actually links against. The wrapper is a thin file (`src/ragbot_c_api.h` + `src/ragbot_c_api.cpp`) — no logic, just translation.

### Header (`ragbot_c_api.h`)

`extern "C"` block, opaque handle, plain types only. Sketch of the surface:

```c
typedef struct ragbot_session ragbot_session;

typedef void (*ragbot_token_cb)(const char* token, size_t len, void* user);
typedef void (*ragbot_done_cb)(const char* full_answer, void* user);
typedef void (*ragbot_log_cb)(int level, const char* msg, void* user);

ragbot_session* ragbot_create(const char* config_json, const char* assets_dir);
void            ragbot_destroy(ragbot_session*);

// Push live game data. Replaces, not appends. JSON shape defined below.
int  ragbot_set_npc_context(ragbot_session*, const char* npc_json);
int  ragbot_set_world_context(ragbot_session*, const char* world_json);

// Async; returns immediately. Tokens stream via on_token, completion via on_done.
int  ragbot_ask(ragbot_session*, const char* question,
                ragbot_token_cb on_token, ragbot_done_cb on_done, void* user);

// Optional: blocking variant for hosts that prefer it.
int  ragbot_ask_blocking(ragbot_session*, const char* question,
                         char* out_buf, size_t out_buf_size);

void ragbot_set_log_callback(ragbot_log_cb, void* user);
```

Return codes are `int` (`0` ok, negative on failure). Strings are UTF-8.

### NPC / world context schema

Open question: what do we want CDDA to send? Initial cut, conservative:

- `npc`: `name`, `profession`, `faction`, `traits[]`, `mutations[]`, `skills{}`, `health_state`, `mood`, `relationship_to_player`.
- `world`: `time_of_day`, `weather`, `location_summary`, `nearby_entities[]`, `current_threat_level`.

These map into:

- **Roleplayer:** the NPC's name and background dynamically replace the static config values for the duration of the session — i.e. the chat partner *is* the in-game NPC, not the generic Survivor.
- **Researcher:** an extra `Live game state:` block in the prompt above the retrieved chunks, so factual answers acknowledge in-fiction context (e.g. "given that it is night and raining…").

Worth deciding before coding: do we want `set_npc_context` to be sticky for the whole session, or pushed per-question? Per-question is more honest (NPCs walk away, mood changes) but multiplies prompt rebuild cost. Recommendation: sticky, with a fast "update only these fields" path later if needed.

### Threading rules in the C API

- `ragbot_create` may be called from any thread; spawns the worker.
- `ragbot_ask` is non-blocking, returns immediately. Token / done callbacks fire **on the worker thread** — host must marshal back to its own UI thread itself. Document this loudly.
- `ragbot_destroy` joins the worker. Calling it concurrently with an in-flight `ask` cancels the ask.

### Logging

Replace the project's `qInstallMessageHandler` default with one that routes through `ragbot_log_cb` if set. Falls back to stderr if not set, so the standalone executable behaviour is unchanged.

### Acceptance for Phase 3

- A C program (not C++) `#include`s `ragbot_c_api.h`, links against `libragbot.so`, and runs an end-to-end ask.
- Headers contain zero Qt symbols. Verify with `grep -r "Q[A-Z]" include/`.

---

## Phase 4 — CDDA-side integration sketch

Not part of this repo's code, but the plan should know what the consumer looks like.

- CDDA's CMake adds `find_package` (or a hand-rolled find module) for RAGBot, links `libragbot.so`, includes `ragbot_c_api.h`.
- A new in-game UI panel: chat window bound to a specific NPC. On open: `ragbot_create` + `ragbot_set_npc_context` from that NPC's data.
- On player input: `ragbot_ask`. Token callback appends to the chat log; done callback finalises the message and re-enables input.
- On NPC dismissal / save: `ragbot_destroy`.
- Distribution: ship `embeddings.db` (pre-built) and a default `config.json` alongside CDDA, plus the GGUF model if `embedded_inference` is on, or a documented network endpoint if not.

Open question worth raising with the CDDA side: is shared-library distribution acceptable to that project's licensing and build culture, or do they want a static archive merged into the main binary? Affects Phase 1's output format.

---

## Phase 5 — Test harness

A standalone driver, `tests/lib_smoke.cpp` (new dir), builds *only* against the public C ABI — same way CDDA will. Lives outside `ragbot.pro`; has its own tiny qmake/CMake file. Steps:

1. Reads `config.json`, calls `ragbot_create`.
2. Sends a hardcoded NPC context blob.
3. Asks two questions, prints streamed tokens.
4. Destroys the session.

This is the canary that proves the library is genuinely consumable without Qt headers leaking. CI can run it; when it breaks, the ABI broke.

---

## Phase 6 — Documentation

- `library-api.md` describing the C API, callback threading rules, and JSON schemas for NPC / world context.
- Update `CLAUDE.md` with a `## Library build` section: how to build, link, and call.
- Note the Qt 5.15.2 hard requirement at link time — host project must use a compatible Qt or arrange ABI isolation.

---

## Sequencing and effort estimate

Rough order, each step independently mergeable:

1. Phase 1 (build split) — small, mostly `.pro` edits.
2. Phase 2 (`RAGBotSession` extraction + streaming hook + asset paths) — the bulk of the C++ work.
3. Phase 3 (C ABI wrapper) — small, mechanical, but the design of the NPC/world JSON shape is worth a pre-discussion.
4. Phase 5 (smoke test) — pull this earlier if Phase 3 feels speculative; the test forces the API to be honest.
5. Phase 4 (CDDA integration) — out of scope for this repo, but a stub PR / fork demonstrating linkage is the real proof.
6. Phase 6 (docs) — last.

---

## Open questions to resolve before coding

1. Static archive vs shared object for the CDDA side?
2. NPC context: sticky-for-session or pushed-per-question?
3. JSON schema for NPC and world state — needs CDDA-side input on what fields are cheap to emit.
4. Does the host want a sync `ask_blocking` API at all, or is async-only fine?
5. Where does the embeddings DB live in a CDDA install, and who runs the indexer — a one-shot CLI tool we ship, or a `ragbot_index()` API the host calls on first run?
6. Licensing compatibility (RAGBot's deps: Qt LGPL, llama.cpp MIT, GGUF model licences) vs CDDA's CC-BY-SA 3.0 + custom — needs confirmation that linkage is allowed in both directions.
