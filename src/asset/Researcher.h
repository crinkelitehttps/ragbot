#ifndef RESEARCHER_H
#define RESEARCHER_H

#include <QJsonObject>
#include "../db/EmbeddingDatabase.h"
#include "../generation/Generator.h"

class Researcher
{
public:
    explicit Researcher(const QJsonObject& config);

    static auto research(
        const QString& question,
        QVector<EmbeddingDatabase::SearchResult>& results
    ) -> QString;

private:
    Generator* m_generator;
};

#endif // RESEARCHER_H
