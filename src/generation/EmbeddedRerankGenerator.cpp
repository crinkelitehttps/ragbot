#include "EmbeddedRerankGenerator.h"
#include <QDebug>
#include "llama.h"
#include "common.h"

static void initLlamaBackend()
{
    static bool initialized = false;
    if (!initialized) {
        llama_backend_init();
        initialized = true;
    }
}


//--------------------------------------------------------------------------------
EmbeddedRerankGenerator::EmbeddedRerankGenerator(const QJsonObject& config)
{
    initLlamaBackend();

    llama_log_set([](ggml_log_level level, const char* text, void*) {
        if (level == GGML_LOG_LEVEL_ERROR)
            qCritical() << "llama.cpp:" << text;
    }, nullptr);

    const QString modelPath = config.value("modelPath").toString();
    if (modelPath.isEmpty()) {
        qCritical() << "EmbeddedRerankGenerator: no modelPath in config";
        return;
    }

    m_model = llama_model_load_from_file(modelPath.toUtf8().constData(),
                                         llama_model_default_params());
    if (!m_model) {
        qCritical() << "EmbeddedRerankGenerator: failed to load model:" << modelPath;
        return;
    }

    llama_context_params ctx = llama_context_default_params();
    ctx.n_ctx        = DefaultCtx;
    ctx.n_batch      = DefaultBatch;
    ctx.embeddings   = true;
    ctx.pooling_type = LLAMA_POOLING_TYPE_RANK;

    m_ctx = llama_init_from_model(m_model, ctx);
    if (!m_ctx) {
        qCritical() << "EmbeddedRerankGenerator: failed to create context";
        return;
    }

    m_isValid = true;
}


//--------------------------------------------------------------------------------
EmbeddedRerankGenerator::~EmbeddedRerankGenerator()
{
    if (m_ctx)   { llama_free(m_ctx);        m_ctx   = nullptr; }
    if (m_model) { llama_model_free(m_model); m_model = nullptr; }
}


//--------------------------------------------------------------------------------
auto EmbeddedRerankGenerator::scoreOne(const QString& query, const QString& doc) -> float
{
    const llama_vocab* vocab = llama_model_get_vocab(m_model);

    // Use the model's built-in "rerank" chat template if available; otherwise
    // fall back to joining query and document with EOS/SEP separator tokens.
    const char* tmpl = llama_model_chat_template(m_model, "rerank");
    std::string prompt;
    if (tmpl) {
        prompt = tmpl;
        string_replace_all(prompt, "{query}",    query.toStdString());
        string_replace_all(prompt, "{document}", doc.toStdString());
    } else {
        prompt = query.toStdString();
        if (llama_vocab_get_add_eos(vocab))
            prompt += llama_vocab_get_text(vocab, llama_vocab_eos(vocab));
        if (llama_vocab_get_add_sep(vocab))
            prompt += llama_vocab_get_text(vocab, llama_vocab_sep(vocab));
        prompt += doc.toStdString();
    }

    auto vtok = common_tokenize(m_ctx, prompt, true, true);

    const int limit = static_cast<int>(llama_n_ubatch(m_ctx));
    if (static_cast<int>(vtok.size()) > limit) {
        qWarning() << "EmbeddedRerankGenerator: truncating tokens from"
                   << vtok.size() << "to" << limit;
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
        qWarning() << "EmbeddedRerankGenerator: llama_decode failed";
    }

    llama_batch_free(batch);
    return result;
}


//--------------------------------------------------------------------------------
auto EmbeddedRerankGenerator::score(const QString& query, const QStringList& documents) -> QVector<float>
{
    if (!m_isValid) return {};

    QVector<float> scores;
    scores.reserve(documents.size());
    for (const auto& doc : documents)
        scores.append(scoreOne(query, doc));

    return scores;
}
