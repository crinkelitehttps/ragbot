#include "GeneratorImmediate.h"
#include "config/ConfigGenerator.h"

GeneratorImmediate::GeneratorImmediate(ConfigGenerator generatorConfig) 
    : Generator(generatorConfig)
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
    m_embedModel = llama_model_load_from_file(QString("PLACEHOLDER-MODEL-NAME").toUtf8().constData(), model_params);

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

void GeneratorImmediate::hello()
{
    qDebug() << "GeneratorImmediate Hello";

}
