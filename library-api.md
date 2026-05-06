# RAGBot Library API

RAGBot can be built as a static library (`libragbot.a`) with a stable, Qt-free C ABI. This document covers the public API surface, threading rules, JSON schemas for game context, and linking instructions.

---

## Build

### CMake (recommended)

```bash
# Library only — Qt build, no embedded inference
cmake -B ../build-ragbot-lib -S . \
    -DRAGBOT_LIBRARY=ON \
    -DRAGBOT_USE_QT=ON
cmake --build ../build-ragbot-lib -j$(nproc)
```

```bash
# Library with in-process inference (llama.cpp)
cmake -B ../build-ragbot-lib -S . \
    -DRAGBOT_LIBRARY=ON \
    -DRAGBOT_USE_QT=ON \
    -DRAGBOT_EMBEDDED_INFERENCE=ON
cmake --build ../build-ragbot-lib -j$(nproc)
```

```bash
# Qt-free build (uses libcurl + nlohmann/json + {fmt} fetched automatically)
cmake -B ../build-ragbot-lib -S . \
    -DRAGBOT_LIBRARY=ON \
    -DRAGBOT_USE_QT=OFF
cmake --build ../build-ragbot-lib -j$(nproc)
```

### qmake (alternative, embedded inference only)

```bash
./b-lib.sh    # outputs libragbot.a to ../build-ragbot-lib/
```

---

## Linking

The public header is `src/ragbot_c_api.h`. Include it from C or C++; it contains zero Qt types.

```bash
# Minimal link line (Qt build, network-only inference)
g++ my_app.cpp -o my_app \
    -I path/to/ragbot/src \
    path/to/libragbot.a \
    -lQt5Core -lQt5Network -lsqlite3 -lpthread -ldl -lm -lstdc++
```

```bash
# With embedded inference, also add:
    -lcommon -lllama -lggml -lggml-base -lggml-cpu -lggml-vulkan \
    -lopenblas -lgomp -lvulkan
```

> **Qt version:** the library is built against Qt 5.15.2 from `/home/joe/Qt/5.15.2/gcc_64`. The system Qt (5.15.18) causes a runtime crash. Host projects that already link Qt must use a compatible Qt build or arrange ABI isolation.

---

## API Reference

### Types

```c
#include "ragbot_c_api.h"

typedef struct ragbot_session ragbot_session;   /* opaque */

typedef void (*ragbot_token_cb)(const char* token, size_t len, void* user);
typedef void (*ragbot_done_cb) (const char* answer, size_t len, void* user);
typedef void (*ragbot_log_cb)  (int level, const char* msg,    void* user);
```

All strings are UTF-8. String pointers passed to callbacks are valid only for the duration of that call — copy if you need to retain them.

Log levels: `0` = info, `1` = warn, `2` = error.

---

### Lifecycle

```c
ragbot_session* ragbot_create(const char* config_json, const char* assets_dir);
```

Creates a session. `config_json` is a UTF-8 string containing the full `config.json` object (same schema used by the console binary). `assets_dir` is the directory containing `roleplayPrompt.txt` and other prompt files; pass `NULL` to use the value already in config (or the `"inputs"` default).

Returns `NULL` on failure (bad JSON, embedder init error, model not found).

---

```c
void ragbot_destroy(ragbot_session* session);
```

Destroys a session. Joins the internal worker thread; blocks until any in-flight async ask completes. Safe to call with `NULL`.

---

### Logging

```c
void ragbot_set_log_callback(ragbot_log_cb callback, void* user);
```

Installs a global log callback. Call this **before** `ragbot_create`; it is not thread-safe. Pass `NULL` to restore the default (stderr). Without a callback, log lines go to `[INFO/WARN/ERROR] …` on stderr.

---

### Game context

```c
int ragbot_set_npc_context  (ragbot_session* session, const char* npc_json);
int ragbot_set_world_context(ragbot_session* session, const char* world_json);
```

Sets sticky context that is prepended to every subsequent question until replaced. `NULL` clears the context. Returns `0` on success, `-1` if `session` is `NULL`.

Context is prepended as:

```
[NPC context: <npc_json>]
[World context: <world_json>]

<question>
```

