#ifndef RESEARCHER_H
#define RESEARCHER_H

#include <QJsonObject>
#include <QString>
#include "../db/EmbeddingDatabase.h"
#include "../generation/Generator.h"

class Researcher
{
public:
    explicit Researcher(const QJsonObject& config);
    ~Researcher() { delete m_generator; }

    // Synthesizes a factual answer from the retrieved chunks.
    auto research(
        const QString& question,
        const QVector<EmbeddingDatabase::SearchResult>& results
    ) -> QString;

private:
    Generator* m_generator {};
    QString    m_instruction;
};

#endif // RESEARCHER_H
