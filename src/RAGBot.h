#ifndef RAGBOT_H
#define RAGBOT_H

#include <QString>
#include <QVector>
#include "db/EmbeddingDatabase.h"
#include "db/ConversationDatabase.h"
#include "config/LLMConfig.h"

#include "config/ConfigRoleplay.h"
#include "config/ConfigEmbed.h"
#include "config/ConfigResearch.h"

#include "llm/LLMClient.h"
#include "llama.h"

class RAGBot
{
public:
    RAGBot(
        ConfigEmbed &embedderConfig,
        ConfigResearch &researchConfig,
        ConfigRoleplay &roleplayConfig
    );

    ~RAGBot() { qDebug() << "~RAGBot()"; }
    
    bool initialize();
    void startChatLoop();

private:
    void cleanup();
    QVector<float> generateEmbedding(const QString &text);
    void processQuestion(const QString &question);

private:
    ConfigRoleplay m_configRoleplay;
    ConfigEmbed m_configEmbed;
    ConfigResearch m_configResearch;

#if 1
    llama_context *m_embedCtx;
    llama_model *m_embedModel;
#endif

};

#endif // RAGBOT_H
