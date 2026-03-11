#ifndef RAGBOT_H
#define RAGBOT_H

#include <QString>
#include <QVector>

#include "asset/Embedder.h"
#include "asset/Researcher.h"
#include "asset/Roleplayer.h"

class RAGBot
{
public:
    RAGBot(Embedder embedder, Researcher researcher, Roleplayer roleplayer);
    ~RAGBot() { qDebug() << "~RAGBot()"; }
    void init();
private:
    void cleanup();
    void processQuestion(const QString &question);

    Embedder m_embedder;
    Researcher m_researcher;
    Roleplayer m_roleplayer;
};

#endif // RAGBOT_H
