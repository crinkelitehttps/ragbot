#include "RAGBot.h"
#include <QTextStream>
#include <QDebug>
#include <QCoreApplication>
#include <QFile>
// llama_silenced.h
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"

#include "common.h"

#pragma GCC diagnostic pop


RAGBot::RAGBot(
        const QString &embedModelPath,
        EmbeddingDatabase *db, 
        ConversationDatabase *convDb,
        const RemoteLLMConfig &llmConfig,
        const RoleplayConfig &rpConfig
    )
    : m_embedModelPath(embedModelPath)
    , m_db(db)
    , m_convDb(convDb)
    , m_llmConfig(llmConfig)
    , m_rpConfig(rpConfig)
    , m_remoteLLM(nullptr)
    , m_roleplayLLM(nullptr)
    , m_embedCtx(nullptr)
    , m_embedModel(nullptr)
{
    if (m_llmConfig.enabled) {
        m_remoteLLM = new RemoteLLMClient(m_llmConfig);
        qDebug() << "Research LLM enabled:" << m_llmConfig.baseUrl;
    } else {
        qDebug() << "Research LLM disabled - would use local models";
    }
    
    if (m_rpConfig.enabled) {
        m_roleplayLLM = new RemoteLLMClient(m_rpConfig.baseUrl, m_rpConfig.model, 60000);
        qDebug() << "Roleplay LLM enabled:" << m_rpConfig.baseUrl;
        qDebug() << "Character:" << m_rpConfig.characterName;
    } else {
        qDebug() << "Roleplay mode disabled";
    }
}

RAGBot::~RAGBot()
{
    cleanup();
    delete m_remoteLLM;
    delete m_roleplayLLM;
}

bool RAGBot::initialize()
{
    qDebug() << "Initializing embedding model...";
    
    llama_log_set([](ggml_log_level level, const char * text, void * user_data) {
        Q_UNUSED(user_data)
        if (level == GGML_LOG_LEVEL_ERROR) {
            fprintf(stderr, "%s", text);
        }
    }, nullptr);
    
    llama_backend_init();
    
    llama_model_params model_params = llama_model_default_params();
    m_embedModel = llama_model_load_from_file(m_embedModelPath.toUtf8().constData(), model_params);
    
    if (!m_embedModel) {
        qCritical() << "Failed to load embedding model";
        return false;
    }
    
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 2048;
    ctx_params.n_batch = 2048;
    ctx_params.n_ubatch = 2048;
    ctx_params.embeddings = true;
    ctx_params.pooling_type = LLAMA_POOLING_TYPE_MEAN;
    
    m_embedCtx = llama_init_from_model(m_embedModel, ctx_params);
    
    if (!m_embedCtx) {
        qCritical() << "Failed to create embedding context";
        return false;
    }
    
    qDebug() << "Embedding model ready";
    return true;
}

void RAGBot::startChatLoop()
{
    if (!initialize()) {
        QCoreApplication::exit(1);
        return;
    }
    
    qDebug() << "\n=== RAG Bot Ready ===";
    if (m_rpConfig.enabled) {
        qDebug() << "Mode: Two-stage (Research + Roleplay)";
        qDebug() << "Character:" << m_rpConfig.characterName;
    } else {
        qDebug() << "Mode: Research only";
    }
    qDebug() << "Type your questions (or 'quit' to exit)";
    qDebug() << "Commands: 'toggle roleplay' to enable/disable stage 2\n";
    
    QTextStream in(stdin);
    
    while (true) {
        QTextStream(stdout) << "\nYou: " << Qt::flush;
        QString question = in.readLine().trimmed();
        
        if (question.isEmpty()) continue;
        if (question.toLower() == "quit" || question.toLower() == "exit") {
            qDebug() << "Goodbye!";
            break;
        }
        if (question.toLower() == "toggle roleplay") {
            m_rpConfig.enabled = !m_rpConfig.enabled;
            qDebug() << "Roleplay mode:" << (m_rpConfig.enabled ? "ENABLED" : "DISABLED");
            continue;
        }
        
        processQuestion(question);
    }
    
    cleanup();
    QCoreApplication::quit();
}

