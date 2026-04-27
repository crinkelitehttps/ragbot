# RAGBot Sanity Audit

## Summary

The codebase is small, focused, and mostly internally consistent. Two recent changes — the threaded researcher/roleplayer pipeline and the new `Reranker` stage — exposed a handful of cross-cutting issues that haven't been smoothed out yet. None are critical; the headline ones are: the threading doesn't actually parallelise the work it was meant to parallelise, three of four stdout writers don't use the new `ThreadSafeOutput` mutex, and the `SearchResult::similarity` field is silently re-purposed mid-pipeline.

| Severity | Count |
|----------|-------|
| Medium   | 4     |
| Low      | 8     |
| Critical | 0     |

---

## 1. Conceptual consistency

### 1.1 Threaded pipeline doesn't actually parallelise *(medium)*

`RAGBot::processQuestion` (`src/RAGBot.cpp:67-104`) spawns two `std::thread`s, but the roleplayer's cond-var predicate waits for `researcherFinished`, which is only set *after* `m_researcher.research()` returns (line 70-77). The roleplayer cannot start until the researcher's full answer is produced — functionally equivalent to sequential execution with extra synchronisation overhead.

The original feature intent (item C in `bugs-and-features.md`) was *"Streaming output from Researcher directly to console while Roleplayer waits (pipeline parallelism)"* — but the parallelism part isn't there.

*What to do:* either drop the threading (simpler, same behaviour) or change the wake trigger so the roleplayer can begin prompt construction / template loading while the researcher is still streaming.

### 1.2 Two on/off conventions in the config schema *(low)*

- `enableRoleplay` is a root-level boolean (`src/main.cpp:125`).
- `reranker.enabled` is nested inside the reranker block (`src/asset/Reranker.cpp:10`).

Two stages, two conventions. Pick one — nested is more scalable.

### 1.3 `SearchResult::similarity` field is re-purposed *(medium)*

`Reranker::rerank` (`src/asset/Reranker.cpp:63`) overwrites `SearchResult::similarity` with the rerank score. Downstream, `Researcher::research` (`src/asset/Researcher.cpp:35,38`) prints it labelled `sim=` and embeds it into the LLM context as `similarity:`. After reranking, that value is a cross-encoder rerank score (typically 0..1, different distribution) — not cosine similarity. The label and the number disagree, and the LLM sees a number under a name that no longer describes it.

*What to do:* add a separate `rerankScore` field, or relabel based on which stage last wrote it.

---

## 2. Threading & concurrency

### 2.1 Three of four stdout writers don't use `ThreadSafeOutput` *(medium)*

Only `GeneratorIP::generateText` (`src/generation/GeneratorIP.cpp:157`) was switched to the mutex-guarded helper. The other writers are still raw `QTextStream(stdout)`:

- `src/generation/EmbeddedTextGenerator.cpp:128` (token loop)
- `src/asset/Researcher.cpp:50, 52` (prompt label + trailing newline)
- `src/asset/Roleplayer.cpp:47, 49` (same)

Currently safe only because the cond-var serialises the two stages (see 1.1). The moment the threading is fixed to actually parallelise — the stated goal — output will race. The mutex was added to one site and forgotten at the others.

### 2.2 `QNetworkReply::readyRead` lambda captures by reference *(low)*

`src/generation/GeneratorIP.cpp:154-158`. The slot captures `streamed` and `reply` by reference (`[&]`) and the connection is never disconnected. If `readyRead` fires in the small window between `runLoop()` returning and `reply->deleteLater()` taking effect, the lambda would dereference a stack-allocated `QString`. In practice runLoop returns on `finished`, so the window is tight — but it's a fragile pattern.

*What to do:* explicit `disconnect()` before falling out of the function, or capture `streamed` by value into a `std::shared_ptr<QString>` if the slot needs to outlive the call.

### 2.3 Two threading models in one binary *(low)*

`std::thread` + `std::condition_variable` in `RAGBot.cpp`; `QEventLoop` + `QTimer` in `GeneratorIP`/`RerankGeneratorIP`. Both work, but mixing is unidiomatic in Qt code; `QThread` with signal/slot wakeups would compose more naturally with the rest of the codebase.

---

## 3. Error handling

### 3.1 Silent zero-score path in `RerankGeneratorIP::score` *(medium)*

`src/generation/RerankGeneratorIP.cpp:61-71`. The scores vector is pre-initialised with zeros (line 61). If the response is missing `"results"` or has unparseable shape, the loop produces no overrides and the all-zeros vector returns silently.

`Reranker` downstream only catches *size* mismatch (`src/asset/Reranker.cpp:45`) — an all-zeros same-size response passes the check and produces an arbitrary "top-N" with similarity 0. Compare to `GeneratorIP::parseEmbeddingResponse`, which warns on empty array.

*What to do:* `qWarning()` if the parsed `results` array is empty or shorter than `documents.size()`.

### 3.2 Successful research is dropped on roleplay failure *(low)*

`src/RAGBot.cpp:111-113`. If `m_enableRoleplay && roleplayerFailed`, the function returns before `logConversation()` (line 116) and before the history append. The researcher's answer is lost from the DB and from history despite being valid.

