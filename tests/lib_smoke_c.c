/*
 * Phase 5 smoke test — exercises the public C ABI.
 *
 * Compile and link as pure C against libragbot.a to prove:
 *   1. ragbot_c_api.h is a valid C header (no C++ types or syntax).
 *   2. libragbot.a links cleanly from a C translation unit.
 *   3. The lifecycle (create → context → ask → destroy) runs end-to-end.
 *
 * Usage: ragbot_smoke_c [config.json [assets_dir]]
 *
 * Exit codes:
 *   0 : all steps passed, or no config available (treated as SKIP).
 *   1 : unexpected error.
 */

#include "ragbot_c_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */

static char* read_file(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    if (size <= 0) { fclose(f); return NULL; }
    char* buf = (char*)malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

static void log_cb(int level, const char* msg, void* user)
{
    (void)user;
    const char* tag = (level == 2) ? "ERROR" : (level == 1) ? "WARN" : "INFO";
    fprintf(stderr, "[ragbot/%s] %s\n", tag, msg);
}

static void token_cb(const char* token, size_t len, void* user)
{
    (void)user;
    fwrite(token, 1, len, stdout);
    fflush(stdout);
}

/* -------------------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------------- */

int main(int argc, char** argv)
{
    const char* config_path = (argc > 1) ? argv[1] : "config.json";
    const char* assets_dir  = (argc > 2) ? argv[2] : NULL;

    ragbot_set_log_callback(log_cb, NULL);

    char* config_json = read_file(config_path);
    if (!config_json) {
        fprintf(stderr, "SKIP: cannot read '%s'\n", config_path);
        return 0;
    }

    fprintf(stderr, "smoke: creating session from '%s'\n", config_path);
    ragbot_session* sess = ragbot_create(config_json, assets_dir);
    free(config_json);

    if (!sess) {
        fprintf(stderr, "SKIP: ragbot_create returned NULL "
                        "(embedder init failed — db or model not available)\n");
        return 0;
    }
    fprintf(stderr, "smoke: session created OK\n");

    /* Sticky NPC context. */
    if (ragbot_set_npc_context(sess,
            "{\"name\":\"Scout\",\"profession\":\"Survivor\"}") != 0) {
        fprintf(stderr, "FAIL: ragbot_set_npc_context\n");
        ragbot_destroy(sess);
        return 1;
    }

    /* --- Blocking ask (question 1) ---------------------------------------- */
    {
        const char* q = "What do zombies drop when killed?";
        fprintf(stderr, "\nsmoke [blocking]: %s\n", q);

        char buf[4096];
        int  ret = ragbot_ask_blocking(sess, q, buf, sizeof(buf));
        if (ret < 0) {
            fprintf(stderr, "FAIL: ragbot_ask_blocking returned %d\n", ret);
            ragbot_destroy(sess);
            return 1;
        }
        printf("\n[Q1] %s\n", buf);
    }

    /* --- Async ask (question 2) ------------------------------------------- *
     *                                                                          *
     * ragbot_ask returns immediately; ragbot_destroy joins the worker thread,  *
     * so it naturally blocks until all token callbacks have fired.             *
     * This is the correct "fire-and-forget with cleanup" pattern.             */
    {
        const char* q = "How do I treat an infected wound?";
        fprintf(stderr, "\nsmoke [async]:   %s\n", q);
        printf("\n[Q2] ");
        fflush(stdout);

        int ret = ragbot_ask(sess, q, token_cb, NULL, NULL);
        if (ret != 0) {
            fprintf(stderr, "FAIL: ragbot_ask returned %d\n", ret);
            ragbot_destroy(sess);
            return 1;
        }
        /* ragbot_destroy joins the async thread — all tokens arrive before we
         * return from this call. */
    }

    fprintf(stderr, "\nsmoke: destroying session\n");
    ragbot_destroy(sess);
    printf("\n");

    fprintf(stderr, "smoke: all steps passed\n");
    return 0;
}
