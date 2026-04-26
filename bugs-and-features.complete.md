# Bugs and Feature Ideas

## Bugs

- [x] 1. `RoleplayDatabase` is built but never used — instantiate in `main.cpp`, call from `RAGBot::processQuestion()`
- [x] 2. **Data integrity hole: source inserted before chunks, no wrapping transaction** — if process dies mid-file, source checksum is marked indexed but chunks are missing; permanently skipped on next run. Fix: wrap source + all chunk inserts in one SQLite transaction, roll back VectorIndex on failure.
- [x] 3. MD5 used for file change detection (`Embedder::fileEmbed()`) — use SHA256 instead
- [x] 4. `CDDAResolver::mergeObjects()` ignores CDDA `relative`/`proportional` merge semantics — objects using these end up with wrong stats
- [x] 5. `EmbeddedTextGenerator` uses greedy sampling (`llama_sampler_init_greedy()`) — repetition-prone; needs temperature/top-p config
- [x] 6. Blocked event loop + nested `QEventLoop` in `GeneratorIP` — `RAGBot::start()` blocks Qt thread; nested event loops are re-entrant and fragile
- [x] 7. Double directory scan during indexing — `processAllFiles()` runs `QDirIterator` twice (count + process)

## Feature Ideas

- [x] A. Wire up `RoleplayDatabase` to log each turn (overlaps with bug 1)
- [x] B. Multi-turn conversation context — pass 1–2 prior Q&A pairs into researcher/roleplayer prompts
- [x] C. Configurable `topK` and similarity threshold as config keys under `embedder`
- [x] D. Transaction batching for indexing (speed) — overlaps with bug 2 fix
- [x] E. Configurable sampling for `EmbeddedTextGenerator` — `temperature`, `top_p`, `repeat_penalty` as config keys
- [x] F. Support `relative`/`proportional` keys in `CDDAResolver` (overlaps with bug 4)
- [x] G. Multiple data directories — make `embedder.files` accept a JSON array