QVector<float> RAGBot::generateEmbedding(const QString &text)
{
    std::vector<llama_token> tokens = common_tokenize(m_embedCtx, text.toStdString(), true);
    if (tokens.empty()) return {};
    
    unsigned int max_tokens = llama_n_ctx(m_embedCtx) - 10;
    if (tokens.size() > max_tokens) {
        tokens.resize(max_tokens);
    }
    
    llama_batch batch = llama_batch_init(tokens.size(), 0, 1);
    for (size_t i = 0; i < tokens.size(); i++) {
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
    return result;
}

void RAGBot::processQuestion(const QString &question)
{
    qDebug() << "\n[Searching database...]";
    
    QVector<float> queryEmb = generateEmbedding(question);
    if (queryEmb.isEmpty()) {
        qWarning() << "Failed to generate query embedding";
        return;
    }
    
    auto results = m_db->search(queryEmb, 10);
    
    if (results.isEmpty()) {
        qDebug() << "No relevant documents found";
        return;
    }
    
    qDebug() << QString("Found %1 relevant documents").arg(results.size());
    
    // Build context from top results
    QString context;
    for (int i = 0; i < results.size(); i++) {
        context += QString("Document %1 (similarity: %2):\n%3\n\n")
            .arg(i + 1)
            .arg(results[i].similarity, 0, 'f', 3)
            .arg(results[i].content);
    }
    
    // Stage 1: Research with remote LLM
    qDebug() << "\n[Stage 1: Research Query]";
    
    QString researchPrompt = QString(
        "You are a helpful assistant analyzing Cataclysm: Dark Days Ahead game data. "
        "Answer the question based on the provided context documents. "
        "Be factual, concise, and cite specific game mechanics, items, or data when relevant.\n\n"
        "Context:\n%1\n\n"
        "Question: %2"
    ).arg(context, question);
    
    QString researchAnswer;
    qDebug() << context;
    qDebug() << question;
    qDebug() << researchAnswer;
    
    if (m_llmConfig.enabled) {
        qInfo() << "m_llmConfig.enabled";
        QTextStream(stdout) << "\nBot (Research): " << Qt::flush;
        researchAnswer = m_remoteLLM->chat("", researchPrompt, false);
        QTextStream(stdout) << "\n" << Qt::flush;
    } else {
        qDebug() << "Would call local research model here";
        researchAnswer = "[Research answer would appear here with local model]";
    }
    
    if (researchAnswer.isEmpty()) {
        qWarning() << "No response from research LLM";
        return;
    }
    
    QString roleplayAnswer;
    
    // Stage 2: Roleplay response
    if (m_rpConfig.enabled && m_roleplayLLM) {
        qInfo() << "m_rpConfig.enabled";
        qDebug() << "\n[Stage 2: Roleplay Response]";

        QFile roleplayPromptFile("roleplayPrompt.txt");
        QString rp;
        
        if (roleplayPromptFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            rp = QString::fromUtf8(roleplayPromptFile.readAll());
            roleplayPromptFile.close();
        } else {
            qWarning() << "Failed to open roleplayPrompt.txt";
        }
        
        QString roleplayPrompt = rp.arg("Survivor", researchAnswer, question);
        qDebug() << roleplayPrompt;
        
        QTextStream(stdout) << "\n" << m_rpConfig.characterName << ": " << Qt::flush;
        roleplayAnswer = m_roleplayLLM->chat(
            m_rpConfig.characterBackground,
            roleplayPrompt,
            true
        );
        QTextStream(stdout) << "\n" << Qt::flush;
        
        if (roleplayAnswer.isEmpty()) {
            qWarning() << "No response from roleplay LLM";
        }
    }
    
    // Log conversation to database
    if (!m_convDb->logConversation(queryEmb, question, researchAnswer, roleplayAnswer)) {
        qWarning() << "Failed to log conversation to database";
    }
}

void RAGBot::cleanup()
{
    if (m_embedCtx) {
        llama_free(m_embedCtx);
        m_embedCtx = nullptr;
    }
    if (m_embedModel) {
        llama_model_free(m_embedModel);
        m_embedModel = nullptr;
    }
    llama_backend_free();
}
