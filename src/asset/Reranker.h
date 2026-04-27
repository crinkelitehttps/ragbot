#ifndef RERANKER_H
#define RERANKER_H

#include <memory>
#include <QJsonObject>
#include "../db/EmbeddingDatabase.h"
#include "../generation/RerankGenerator.h"

class Reranker
{
public:
    explicit Reranker(const QJsonObject& config);

    // Reranks results by relevance and returns the top-N.
    // Returns results unchanged if disabled or generator is invalid.
    auto rerank(
        const QString& query,
        QVector<EmbeddingDatabase::SearchResult> results
    ) -> QVector<EmbeddingDatabase::SearchResult>;

    [[nodiscard]] auto isEnabled() const -> bool;

private:
    std::unique_ptr<RerankGenerator> m_generator;
    int  m_topN    { 5 };
    bool m_enabled { false };
};

#endif // RERANKER_H
