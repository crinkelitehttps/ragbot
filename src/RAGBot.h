#ifndef RAGBOT_H
#define RAGBOT_H

#include "asset/Embedder.h"
#include "asset/Researcher.h"
#include "asset/Roleplayer.h"
#include "db/RoleplayDatabase.h"

class RAGBot
{
public:
    RAGBot(Embedder& embedder, Researcher& researcher, Roleplayer& roleplayer,
           RoleplayDatabase& roleplayDb);
    ~RAGBot() { qDebug() << "~RAGBot()"; }

    void start();

private:
    auto processQuestion(const QString& question) -> void;

    Embedder&         m_embedder;
    Researcher&       m_researcher;
    Roleplayer&       m_roleplayer;
    RoleplayDatabase& m_roleplayDb;
};

#endif // RAGBOT_H
