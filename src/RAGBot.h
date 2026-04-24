#ifndef RAGBOT_H
#define RAGBOT_H

#include "ConversationTurn.h"
#include "asset/Embedder.h"
#include "asset/Researcher.h"
#include "asset/Roleplayer.h"
#include "db/RoleplayDatabase.h"

class RAGBot
{
public:
    static constexpr int MaxHistoryTurns = 2;

    RAGBot(Embedder& embedder, Researcher& researcher, Roleplayer& roleplayer,
           RoleplayDatabase& roleplayDb, bool enableRoleplay);
    ~RAGBot() { qDebug() << "~RAGBot()"; }

    void start();

private:
    auto processQuestion(const QString& question) -> void;

    Embedder&         m_embedder;
    Researcher&       m_researcher;
    Roleplayer&       m_roleplayer;
    RoleplayDatabase& m_roleplayDb;
    bool              m_enableRoleplay;

    QVector<ConversationTurn> m_history;
};

#endif // RAGBOT_H
