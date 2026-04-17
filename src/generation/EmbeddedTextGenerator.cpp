#include "EmbeddedTextGenerator.h"
#include <QDebug>
#include <QTextStream>
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
EmbeddedTextGenerator::EmbeddedTextGenerator(const QJsonObject& config)
{
    initLlamaBackend();

    llama_log_set([](ggml_log_level level, const char* text, void*) {
        if (level == GGML_LOG_LEVEL_ERROR)
            qCritical() << "llama.cpp:" << text;
    }, nullptr);

    m_temperature   = static_cast<float>(config.value("temperature"  ).toDouble(DefaultTemperature));
    m_topP          = static_cast<float>(config.value("topP"         ).toDouble(DefaultTopP));
    m_repeatPenalty = static_cast<float>(config.value("repeatPenalty").toDouble(DefaultRepeatPenalty));

    const QString modelPath = config.value("modelPath").toString();
    if (modelPath.isEmpty()) {
        qCritical() << "EmbeddedTextGenerator: no modelPath in config";
        return;
    }

    m_model = llama_model_load_from_file(modelPath.toUtf8().constData(),
                                         llama_model_default_params());
    if (!m_model) {
        qCritical() << "EmbeddedTextGenerator: failed to load model:" << modelPath;
        return;
    }

    llama_context_params ctx = llama_context_default_params();
    ctx.n_ctx   = DefaultCtx;
    ctx.n_batch = DefaultBatch;

    m_ctx = llama_init_from_model(m_model, ctx);
    if (!m_ctx) {
        qCritical() << "EmbeddedTextGenerator: failed to create context";
        return;
    }

    m_isValid = true;
}

//--------------------------------------------------------------------------------
EmbeddedTextGenerator::~EmbeddedTextGenerator()
{
    if (m_ctx)   { llama_free(m_ctx);        m_ctx   = nullptr; }
    if (m_model) { llama_model_free(m_model); m_model = nullptr; }
}

//--------------------------------------------------------------------------------
auto EmbeddedTextGenerator::generateText(
        const QString& systemPrompt,
        bool isStream,
        const QString& prompt
) -> QString
{
    if (!m_isValid) return {};

    // ChatML format — compatible with Qwen3, Mistral-Instruct, and most modern models.
    const QString fullPrompt =
        "<|im_start|>system\n" + systemPrompt + "<|im_end|>\n"
        "<|im_start|>user\n"   + prompt       + "<|im_end|>\n"
        "<|im_start|>assistant\n";

    auto vtok = common_tokenize(m_ctx, fullPrompt.toStdString(), true, true);
    if (vtok.empty()) return {};

    const int nCtx = llama_n_ctx(m_ctx);
    if (static_cast<int>(vtok.size()) >= nCtx) {
        qWarning() << "EmbeddedTextGenerator: prompt" << vtok.size()
                   << "tokens exceeds context" << nCtx << "— truncating";
        vtok.resize(nCtx - 1);
    }

    llama_memory_clear(llama_get_memory(m_ctx), true);

    const int nBatch = static_cast<int>(llama_n_batch(m_ctx));
    for (int pos = 0; pos < static_cast<int>(vtok.size()); ) {
        const int chunk = std::min(nBatch, static_cast<int>(vtok.size()) - pos);
        llama_batch batch = llama_batch_get_one(vtok.data() + pos, chunk);
        if (llama_decode(m_ctx, batch) != 0) {
            qWarning() << "EmbeddedTextGenerator: prompt decode failed";
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

    QString response;
    for (int n = 0; n < DefaultMaxTokenGen; ++n) {
        llama_token tok = llama_sampler_sample(sampler, m_ctx, -1);

        if (llama_vocab_is_eog(vocab, tok)) break;

        std::array<char, DefaultBufferLength> buf{};
        const int len = llama_token_to_piece(vocab, tok, buf.data(), buf.size(), 0, true);
        if (len > 0) {
            const QString piece = QString::fromUtf8(buf.data(), len);
            response += piece;
            if (isStream) QTextStream(stdout) << piece << Qt::flush;
        }

        llama_batch next = llama_batch_get_one(&tok, 1);
        if (llama_decode(m_ctx, next) != 0) break;
    }

    llama_sampler_free(sampler);
    return response;
}
