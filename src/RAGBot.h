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

    void processQuestion(const QString &question);

    Embedder m_embedder;
    Researcher m_researcher;
    Roleplayer m_roleplayer;
};

#endif // RAGBOT_H