The strings are forwarded verbatim to the pipeline (they are not parsed by the library). See [JSON schemas](#json-schemas) below for the recommended shapes.

Thread-safe: `set_npc_context` / `set_world_context` may be called from any thread at any time, including while an ask is in flight.

---

### Asking questions

```c
int ragbot_ask(ragbot_session* session,
               const char*     question,
               ragbot_token_cb on_token,
               ragbot_done_cb  on_done,
               void*           user);
```

Dispatches an async question. Returns immediately.

- Returns `0` on success.
- Returns `-1` if an ask is already in flight, or if `session` / `question` is `NULL`.
- `on_token` fires for each streamed token (may be `NULL`).
- `on_done` fires once when the full answer is available (may be `NULL`).

**Threading:** both callbacks fire on the ragbot internal worker thread. Marshal to your own UI thread before touching UI state. Do not call `ragbot_ask` again from within `on_done` — post it to your own thread instead.

`ragbot_destroy` joins the worker thread, so the pattern `ragbot_ask(…); ragbot_destroy(…)` is a correct "fire-and-wait" idiom.

---

```c
int ragbot_ask_blocking(ragbot_session* session,
                        const char*     question,
                        char*           out_buf,
                        size_t          out_buf_size);
```

Synchronous ask. Blocks on the calling thread until the full answer is ready.

- Returns the full byte-length of the answer (may exceed `out_buf_size`).
- Returns `-1` on error or if an async ask is already in flight.
- Writes a NUL-terminated answer into `out_buf`, truncated to `out_buf_size - 1` bytes. Pass `NULL` / `0` if you only want the length.

---

## JSON schemas

These are the recommended shapes for NPC and world context. The library forwards them verbatim; field names are a convention for the LLM prompt, not parsed by the C API.

### NPC context

```json
{
    "name":                   "Scout",
    "profession":             "Survivor",
    "faction":                "Free Merchants",
    "traits":                 ["Quick", "Night Vision"],
    "mutations":              [],
    "skills":                 {"survival": 5, "first_aid": 3, "melee": 4},
    "health_state":           "healthy",
    "mood":                   "cautious",
    "relationship_to_player": "neutral"
}
```

### World context

```json
{
    "time_of_day":        "02:30",
    "weather":            "light rain",
    "location_summary":   "abandoned warehouse, outskirts of Woodstock",
    "nearby_entities":    ["zombie", "zombie brute", "rat"],
    "current_threat_level": "moderate"
}
```

---

## Minimal C example

```c
#include "ragbot_c_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void on_log(int level, const char* msg, void* user) {
    (void)user;
    fprintf(stderr, "[%s] %s\n", level == 2 ? "ERR" : level == 1 ? "WRN" : "INF", msg);
}

static void on_token(const char* tok, size_t len, void* user) {
    (void)user; fwrite(tok, 1, len, stdout); fflush(stdout);
}

int main(void) {
    ragbot_set_log_callback(on_log, NULL);

    /* Read config.json into a string (not shown). */
    const char* config_json = /* … */;

    ragbot_session* sess = ragbot_create(config_json, NULL);
    if (!sess) { fputs("ragbot_create failed\n", stderr); return 1; }

    ragbot_set_npc_context(sess,
        "{\"name\":\"Scout\",\"profession\":\"Survivor\"}");

    /* Async ask — ragbot_destroy blocks until done. */
    ragbot_ask(sess, "What do zombies drop when killed?", on_token, NULL, NULL);
    ragbot_destroy(sess);
    return 0;
}
```

---

## Threading summary

| Call | Thread | Notes |
|------|--------|-------|
| `ragbot_set_log_callback` | any (before create) | not thread-safe |
| `ragbot_create` | any | spawns internal worker |
| `ragbot_destroy` | any | joins worker; blocks until ask done |
| `ragbot_set_npc_context` | any | thread-safe |
| `ragbot_set_world_context` | any | thread-safe |
| `ragbot_ask` | any | returns immediately; callbacks on worker thread |
| `ragbot_ask_blocking` | any (not from a callback) | blocks calling thread |
| `on_token` / `on_done` callbacks | **worker thread** | marshal before touching UI |
