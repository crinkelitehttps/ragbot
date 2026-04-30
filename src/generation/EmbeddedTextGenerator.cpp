#include "EmbeddedTextGenerator.h"
#include "../compat/Logging.h"
#include "llama.h"
#include "common.h"
#include "../ConfigKeys.h"
#include <array>
#include <cstdio>

static void initLlamaBackend()
{
    static bool initialized = false;
    if (!initialized) {
        llama_backend_init();
        initialized = true;
    }
}

EmbeddedTextGenerator::EmbeddedTextGenerator(const rb::Json& config)
{
    initLlamaBackend();

    llama_log_set([](ggml_log_level level, const char* text, void*) {
        if (level == GGML_LOG_LEVEL_ERROR)
            RAGBOT_LOG_ERROR("llama.cpp: {}", text);
    }, nullptr);

    m_temperature    = static_cast<float>(config.doubleValue(ConfigKeys::Temperature,   DefaultTemperature));
    m_topP           = static_cast<float>(config.doubleValue(ConfigKeys::TopP,          DefaultTopP));
    m_repeatPenalty  = static_cast<float>(config.doubleValue(ConfigKeys::RepeatPenalty, DefaultRepeatPenalty));
    m_maxTokenGen    = config.intValue(ConfigKeys::MaxTokens,     DefaultMaxTokenGen);
    m_enableThinking = config.boolValue(ConfigKeys::EnableThinking, false);

    const rb::String modelPath = config.stringValue(ConfigKeys::ModelPath);
    if (rb::str_empty(modelPath)) {
        RAGBOT_LOG_ERROR("EmbeddedTextGenerator: no modelPath in config");
        return;
    }

    m_model = llama_model_load_from_file(rb::to_std(modelPath).c_str(),
                                         llama_model_default_params());
    if (!m_model) {
        RAGBOT_LOG_ERROR("EmbeddedTextGenerator: failed to load model: {}", rb::to_std(modelPath));
        return;
    }

    llama_context_params ctx = llama_context_default_params();
    ctx.n_ctx   = DefaultCtx;
    ctx.n_batch = DefaultBatch;

    m_ctx = llama_init_from_model(m_model, ctx);
    if (!m_ctx) {
        RAGBOT_LOG_ERROR("EmbeddedTextGenerator: failed to create context");
        return;
    }

    m_isValid = true;
}

EmbeddedTextGenerator::~EmbeddedTextGenerator()
{
    if (m_ctx)   { llama_free(m_ctx);        m_ctx   = nullptr; }
    if (m_model) { llama_model_free(m_model); m_model = nullptr; }
}

auto EmbeddedTextGenerator::generateText(
        const rb::String& systemPrompt,
        bool isStream,
        const rb::String& prompt,
        const TokenSink& tokenSink
) -> rb::String
{
    if (!m_isValid) return {};

    const rb::String userTurn = m_enableThinking
        ? prompt : prompt + rb::from_std(" /no_think");
    const rb::String fullPrompt =
        rb::from_std("<|im_start|>system\n") + systemPrompt + rb::from_std("<|im_end|>\n")
        + rb::from_std("<|im_start|>user\n")   + userTurn     + rb::from_std("<|im_end|>\n")
        + rb::from_std("<|im_start|>assistant\n");

    auto vtok = common_tokenize(m_ctx, rb::to_std(fullPrompt), true, true);
    if (vtok.empty()) return {};

    const int nCtx = llama_n_ctx(m_ctx);
    if (static_cast<int>(vtok.size()) >= nCtx) {
        RAGBOT_LOG_WARN("EmbeddedTextGenerator: prompt {} tokens exceeds context {} — truncating",
                        static_cast<int>(vtok.size()), nCtx);
        vtok.resize(static_cast<size_t>(nCtx - 1));
    }

    llama_memory_clear(llama_get_memory(m_ctx), true);

    const int nBatch = static_cast<int>(llama_n_batch(m_ctx));
    for (int pos = 0; pos < static_cast<int>(vtok.size()); ) {
        const int chunk = std::min(nBatch, static_cast<int>(vtok.size()) - pos);
        llama_batch batch = llama_batch_get_one(vtok.data() + pos, chunk);
        if (llama_decode(m_ctx, batch) != 0) {
            RAGBOT_LOG_WARN("EmbeddedTextGenerator: prompt decode failed");
            return {};
        }
        pos += chunk;
    }

    llama_sampler* sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(sampler, llama_sampler_init_penalties(64, m_repeatPenalty, 0.0f, 0.0f));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(m_topP, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(m_temperature));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
    const llama_vocab* vocab = llama_model_get_vocab(m_model);

    rb::String response;
    bool inThinkBlock = false;
    for (int n = 0; n < m_maxTokenGen; ++n) {
        llama_token tok = llama_sampler_sample(sampler, m_ctx, -1);

        if (llama_vocab_is_eog(vocab, tok)) break;

        std::array<char, DefaultBufferLength> buf{};
        const int len = llama_token_to_piece(vocab, tok, buf.data(), buf.size(), 0, true);
        if (len > 0) {
            const rb::String piece = rb::from_std(std::string(buf.data(), static_cast<size_t>(len)));

            if (piece == rb::from_std("<think>"))  inThinkBlock = true;
            const bool visible = !inThinkBlock || m_enableThinking;
            if (visible) {
                response += piece;
                if (isStream) {
                    if (tokenSink)
                        tokenSink(rb::StringView(piece));
                    else {
                        std::fputs(rb::to_std(piece).c_str(), stdout);
                        std::fflush(stdout);
                    }
                }
            }
            if (piece == rb::from_std("</think>")) inThinkBlock = false;
        }

        llama_batch next = llama_batch_get_one(&tok, 1);
        if (llama_decode(m_ctx, next) != 0) break;
    }

    llama_sampler_free(sampler);
    return response;
}
