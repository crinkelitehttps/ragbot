// Modified to use local llama-swap embedding server
#ifndef RESEARCHER_H
#define RESEARCHER_H

#include "../db/EmbeddingDatabase.h"
#include "../generation/Generator.h"
#include "../asset/Embedder.h"

class Researcher
{
public:
    Researcher(const QJsonObject& config);

    auto research(
        const QString& question,
        QVector<EmbeddingDatabase::SearchResult>& results
    ) -> QString;

private:
    Embedder* m_embedder;
    Generator* m_generator;
};

#endif // RESEARCHER_H
