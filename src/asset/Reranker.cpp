#include "Reranker.h"
#include "../generation/GeneratorFactory.h"
#include "../ConfigKeys.h"
#include "../compat/Logging.h"
#include <algorithm>


Reranker::Reranker(const rb::Json& config)
    : m_topN(config.intValue(ConfigKeys::TopN, 5))
    , m_enabled(config.boolValue(ConfigKeys::Enabled, false))
{
    if (!m_enabled) return;

    m_generator = GeneratorFactory::createRerank(config.value(ConfigKeys::Generator));
    if (!m_generator || !m_generator->isValid()) {
        RAGBOT_LOG_WARN("Reranker: generator failed to initialise — disabling");
        m_enabled = false;
        m_generator.reset();
    }
}

auto Reranker::isEnabled() const -> bool
{
    return m_enabled && m_generator && m_generator->isValid();
}

auto Reranker::rerank(
        const rb::String& query,
        rb::Vector<EmbeddingDatabase::SearchResult> results
) -> rb::Vector<EmbeddingDatabase::SearchResult>
{
    if (!isEnabled() || results.empty()) return results;

    rb::Vector<rb::String> documents;
    documents.reserve(results.size());
    for (const auto& r : results) documents.push_back(r.content);

    RAGBOT_LOG_INFO("Reranker::rerank(): scoring {} documents",
                    static_cast<int>(results.size()));
    const rb::Vector<float> scores = m_generator->score(query, documents);

    if (scores.size() != results.size()) {
        RAGBOT_LOG_WARN("Reranker::rerank(): score count mismatch — returning unmodified results");
        return results;
    }

    rb::Vector<std::pair<float, EmbeddingDatabase::SearchResult>> ranked;
    ranked.reserve(results.size());
    for (size_t i = 0; i < results.size(); ++i)
        ranked.push_back({scores[i], results[i]});

    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    const int n = std::min(m_topN, static_cast<int>(ranked.size()));
    rb::Vector<EmbeddingDatabase::SearchResult> out;
    out.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        auto& entry = ranked[static_cast<size_t>(i)];
        entry.second.rerankScore = entry.first;
        out.push_back(entry.second);
    }

    RAGBOT_LOG_INFO("Reranker::rerank(): returning top {} of {}",
                    static_cast<int>(out.size()), static_cast<int>(results.size()));
    return out;
}
