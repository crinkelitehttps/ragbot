# RAGBot Sanity Audit

## Summary

**Updated after four follow-up sessions.** All four medium items are resolved. Three low items remain open.

The threading was reverted entirely (commit `b96c80e`). The `readyRead` by-reference capture was fixed with an explicit `disconnect()`. Items 3.2 and 5 were found already resolved in the current code.

| Severity | Open | Resolved |
|----------|------|----------|
| Medium   | 0    | 4        |
| Low      | 3    | 5        |
| Critical | 0    | —        |

---

## 1. Conceptual consistency

### 1.1 Threaded pipeline doesn't actually parallelise *(medium — resolved)*

Threading reverted in commit `b96c80e`. `processQuestion` is now fully sequential; `std::thread`, `std::mutex`, `std::condition_variable`, and the `ThreadSafeOutput` helper have been removed.

### 1.2 Two on/off conventions in the config schema *(low)*

- `enableRoleplay` is a root-level boolean (`src/main.cpp:125`).
- `reranker.enabled` is nested inside the reranker block (`src/asset/Reranker.cpp:10`).

Two stages, two conventions. Pick one — nested is more scalable.

### 1.3 `SearchResult::similarity` field is re-purposed *(medium — resolved)*

Fixed: `EmbeddingDatabase::SearchResult` now has a separate `float rerankScore { -1.0f }` field (negative = not reranked). `Reranker::rerank` sets `rerankScore` instead of overwriting `similarity`. `Researcher::research` uses `relevance:` in the LLM context when reranked, `similarity:` otherwise; the debug line shows both scores.

---

## 2. Threading & concurrency

### 2.1 Three of four stdout writers don't use `ThreadSafeOutput` *(medium — resolved)*

Resolved with 1.1: threading and `ThreadSafeOutput` were removed entirely in commit `b96c80e`. All stdout writes are back to plain `QTextStream(stdout)`.

### 2.2 `QNetworkReply::readyRead` lambda captures by reference *(low — resolved)*

Fixed: `QObject::disconnect(reply, &QNetworkReply::readyRead, nullptr, nullptr)` added before `reply->deleteLater()` in `generateText`. Any queued `readyRead` deliveries after `runLoop()` returns are now suppressed before the stack frame is destroyed.

### 2.3 Two threading models in one binary *(low — resolved)*

Resolved with 1.1: `std::thread` / `std::condition_variable` were removed in commit `b96c80e`. Only the `QEventLoop` + `QTimer` pattern in `GeneratorIP` / `RerankGeneratorIP` remains.

---

## 3. Error handling

### 3.1 Silent zero-score path in `RerankGeneratorIP::score` *(medium — resolved)*

Fixed: `RerankGeneratorIP::score` now calls `qWarning()` when the parsed `results` array is empty (full failure) or shorter than `documents.size()` (partial failure).

### 3.2 Successful research is dropped on roleplay failure *(low — resolved)*

Already fixed in the current `RAGBot.cpp` — the function is sequential and always reaches `logConversation()` regardless of whether roleplay produced output. `roleplayAnswer` is simply an empty string when roleplay is disabled or fails.

---

## 4. Code smells & readability

### 4.1 Magic config keys scattered across files *(low)*

`"embedder"`, `"reranker"`, `"researcher"`, `"roleplayer"`, `"enableRoleplay"`, `"conversationsDb"` in `main.cpp:79-126`; `"generator"`, `"instruction"`, `"characterName"`, `"modelPath"`, `"backend"`, etc. in each asset constructor and each generator. No central key list, no JSON schema. Renaming a key is a multi-file grep.

*What to do:* a single header (`ConfigKeys.h`) of `inline constexpr const char* kEmbedder = "embedder";` constants — or at minimum, document the schema in `CLAUDE.md`.

### 4.2 `basePath` ↔ `remotePath` silent aliasing *(low)*

