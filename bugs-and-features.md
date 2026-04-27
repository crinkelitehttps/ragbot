# Bugs and Feature Ideas

## Bugs

- [x] 1. Double directory scan during indexing — `processAllFiles()` runs `QDirIterator` twice (count + process); fix by computing total from `paths.size()` before the loop (already partially fixed — verify no second scan remains)
- [x] 2. `CDDAResolver::mergeObjects()` may handle `relative`/`proportional` incorrectly when chained across multiple inheritance levels — the separate post-pass processes deltas against the base object, not the already-merged result
- [x] 3. `SearchResult::similarity` silently overwritten by `Reranker::rerank()` with the cross-encoder score — `Researcher` then labels it `similarity:` in the LLM context, which is wrong. Fixed by adding a separate `rerankScore` field (negative = not reranked); `Researcher` now uses `relevance:` label when reranked.
- [x] 4. `RerankGeneratorIP::score()` pre-fills scores with zeros and returns silently on a malformed or empty `results` array — the caller cannot distinguish a real zero score from an HTTP/parse failure. Fixed by adding `qWarning()` when the results array is empty or shorter than the document count.

## Feature Ideas

- [x] A. Man page parser — `ManPageResolver` + `parserType: "man_page"` config key in embedder
- [x] B. `enableRoleplay` config key — Roleplayer off by default; set `"enableRoleplay": true` at the **root** of config.json (not nested inside `researcher` or `roleplayer`)
- [x] C. Streaming output from Researcher directly to console while Roleplayer waits (pipeline parallelism)
- [x] D. Re-ranker — `Reranker` class in `src/asset/`; `EmbeddedRerankGenerator` (llama.cpp `LLAMA_POOLING_TYPE_RANK`) and `RerankGeneratorIP` (Cohere `/v1/rerank`); sits between Embedder search and Researcher; controlled by `reranker.enabled` + `reranker.topN` in config.json. **Awaiting model path** — set `reranker.generator.modelPath` and flip `"enabled": true` to activate.
