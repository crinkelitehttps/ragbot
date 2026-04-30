#include "ragbot_c_api.h"
#include "RAGBotSession.h"
#include "ConfigKeys.h"
#include "compat/Json.h"
#include "compat/Logging.h"
#include "compat/Strings.h"
#include "generation/TextGenerator.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

// ---------------------------------------------------------------------------
// Internal session struct (definition hidden from C callers)
// ---------------------------------------------------------------------------

struct ragbot_session {
    RAGBotSession* session { nullptr };

    std::string npc_context;
    std::string world_context;
    std::mutex  context_mutex;

    std::atomic<bool> ask_in_flight { false };
    std::thread       async_thread;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static auto build_augmented_question(ragbot_session* sess, const std::string& question)
    -> std::string
{
    std::lock_guard<std::mutex> lock(sess->context_mutex);
    if (sess->npc_context.empty() && sess->world_context.empty())
        return question;

    std::string prefix;
    if (!sess->npc_context.empty())
        prefix += "[NPC context: " + sess->npc_context + "]\n";
    if (!sess->world_context.empty())
        prefix += "[World context: " + sess->world_context + "]\n";
    return prefix + "\n" + question;
}

static auto make_token_sink(ragbot_token_cb on_token, void* user)
    -> TextGenerator::TokenSink
{
    if (on_token == nullptr) return {};
    return [on_token, user](rb::StringView token_sv) -> void {
        std::string utf8 = rb::to_std(token_sv);
        on_token(utf8.c_str(), utf8.size(), user);
    };
}

// ---------------------------------------------------------------------------
// Logging bridge
// ---------------------------------------------------------------------------

static struct {
    ragbot_log_cb fn   = nullptr;
    void*         user = nullptr;
} g_log;

static auto c_log_adapter(rb::LogLevel level, const std::string& msg) -> void
{
    if (g_log.fn != nullptr) {
        g_log.fn(static_cast<int>(level), msg.c_str(), g_log.user);
        return;
    }
    const char* tag = "INFO";
    if      (level == rb::LogLevel::Error) tag = "ERROR";
    else if (level == rb::LogLevel::Warn)  tag = "WARN";
    std::fprintf(stderr, "[%s] %s\n", tag, msg.c_str());
}

// ---------------------------------------------------------------------------
// API implementation
// ---------------------------------------------------------------------------

void ragbot_set_log_callback(ragbot_log_cb callback, void* user)
{
    g_log.fn   = callback;
    g_log.user = user;
    if (callback != nullptr)
        rb::install_log_callback(rb::LogCallback{c_log_adapter});
    else
        rb::install_log_callback({});
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto ragbot_create(const char* config_json, const char* assets_dir) -> ragbot_session*
{
    if (config_json == nullptr) return nullptr;

    auto config = rb::Json::parse(std::string_view(config_json));
    if (!config.isValid() || !config.isObject()) return nullptr;

    if (assets_dir != nullptr && assets_dir[0] != '\0') {
        auto roleplayer_cfg = config.value(ConfigKeys::Roleplayer);
        if (!roleplayer_cfg.isObject()) roleplayer_cfg = rb::Json::object();
        roleplayer_cfg.setString(ConfigKeys::AssetsDir, rb::from_std(std::string(assets_dir)));
        config.set(ConfigKeys::Roleplayer, roleplayer_cfg);
    }

    auto* sess = new ragbot_session;
    sess->session = new RAGBotSession(config);
    if (!sess->session->isValid()) {
        delete sess->session;
        delete sess;
        return nullptr;
    }
    return sess;
}

void ragbot_destroy(ragbot_session* sess)
{
    if (sess == nullptr) return;
    if (sess->async_thread.joinable())
        sess->async_thread.join();
    delete sess->session;
    delete sess;
}

auto ragbot_set_npc_context(ragbot_session* sess, const char* npc_json) -> int
{
    if (sess == nullptr) return -1;
    std::lock_guard<std::mutex> lock(sess->context_mutex);
    sess->npc_context = (npc_json != nullptr) ? npc_json : "";
    return 0;
}

auto ragbot_set_world_context(ragbot_session* sess, const char* world_json) -> int
{
    if (sess == nullptr) return -1;
    std::lock_guard<std::mutex> lock(sess->context_mutex);
    sess->world_context = (world_json != nullptr) ? world_json : "";
    return 0;
}

auto ragbot_ask(ragbot_session* sess, const char* question,
                ragbot_token_cb on_token, ragbot_done_cb on_done, // NOLINT(bugprone-easily-swappable-parameters)
                void* user) -> int
{
    if (sess == nullptr || question == nullptr) return -1;

    bool expected = false;
    if (!sess->ask_in_flight.compare_exchange_strong(expected, true))
        return -1;

    // Previous thread is done (ask_in_flight was false). Join to clean up.
    if (sess->async_thread.joinable())
        sess->async_thread.join();

    std::string augmented = build_augmented_question(sess, question);

    sess->async_thread = std::thread([sess,
                                      question_str = std::move(augmented),
                                      on_token,
                                      on_done,
                                      user]() -> void
    {
        auto sink = make_token_sink(on_token, user);
        rb::String result = sess->session->ask(rb::from_std(question_str), sink);
        std::string result_str = rb::to_std(result);

        sess->ask_in_flight.store(false);

        if (on_done != nullptr)
            on_done(result_str.c_str(), result_str.size(), user);
    });

    return 0;
}

auto ragbot_ask_blocking(ragbot_session* sess, const char* question,
                         char* out_buf, size_t out_buf_size) -> int
{
    if (sess == nullptr || question == nullptr) return -1;
    if (sess->ask_in_flight.load()) return -1;

    std::string augmented = build_augmented_question(sess, question);
    rb::String result = sess->session->ask(rb::from_std(augmented));
    std::string result_str = rb::to_std(result);

    if (out_buf != nullptr && out_buf_size > 0) {
        size_t copy_len = result_str.size() < out_buf_size - 1
                        ? result_str.size() : out_buf_size - 1;
        std::memcpy(out_buf, result_str.c_str(), copy_len);
        out_buf[copy_len] = '\0';
    }

    return static_cast<int>(result_str.size());
}
