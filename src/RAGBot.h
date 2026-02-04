#ifndef RAGBOT_H
#define RAGBOT_H

#include <QString>
#include <QVector>
#include "EmbeddingDatabase.h"
#include "ConversationDatabase.h"
#include "RemoteLLMClient.h"
#include "RemoteLLMConfig.h"
#include "RoleplayConfig.h"
#include "llama.h"

class RAGBot
{
public:
    RAGBot(const QString &embedModelPath, EmbeddingDatabase *db, 
           ConversationDatabase *convDb,
           const RemoteLLMConfig &llmConfig, const RoleplayConfig &rpConfig);
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
    RemoteLLMConfig m_llmConfig;
    RoleplayConfig m_rpConfig;
    RemoteLLMClient *m_remoteLLM;
    RemoteLLMClient *m_roleplayLLM;
    
    llama_model *m_embedModel;
    llama_context *m_embedCtx;
};

#endif // RAGBOT_H
