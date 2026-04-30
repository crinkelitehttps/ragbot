#ifndef RAGBOT_C_API_H
#define RAGBOT_C_API_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque session handle. */
typedef struct ragbot_session ragbot_session;

/* Callbacks — all strings are UTF-8 and valid only during the call.
 *
 * ragbot_token_cb : one streamed token; fires on the ragbot worker thread.
 * ragbot_done_cb  : full answer ready; fires on the ragbot worker thread.
 * ragbot_log_cb   : level 0=info 1=warn 2=error.
 *
 * THREADING: token and done callbacks fire on the internal worker thread.
 * Marshal to your UI thread before touching UI state. Do NOT call ragbot_ask
 * again from within on_done — post it to your own thread instead.
 */
typedef void (*ragbot_token_cb)(const char* token, size_t len, void* user);
typedef void (*ragbot_done_cb) (const char* answer, size_t len, void* user);
typedef void (*ragbot_log_cb)  (int level, const char* msg, void* user);

/* Create a session from a config JSON string (same schema as config.json).
 * assets_dir: directory containing roleplayPrompt.txt etc. Pass NULL to use
 *             the value already in config (or the "inputs" default).
 * Returns NULL on failure (bad JSON, embedder init error).
 */
ragbot_session* ragbot_create(const char* config_json, const char* assets_dir);

/* Destroy a session. Joins the worker thread; blocks if an ask is in flight. */
void ragbot_destroy(ragbot_session* session);

/* Set sticky NPC / world context (replaces previous value; NULL clears).
 * Prepended to each question until replaced. Returns 0 on success, -1 on error.
 */
int ragbot_set_npc_context  (ragbot_session* session, const char* npc_json);
int ragbot_set_world_context(ragbot_session* session, const char* world_json);

/* Async ask. Returns immediately.
 *   0  : ask dispatched successfully.
 *  -1  : already in flight (previous ask not done), or bad arguments.
 * on_token fires for each streamed token (may be NULL).
 * on_done  fires when the full answer is available (may be NULL).
 */
int ragbot_ask(ragbot_session* session,
               const char*     question,
               ragbot_token_cb on_token,
               ragbot_done_cb  on_done,
               void*           user);

/* Blocking ask. Writes a NUL-terminated answer into out_buf (truncated to
 * out_buf_size bytes including the terminator). Returns the full byte-length
 * of the answer (may exceed out_buf_size). Returns -1 on error.
 * Fails with -1 if an async ask is already in flight.
 */
int ragbot_ask_blocking(ragbot_session* session,
                        const char*     question,
                        char*           out_buf,
                        size_t          out_buf_size);

/* Install a global log callback. Pass NULL to restore default (stderr).
 * Not thread-safe; call before ragbot_create. Level: 0=info 1=warn 2=error.
 */
void ragbot_set_log_callback(ragbot_log_cb callback, void* user);

#ifdef __cplusplus
}
#endif

#endif /* RAGBOT_C_API_H */
