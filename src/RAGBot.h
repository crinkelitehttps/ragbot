#ifndef RAGBOT_H
#define RAGBOT_H

#include "asset/Embedder.h"
#include "asset/Researcher.h"
#include "asset/Roleplayer.h"

class RAGBot
{
public:
    RAGBot(const Embedder& embedder, const Researcher& researcher, const Roleplayer& roleplayer);
    ~RAGBot() { qDebug() << "~RAGBot()"; }
    void start();
private:

    auto processQuestion(const QString &question) -> void;

    const Embedder& m_embedder;
    const Researcher& m_researcher;
    const Roleplayer& m_roleplayer;
};

#endif // RAGBOT_H
