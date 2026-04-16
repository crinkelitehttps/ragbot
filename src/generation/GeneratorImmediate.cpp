#include "GeneratorImmediate.h"
#include <QJsonObject>
#include <QDir>
#include <QDebug>
#include <QTextStream>
#include "llama.h"
#include "common.h"

//--------------------------------------------------------------------------------
GeneratorImmediate::GeneratorImmediate(const QJsonObject& config)
    : Generator(config)
{
    static bool backend_initialized = false;
    if (!backend_initialized) {
        llama_backend_init();
        backend_initialized = true;
    }

    llama_log_set([](ggml_log_level level, const char* text, void* user_data) -> void {
        Q_UNUSED(user_data)
        if (level == GGML_LOG_LEVEL_ERROR) {
            qCritical() << "llama.cpp error:" << text;
        }
    }, nullptr);

    const QString modeStr = config.value("mode").toString("embedding");
    m_mode = (modeStr == "generation") ? Mode::Generation : Mode::Embedding;

    const QString modelPath = config.value("modelPath").toString();
    if (modelPath.isEmpty()) {
        qCritical() << "GeneratorImmediate: no modelPath in config";
        return;
    }

    llama_model_params model_params = llama_model_default_params();
    m_model = llama_model_load_from_file(modelPath.toUtf8().constData(), model_params);
    if (m_model == nullptr) {
        qCritical() << "GeneratorImmediate: failed to load model:" << modelPath;
        return;
    }

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx   = DefaultCtx;
    ctx_params.n_batch = DefaultBatch;

    if (m_mode == Mode::Embedding) {
        ctx_params.embeddings    = true;
        ctx_params.pooling_type  = LLAMA_POOLING_TYPE_MEAN;
    }

    m_ctx = llama_init_from_model(m_model, ctx_params);
    if (m_ctx == nullptr) {
        qCritical() << "GeneratorImmediate: failed to create context";
        return;
    }

    m_isValid = true;
}

//--------------------------------------------------------------------------------
GeneratorImmediate::~GeneratorImmediate()
{
    if (m_ctx != nullptr) {
        llama_free(m_ctx);
        m_ctx = nullptr;
    }
    if (m_model != nullptr) {
        llama_model_free(m_model);
        m_model = nullptr;
    }
}


//--------------------------------------------------------------------------------
auto GeneratorImmediate::generate(const QString& data) -> QVector<float>
{
    if (m_ctx == nullptr || m_mode != Mode::Embedding) return {};

    std::string preparedData = data.toStdString();
    auto vtok = common_tokenize(m_ctx, preparedData, true, true);

    const auto safe_limit = llama_n_ubatch(m_ctx);
    if (vtok.size() > safe_limit) {
        qWarning() << "Truncating tokens from" << vtok.size() << "to" << safe_limit;
        vtok.resize(safe_limit);
    }

    llama_memory_t kv_cache = llama_get_memory(m_ctx);
    llama_memory_seq_rm(kv_cache, -1, 0, -1);

    llama_batch batch = llama_batch_init(static_cast<int>(vtok.size()), 0, 1);
    for (int32_t i = 0; i < static_cast<int32_t>(vtok.size()); i++) {
        common_batch_add(batch, vtok[i], i, {0}, (i == static_cast<int32_t>(vtok.size()) - 1));
    }

    if (llama_decode(m_ctx, batch) != 0) {
        qWarning() << "llama_decode failed";
        llama_batch_free(batch);
        return {};
    }

    const int n_embd = llama_model_n_embd(m_model);
    const float* emb = llama_get_embeddings_seq(m_ctx, 0);
    if (emb == nullptr) emb = llama_get_embeddings(m_ctx);

    QVector<float> result;
    if (emb != nullptr) {
        result.reserve(n_embd);
        for (int i = 0; i < n_embd; i++) result.append(emb[i]);
    }

    llama_batch_free(batch);
    return result;
}


//--------------------------------------------------------------------------------
auto GeneratorImmediate::generateText(
        QString& systemPrompt,
        bool isStream,
        QString& prompt
    ) -> QString
{
    if (!m_isValid || m_mode != Mode::Generation) {
        qWarning() << "GeneratorImmediate::generateText: not in generation mode or not valid";
        return {};
    }

    // Qwen3 / ChatML format
    const QString fullPrompt =
        "<|im_start|>system\n" + systemPrompt + "<|im_end|>\n"
        "<|im_start|>user\n"   + prompt       + "<|im_end|>\n"
        "<|im_start|>assistant\n";

    auto vtok = common_tokenize(m_ctx, fullPrompt.toStdString(), true, true);
    if (vtok.empty()) return {};

    const int n_ctx = llama_n_ctx(m_ctx);
    if (static_cast<int>(vtok.size()) >= n_ctx) {
        qWarning() << "GeneratorImmediate::generateText: prompt" << vtok.size()
                   << "tokens exceeds context size" << n_ctx << "— truncating";
        vtok.resize(n_ctx - 1);
    }

    llama_memory_t kv_cache = llama_get_memory(m_ctx);
    llama_memory_seq_rm(kv_cache, -1, 0, -1);

    // Process the prompt in sub-batches so it never exceeds n_batch in one call.
    const int n_batch = static_cast<int>(llama_n_batch(m_ctx));
    int n_processed = 0;
    while (n_processed < static_cast<int>(vtok.size())) {
        const int chunk = std::min(n_batch, static_cast<int>(vtok.size()) - n_processed);
        llama_batch batch = llama_batch_get_one(vtok.data() + n_processed, chunk);
        if (llama_decode(m_ctx, batch) != 0) {
            qWarning() << "GeneratorImmediate::generateText: decode failed on prompt";
            return {};
        }
        n_processed += chunk;
    }

    llama_sampler* smpl = llama_sampler_init_greedy();

    QString response;
    int n_generated = 0;

    while (n_generated < DefaultMaxTokenGen) {
        llama_token tokId = llama_sampler_sample(smpl, m_ctx, -1);

        if (llama_vocab_is_eog(llama_model_get_vocab(m_model), tokId)) break;

        std::array<char, DefaultBufferLength> buf;
        const int num = llama_token_to_piece(
            llama_model_get_vocab(m_model),
            tokId,
            buf.data(),
            sizeof(buf),
            0,
            true
        );
        if (num > 0) {
            const QString piece = QString::fromUtf8(buf.data(), num);
            response += piece;
            if (isStream) {
                QTextStream(stdout) << piece << Qt::flush;
            }
        }

        auto batch = llama_batch_get_one(&tokId, 1);
        if (llama_decode(m_ctx, batch) != 0) break;
        n_generated++;
    }

    llama_sampler_free(smpl);
    return response;
}
