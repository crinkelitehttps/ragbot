#include "EmbeddedRerankGenerator.h"
#include "../compat/Logging.h"
#include "llama.h"
#include "common.h"
#include "../ConfigKeys.h"

static void initLlamaBackend()
{
    static bool initialized = false;
    if (!initialized) {
        llama_backend_init();
        initialized = true;
    }
}


EmbeddedRerankGenerator::EmbeddedRerankGenerator(const rb::Json& config)
{
    initLlamaBackend();

    llama_log_set([](ggml_log_level level, const char* text, void*) {
        if (level == GGML_LOG_LEVEL_ERROR)
            RAGBOT_LOG_ERROR("llama.cpp: {}", text);
    }, nullptr);

    const rb::String modelPath = config.stringValue(ConfigKeys::ModelPath);
    if (rb::str_empty(modelPath)) {
        RAGBOT_LOG_ERROR("EmbeddedRerankGenerator: no modelPath in config");
        return;
    }

    m_model = llama_model_load_from_file(rb::to_std(modelPath).c_str(),
                                         llama_model_default_params());
    if (!m_model) {
        RAGBOT_LOG_ERROR("EmbeddedRerankGenerator: failed to load model: {}", rb::to_std(modelPath));
        return;
    }

    llama_context_params ctx = llama_context_default_params();
    ctx.n_ctx        = DefaultCtx;
    ctx.n_batch      = DefaultBatch;
    ctx.embeddings   = true;
    ctx.pooling_type = LLAMA_POOLING_TYPE_RANK;

    m_ctx = llama_init_from_model(m_model, ctx);
    if (!m_ctx) {
        RAGBOT_LOG_ERROR("EmbeddedRerankGenerator: failed to create context");
        return;
    }

    m_isValid = true;
}


EmbeddedRerankGenerator::~EmbeddedRerankGenerator()
{
    if (m_ctx)   { llama_free(m_ctx);        m_ctx   = nullptr; }
    if (m_model) { llama_model_free(m_model); m_model = nullptr; }
}


auto EmbeddedRerankGenerator::scoreOne(const rb::String& query,
                                       const rb::String& doc) -> float
{
    const llama_vocab* vocab = llama_model_get_vocab(m_model);
    const char* tmpl = llama_model_chat_template(m_model, "rerank");
    std::string prompt;
    if (tmpl) {
        prompt = tmpl;
        string_replace_all(prompt, "{query}",    rb::to_std(query));
        string_replace_all(prompt, "{document}", rb::to_std(doc));
    } else {
        prompt = rb::to_std(query);
        if (llama_vocab_get_add_eos(vocab))
            prompt += llama_vocab_get_text(vocab, llama_vocab_eos(vocab));
        if (llama_vocab_get_add_sep(vocab))
            prompt += llama_vocab_get_text(vocab, llama_vocab_sep(vocab));
        prompt += rb::to_std(doc);
    }

    auto vtok = common_tokenize(m_ctx, prompt, true, true);

    const int limit = static_cast<int>(llama_n_ubatch(m_ctx));
    if (static_cast<int>(vtok.size()) > limit) {
        RAGBOT_LOG_WARN("EmbeddedRerankGenerator: truncating tokens from {} to {}",
                        static_cast<int>(vtok.size()), limit);
        vtok.resize(static_cast<size_t>(limit));
    }

    llama_memory_clear(llama_get_memory(m_ctx), true);

    llama_batch batch = llama_batch_init(static_cast<int>(vtok.size()), 0, 1);
    for (int32_t i = 0; i < static_cast<int32_t>(vtok.size()); ++i)
        common_batch_add(batch, vtok[i], i, {0}, i == static_cast<int32_t>(vtok.size()) - 1);

    float result = 0.0f;
    if (llama_decode(m_ctx, batch) == 0) {
        const float* emb = llama_get_embeddings_seq(m_ctx, 0);
        if (emb) result = emb[0];
    } else {
        RAGBOT_LOG_WARN("EmbeddedRerankGenerator: llama_decode failed");
    }

    llama_batch_free(batch);
    return result;
}


auto EmbeddedRerankGenerator::score(const rb::String& query,
                                    const rb::Vector<rb::String>& documents) -> rb::Vector<float>
{
    if (!m_isValid) return {};

    rb::Vector<float> scores;
    scores.reserve(documents.size());
    for (const auto& doc : documents)
        scores.push_back(scoreOne(query, doc));

    return scores;
}
