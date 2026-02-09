#ifndef RAGBOT_H
#define RAGBOT_H

#include <QString>
#include <QVector>

#include "config/ConfigEmbed.h"
#include "config/ConfigRoleplay.h"
#include "config/ConfigResearch.h"

#include "asset/Embedder.h"
#include "asset/Researcher.h"
#include "asset/Roleplayer.h"

class RAGBot
{
public:
    RAGBot(
        ConfigEmbed embedderConfig,
        ConfigResearch researchConfig,
        ConfigRoleplay roleplayConfig
    );

    ~RAGBot() { qDebug() << "~RAGBot()"; }
    
    void startChatLoop();

private:
    void cleanup();
    void processQuestion(const QString &question);

private:
    Embedder m_embedder;
    Researcher m_researcher;
    Roleplayer m_roleplayer;
};

#endif // RAGBOT_H
