#ifndef RAGBOT_H
#define RAGBOT_H

#include <QString>
#include <QVector>
#include "db/EmbeddingDatabase.h"
#include "db/ConversationDatabase.h"
#include "llm/LLMClient.h"
#include "llm/LLMConfig.h"
#include "config/RoleplayConfig.h"
#include "llama.h"

class RAGBot
{
public:
    RAGBot(const QString &embedModelPath, EmbeddingDatabase *db, 
           ConversationDatabase *convDb,
           const LLMConfig &llmConfig, const RoleplayConfig &rpConfig);
    ~RAGBot();
    
    bool initialize();
    void startChatLoop();

private:
    QVector<float> generateEmbedding(const QString &text);
    void processQuestion(const QString &question);
    void cleanup();

private:
    QString m_embedModelPath;
    EmbeddingDatabase *m_db;
    ConversationDatabase *m_convDb;
    LLMConfig m_llmConfig;
    RoleplayConfig m_rpConfig;
    LLMClient *m_remoteLLM;
    LLMClient *m_roleplayLLM;
    llama_context *m_embedCtx;
    llama_model *m_embedModel;
};

#endif // RAGBOT_H
