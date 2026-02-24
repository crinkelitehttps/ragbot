#include "GeneratorImmediate.h"
#include "../config/ConfigGenerator.h"
#include <QJsonObject>
#include "llama.h"


//--------------------------------------------------------------------------------
GeneratorImmediate::GeneratorImmediate(ConfigGenerator generatorConfig) 
    : Generator(generatorConfig)
    , m_config(generatorConfig)
{
    QString jsonDir = QDir::homePath() + "/source/Cataclysm-DDA/data/json";
    if (!QDir(jsonDir).exists()) {
        qCritical() << "Embedder::Embedder(): JSON directory not found:" << jsonDir;
    }
    
    llama_log_set([](ggml_log_level level, const char * text, void * user_data) {
        Q_UNUSED(user_data)
        if (level == GGML_LOG_LEVEL_ERROR) {
            fprintf(stderr, "%s", text);
        }
    }, nullptr);
    
    llama_backend_init();

    llama_model_params model_params = llama_model_default_params();

    m_embedModel = llama_model_load_from_file(QString("/home/joe/.local/models/nomic-embed-text-v1.5.f32.gguf")
            .toUtf8().constData(), model_params);

    if (!m_embedModel) {
        qCritical() << "RAGBot::initialize(): Failed to load embedding model";
    }

    llama_context_params ctx_params = llama_context_default_params();

    ctx_params.n_ctx = 2048;
    ctx_params.n_batch = 2048;
    ctx_params.n_ubatch = 2048;
    ctx_params.embeddings = true;
    ctx_params.pooling_type = LLAMA_POOLING_TYPE_MEAN;

    m_embedCtx = llama_init_from_model(m_embedModel, ctx_params);

    if (!m_embedCtx) {
        qCritical() << "Embedder::Embedder(): Failed to create embedding context";
    }
}


//--------------------------------------------------------------------------------
QVector<float> GeneratorImmediate::generate(const QString data) 
{
    auto vt = common_tokenize(m_embedCtx, data.toStdString(), true);
    auto tokens = QVector<llama_token>(vt.begin(), vt.end());

    if (tokens.isEmpty()) return {};
    
    int max_tokens = llama_n_ctx(m_embedCtx) - 10;

    if (tokens.size() > max_tokens) {
        tokens.resize(max_tokens);
    }
    
    llama_batch batch = llama_batch_init(tokens.size(), 0, 1);
    for (int i = 0; i < tokens.size(); i++) {
        common_batch_add(batch, tokens[i], i, {0}, true);
    }
    
    if (llama_encode(m_embedCtx, batch) != 0) {
        llama_batch_free(batch);
        return {};
    }
    
    llama_synchronize(m_embedCtx);
    
    int n_embd = llama_model_n_embd(m_embedModel);
    const float *embeddings = llama_get_embeddings_seq(m_embedCtx, 0);
    
    if (!embeddings) {
        embeddings = llama_get_embeddings(m_embedCtx);
    }
    
    QVector<float> result;
    if (embeddings) {
        result.resize(n_embd);
        for (int i = 0; i < n_embd; i++) {
            result[i] = embeddings[i];
        }
    }
    
    llama_batch_free(batch);
    return QVector<float>();
};


//--------------------------------------------------------------------------------
QString GeneratorImmediate::generateText(
        QString& systemMessage,
        QString& prompt,
        bool isStream
    ) 
{
    Q_UNUSED(systemMessage)
    Q_UNUSED(prompt)
    Q_UNUSED(isStream)
    const QString r;

    return r;
}
