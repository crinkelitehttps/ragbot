#include "GeneratorImmediate.h"
#include "../config/ConfigGenerator.h"

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

QByteArray GeneratorImmediate::generate(QByteArray question) 
{
    QString roleplayAnswer;
    
    // Stage 2: Roleplay response
    qDebug() << "RAGBot::processQuestion(): Roleplay Response";

    QFile roleplayPromptFile("roleplayPrompt.txt");
    QString rp;
    
    if (roleplayPromptFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        rp = QString::fromUtf8(roleplayPromptFile.readAll());
        roleplayPromptFile.close();
    } else {
        qWarning() << "RAGBot::processQuestion(): Failed to open roleplayPrompt.txt";
    }
    
#if 0
    QString roleplayPrompt = rp.arg("Survivor", researchAnswer, question);
    qDebug() << roleplayPrompt;
    
    QTextStream(stdout) << "\n" << m_config.characterName << ": " << Qt::flush;
    roleplayAnswer = m_roleplayLLM->chat(
        m_rpConfig.characterBackground,
        roleplayPrompt,
        true
    );

#endif
    QTextStream(stdout) << "\n" << Qt::flush;
    
    if (roleplayAnswer.isEmpty()) {
        qWarning() << "RAGBot::processQuestio(): No response from roleplay LLM";
    }
    
#if 0
    // Log conversation to database
    if (!m_convDb->logConversation(queryEmb, question, researchAnswer, roleplayAnswer)) {
        qWarning() << "RAGBot::processQuestion(): Failed to log conversation to database";
    }
#endif
    return QByteArray();
};
