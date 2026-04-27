#include "Reranker.h"
#include "../generation/GeneratorFactory.h"
#include <algorithm>
#include <QDebug>


//--------------------------------------------------------------------------------
Reranker::Reranker(const QJsonObject& config)
    : m_topN(config.value("topN").toInt(5))
    , m_enabled(config.value("enabled").toBool(false))
{
    if (!m_enabled) return;

    m_generator = GeneratorFactory::createRerank(config.value("generator").toObject());
    if (!m_generator || !m_generator->isValid()) {
        qWarning() << "Reranker: generator failed to initialise — disabling";
        m_enabled = false;
        m_generator.reset();
    }
}


//--------------------------------------------------------------------------------
auto Reranker::isEnabled() const -> bool
{
    return m_enabled && m_generator && m_generator->isValid();
}


//--------------------------------------------------------------------------------
auto Reranker::rerank(
        const QString& query,
        QVector<EmbeddingDatabase::SearchResult> results
) -> QVector<EmbeddingDatabase::SearchResult>
{
    if (!isEnabled() || results.isEmpty()) return results;

    QStringList documents;
    documents.reserve(results.size());
    for (const auto& r : results) documents.append(r.content);

    qDebug() << "Reranker::rerank(): scoring" << results.size() << "documents";
    const QVector<float> scores = m_generator->score(query, documents);

    if (scores.size() != results.size()) {
        qWarning() << "Reranker::rerank(): score count mismatch — returning unmodified results";
        return results;
    }

    QVector<std::pair<float, EmbeddingDatabase::SearchResult>> ranked;
    ranked.reserve(results.size());
    for (int i = 0; i < results.size(); ++i)
        ranked.append({scores[i], results[i]});

    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    const int n = std::min(m_topN, static_cast<int>(ranked.size()));
    QVector<EmbeddingDatabase::SearchResult> out;
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        auto& entry = ranked[i];
        entry.second.rerankScore = entry.first;
        out.append(entry.second);
    }

    qDebug() << "Reranker::rerank(): returning top" << out.size() << "of" << results.size();
    return out;
}