`src/generation/GeneratorIP.cpp:32-33` and `src/generation/RerankGeneratorIP.cpp:29-30` both fall back from `basePath` to `remotePath` with no log line. The legacy name isn't mentioned in `CLAUDE.md` or the example config.

*What to do:* pick one canonical key, log a deprecation warning when the other is used, or remove the fallback if no live config still uses it.

### 4.3 `RAGBot::processQuestion` does too much *(low — partially resolved)*

Threading removed in `b96c80e`; function is now ~35 lines. Remaining mix of pipeline logic, error checks, history append, and DB write is acceptable at this size.

### 4.4 Inconsistent disable-on-failure policy *(low — resolved)*

Fixed: `Researcher` and `Roleplayer` now reset their generator to `nullptr` in the constructor when init fails, matching the reranker's pattern. Startup warning fires once; the per-call null check is still present but no longer emits redundant warnings.

---

## 5. What's clean

- **`ragbot.pro` registration is consistent.** All four Reranker files are listed (lines 12, 20, 30, 39-40); embedded variants are correctly gated under `embedded_inference {}` (lines 56-63).
- **`GeneratorFactory` handles the missing-embedded build correctly.** Returns `nullptr` with `qCritical` when `backend=embedded` is requested without `RAGBOT_EMBEDDED_INFERENCE` (`GeneratorFactory.cpp:22-24, 47-49, 72-74`). No silent crash.
- **RAII destructors in embedded generators.** `EmbeddedRerankGenerator::~EmbeddedRerankGenerator` (`src/generation/EmbeddedRerankGenerator.cpp:56-60`) frees `m_ctx` and `m_model`; the other embedded generators follow the same pattern.
- **`Reranker` null-safety.** The constructor disables itself if the generator init fails, and `rerank()` is a no-op if disabled or if results are empty — graceful degradation through the whole stage.
- **`isValid()` checks at the entry of every generator method.** `Researcher::research`, `Roleplayer::respond`, `Embedder`, and the embedded/network generators all early-return on an invalid generator. The earlier audit pass missed these — they exist.

---

## Severity table

| #   | Item                                                  | Severity | Status   | File:line                                |
|-----|-------------------------------------------------------|----------|----------|------------------------------------------|
| 1.1 | Threaded pipeline doesn't parallelise                 | Medium   | Resolved | commit `b96c80e`                         |
| 1.2 | Inconsistent enable/disable convention                | Low      | Open     | `src/main.cpp:125`, `src/asset/Reranker.cpp:10` |
| 1.3 | `similarity` field repurposed for rerank score        | Medium   | Resolved | `EmbeddingDatabase.h`, `Reranker.cpp`, `Researcher.cpp` |
| 2.1 | Stdout writes bypass `ThreadSafeOutput`               | Medium   | Resolved | commit `b96c80e`                         |
| 2.2 | By-reference lambda capture on `readyRead`            | Low      | Resolved | `src/generation/GeneratorIP.cpp`         |
| 2.3 | Mixed `std::thread` and Qt event-loop threading       | Low      | Resolved | commit `b96c80e`                         |
| 3.1 | Silent zero-score path on malformed rerank response   | Medium   | Resolved | `src/generation/RerankGeneratorIP.cpp`   |
| 3.2 | Research answer dropped if roleplayer fails           | Low      | Resolved | `src/RAGBot.cpp` (sequential flow)       |
| 4.1 | Magic config keys, no central schema                  | Low      | Open     | `main.cpp` + every asset constructor     |
| 4.2 | `basePath` / `remotePath` undocumented aliasing       | Low      | Open     | `GeneratorIP.cpp:32-33`, `RerankGeneratorIP.cpp:29-30` |
| 4.3 | `processQuestion` mixes pipeline + threading plumbing | Low      | Resolved | commit `b96c80e`                         |
| 4.4 | Inconsistent disable-on-failure policy                | Low      | Resolved | `Researcher.cpp`, `Roleplayer.cpp` constructors      |
