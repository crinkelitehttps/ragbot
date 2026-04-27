# Bugs and Feature Ideas

## Bugs

- [x] 1. Double directory scan during indexing — `processAllFiles()` runs `QDirIterator` twice (count + process); fix by computing total from `paths.size()` before the loop (already partially fixed — verify no second scan remains)
- [x] 2. `CDDAResolver::mergeObjects()` may handle `relative`/`proportional` incorrectly when chained across multiple inheritance levels — the separate post-pass processes deltas against the base object, not the already-merged result
- [x] 3. `SearchResult::similarity` silently overwritten by `Reranker::rerank()` with the cross-encoder score — `Researcher` then labels it `similarity:` in the LLM context, which is wrong. Fixed by adding a separate `rerankScore` field (negative = not reranked); `Researcher` now uses `relevance:` label when reranked.
- [x] 4. `RerankGeneratorIP::score()` pre-fills scores with zeros and returns silently on a malformed or empty `results` array — the caller cannot distinguish a real zero score from an HTTP/parse failure. Fixed by adding `qWarning()` when the results array is empty or shorter than the document count.

- [x] 5. `QNetworkReply::readyRead` lambda captures `streamed` and `reply` by reference (`[&]`) in `src/generation/GeneratorIP.cpp:154-158`. If `readyRead` fires after `runLoop()` returns (before `deleteLater` takes effect), the lambda dereferences a stack variable. Fixed: explicit `QObject::disconnect()` call before `reply->deleteLater()`.
- [x] 6. Research answer dropped on roleplay failure — already fixed in `RAGBot.cpp` (sequential flow always reaches `logConversation()`).

## Feature Ideas

- [x] A. Man page parser — `ManPageResolver` + `parserType: "man_page"` config key in embedder
- [x] B. `enableRoleplay` config key — Roleplayer off by default; set `"enableRoleplay": true` at the **root** of config.json (not nested inside `researcher` or `roleplayer`)
- [x] C. Streaming output from Researcher directly to console while Roleplayer waits (pipeline parallelism)
- [x] E. Inconsistent enable/disable convention — `enableRoleplay` is a root-level boolean (`main.cpp:125`) while `reranker.enabled` is nested. Pick one; nested is more scalable.
- [x] F. `basePath` / `remotePath` silent aliasing in `GeneratorIP.cpp:32-33` and `RerankGeneratorIP.cpp:29-30` — the legacy key has no deprecation warning and isn't documented. Pick a canonical key and log or remove the fallback.
- [x] G. Inconsistent disable-on-failure policy — `Reranker` disables itself on init failure (`Reranker.cpp:15-19`); `Researcher` and `Roleplayer` carry an invalid generator and refuse work per call (`Researcher.cpp:23`, `Roleplayer.cpp:23`). Standardise on the reranker's pattern.
- [x] H. Magic config keys scattered across files — no central list; renaming a key requires a multi-file grep. Add a `ConfigKeys.h` with `inline constexpr` string constants.

- [x] D. Re-ranker — `Reranker` class in `src/asset/`; `EmbeddedRerankGenerator` (llama.cpp `LLAMA_POOLING_TYPE_RANK`) and `RerankGeneratorIP` (Cohere `/v1/rerank`); sits between Embedder search and Researcher; controlled by `reranker.enabled` + `reranker.topN` in config.json. **Awaiting model path** — set `reranker.generator.modelPath` and flip `"enabled": true` to activate.
