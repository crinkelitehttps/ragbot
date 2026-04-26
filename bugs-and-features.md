# Bugs and Feature Ideas

## Bugs

- [x] 1. Double directory scan during indexing — `processAllFiles()` runs `QDirIterator` twice (count + process); fix by computing total from `paths.size()` before the loop (already partially fixed — verify no second scan remains)
- [x] 2. `CDDAResolver::mergeObjects()` may handle `relative`/`proportional` incorrectly when chained across multiple inheritance levels — the separate post-pass processes deltas against the base object, not the already-merged result

## Feature Ideas

- [x] A. Man page parser — `ManPageResolver` + `parserType: "man_page"` config key in embedder
- [x] B. `enableRoleplay` config key — Roleplayer off by default; set `"enableRoleplay": true` in root config to enable it
- [x] C. Streaming output from Researcher directly to console while Roleplayer waits (pipeline parallelism)
