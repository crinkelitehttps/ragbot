#ifndef RESEARCHER_H
#define RESEARCHER_H

#include <memory>
#include <QJsonObject>
#include <QString>
#include "../ConversationTurn.h"
#include "../db/EmbeddingDatabase.h"
#include "../generation/TextGenerator.h"

class Researcher
{
public:
    explicit Researcher(const QJsonObject& config);

    auto research(
        const QString& question,
        const QVector<EmbeddingDatabase::SearchResult>& results,
        const QVector<ConversationTurn>& history
    ) -> QString;

private:
    std::unique_ptr<TextGenerator> m_generator;
    QString                        m_instruction;
};

#endif // RESEARCHER_H
