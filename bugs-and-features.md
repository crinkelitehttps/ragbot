# Bugs and Feature Ideas

## Bugs

- [x] 1. Double directory scan during indexing — `processAllFiles()` runs `QDirIterator` twice (count + process); fix by computing total from `paths.size()` before the loop (already partially fixed — verify no second scan remains)
- [x] 2. `CDDAResolver::mergeObjects()` may handle `relative`/`proportional` incorrectly when chained across multiple inheritance levels — the separate post-pass processes deltas against the base object, not the already-merged result

## Feature Ideas

- [x] A. Man page parser — `ManPageResolver` + `parserType: "man_page"` config key in embedder
- [x] B. `enableRoleplay` config key — Roleplayer off by default; set `"enableRoleplay": true` at the **root** of config.json (not nested inside `researcher` or `roleplayer`)
- [x] C. Streaming output from Researcher directly to console while Roleplayer waits (pipeline parallelism)
- [ ] D. Re-ranker — `Reranker` class in `src/asset/`; `EmbeddedRerankGenerator` (llama.cpp `LLAMA_POOLING_TYPE_RANK`) and `RerankGeneratorIP` (Cohere `/v1/rerank`); sits between Embedder search and Researcher; controlled by `reranker.enabled` + `reranker.topN` in config.json. **Awaiting model path** — set `reranker.generator.modelPath` and flip `"enabled": true` to activate.
