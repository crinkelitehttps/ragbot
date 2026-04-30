#include "EmbeddedEmbeddingGenerator.h"
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

EmbeddedEmbeddingGenerator::EmbeddedEmbeddingGenerator(const rb::Json& config)
{
    initLlamaBackend();

    llama_log_set([](ggml_log_level level, const char* text, void*) {
        if (level == GGML_LOG_LEVEL_ERROR)
            RAGBOT_LOG_ERROR("llama.cpp: {}", text);
    }, nullptr);

    const rb::String modelPath = config.stringValue(ConfigKeys::ModelPath);
    if (rb::str_empty(modelPath)) {
        RAGBOT_LOG_ERROR("EmbeddedEmbeddingGenerator: no modelPath in config");
        return;
    }

    m_model = llama_model_load_from_file(rb::to_std(modelPath).c_str(),
                                         llama_model_default_params());
    if (!m_model) {
        RAGBOT_LOG_ERROR("EmbeddedEmbeddingGenerator: failed to load model: {}",
                         rb::to_std(modelPath));
        return;
    }

    llama_context_params ctx = llama_context_default_params();
    ctx.n_ctx          = DefaultCtx;
    ctx.n_batch        = DefaultBatch;
    ctx.embeddings     = true;
    ctx.pooling_type   = LLAMA_POOLING_TYPE_MEAN;

    m_ctx = llama_init_from_model(m_model, ctx);
    if (!m_ctx) {
        RAGBOT_LOG_ERROR("EmbeddedEmbeddingGenerator: failed to create context");
        return;
    }

    m_isValid = true;
}

EmbeddedEmbeddingGenerator::~EmbeddedEmbeddingGenerator()
{
    if (m_ctx)   { llama_free(m_ctx);        m_ctx   = nullptr; }
    if (m_model) { llama_model_free(m_model); m_model = nullptr; }
}

auto EmbeddedEmbeddingGenerator::generate(const rb::String& data) -> rb::Vector<float>
{
    if (!m_isValid) return {};

    auto vtok = common_tokenize(m_ctx, rb::to_std(data), true, true);

    const auto limit = llama_n_ubatch(m_ctx);
    if (vtok.size() > limit) {
        RAGBOT_LOG_WARN("EmbeddedEmbeddingGenerator: truncating tokens from {} to {}",
                        static_cast<int>(vtok.size()), static_cast<int>(limit));
        vtok.resize(limit);
    }

    llama_memory_clear(llama_get_memory(m_ctx), true);

    llama_batch batch = llama_batch_init(static_cast<int>(vtok.size()), 0, 1);
    for (int32_t i = 0; i < static_cast<int32_t>(vtok.size()); ++i) {
        common_batch_add(batch, vtok[i], i, {0},
                         i == static_cast<int32_t>(vtok.size()) - 1);
    }

    if (llama_decode(m_ctx, batch) != 0) {
        RAGBOT_LOG_WARN("EmbeddedEmbeddingGenerator: llama_decode failed");
        llama_batch_free(batch);
        return {};
    }

    const int    n   = llama_model_n_embd(m_model);
    const float* emb = llama_get_embeddings_seq(m_ctx, 0);
    if (!emb) emb    = llama_get_embeddings(m_ctx);

    rb::Vector<float> result;
    if (emb) {
        result.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) result.push_back(emb[i]);
    }

    llama_batch_free(batch);
    return result;
}