*What to do:* log the conversation (with empty `roleplayAnswer`) before returning, or only short-circuit on researcher failure.

---

## 4. Code smells & readability

### 4.1 Magic config keys scattered across files *(low)*

`"embedder"`, `"reranker"`, `"researcher"`, `"roleplayer"`, `"enableRoleplay"`, `"conversationsDb"` in `main.cpp:79-126`; `"generator"`, `"instruction"`, `"characterName"`, `"modelPath"`, `"backend"`, etc. in each asset constructor and each generator. No central key list, no JSON schema. Renaming a key is a multi-file grep.

*What to do:* a single header (`ConfigKeys.h`) of `inline constexpr const char* kEmbedder = "embedder";` constants — or at minimum, document the schema in `CLAUDE.md`.

### 4.2 `basePath` ↔ `remotePath` silent aliasing *(low)*

`src/generation/GeneratorIP.cpp:32-33` and `src/generation/RerankGeneratorIP.cpp:29-30` both fall back from `basePath` to `remotePath` with no log line. The legacy name isn't mentioned in `CLAUDE.md` or the example config.

*What to do:* pick one canonical key, log a deprecation warning when the other is used, or remove the fallback if no live config still uses it.

### 4.3 `RAGBot::processQuestion` does too much *(low)*

80 lines mixing pipeline logic, thread setup, cond-var plumbing, error checking, history append, and DB write. The threading scaffolding is roughly half the function. Extracting `runResearcherAsync()` / `runRoleplayerAsync()` helpers would let the pipeline shape read top-to-bottom in ~15 lines.

### 4.4 Inconsistent disable-on-failure policy *(low)*

`Reranker` (`src/asset/Reranker.cpp:15-19`) cleanly disables itself if its generator fails to init — `isEnabled()` returns false and the stage is skipped. `Researcher` and `Roleplayer` instead carry an invalid generator and refuse work per call (`Researcher.cpp:23`, `Roleplayer.cpp:23`). Both work; the asymmetry is noise.

*What to do:* pick one. The reranker's "disable yourself" pattern is cleaner — failure is detected once at startup, not on every call.

---

## 5. What's clean

- **`ragbot.pro` registration is consistent.** All four Reranker files are listed (lines 12, 20, 30, 39-40); embedded variants are correctly gated under `embedded_inference {}` (lines 56-63).
- **`GeneratorFactory` handles the missing-embedded build correctly.** Returns `nullptr` with `qCritical` when `backend=embedded` is requested without `RAGBOT_EMBEDDED_INFERENCE` (`GeneratorFactory.cpp:22-24, 47-49, 72-74`). No silent crash.
- **RAII destructors in embedded generators.** `EmbeddedRerankGenerator::~EmbeddedRerankGenerator` (`src/generation/EmbeddedRerankGenerator.cpp:56-60`) frees `m_ctx` and `m_model`; the other embedded generators follow the same pattern.
- **`Reranker` null-safety.** The constructor disables itself if the generator init fails, and `rerank()` is a no-op if disabled or if results are empty — graceful degradation through the whole stage.
- **`isValid()` checks at the entry of every generator method.** `Researcher::research`, `Roleplayer::respond`, `Embedder`, and the embedded/network generators all early-return on an invalid generator. The earlier audit pass missed these — they exist.

---

## Severity table

| #   | Item                                                  | Severity | File:line                                |
|-----|-------------------------------------------------------|----------|------------------------------------------|
| 1.1 | Threaded pipeline doesn't parallelise                 | Medium   | `src/RAGBot.cpp:67-104`                  |
| 1.2 | Inconsistent enable/disable convention                | Low      | `src/main.cpp:125`, `src/asset/Reranker.cpp:10` |
| 1.3 | `similarity` field repurposed for rerank score       | Medium   | `src/asset/Reranker.cpp:63`              |
| 2.1 | Stdout writes bypass `ThreadSafeOutput`               | Medium   | `EmbeddedTextGenerator.cpp:128`, `Researcher.cpp:50,52`, `Roleplayer.cpp:47,49` |
| 2.2 | By-reference lambda capture on `readyRead`            | Low      | `src/generation/GeneratorIP.cpp:154-158` |
| 2.3 | Mixed `std::thread` and Qt event-loop threading        | Low      | `RAGBot.cpp` vs `GeneratorIP.cpp`        |
| 3.1 | Silent zero-score path on malformed rerank response   | Medium   | `src/generation/RerankGeneratorIP.cpp:61-71` |
| 3.2 | Research answer dropped if roleplayer fails           | Low      | `src/RAGBot.cpp:111-113`                 |
| 4.1 | Magic config keys, no central schema                  | Low      | `main.cpp` + every asset constructor     |
| 4.2 | `basePath` / `remotePath` undocumented aliasing       | Low      | `GeneratorIP.cpp:32-33`, `RerankGeneratorIP.cpp:29-30` |
| 4.3 | `processQuestion` mixes pipeline + threading plumbing | Low      | `src/RAGBot.cpp:42-121`                  |
| 4.4 | Inconsistent disable-on-failure policy                | Low      | `Reranker.cpp:15-19` vs `Researcher.cpp:23`, `Roleplayer.cpp:23` |
