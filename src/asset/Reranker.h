#ifndef RERANKER_H
#define RERANKER_H

#include <memory>
#include "../compat/Json.h"
#include "../db/EmbeddingDatabase.h"
#include "../generation/RerankGenerator.h"

class Reranker
{
public:
    explicit Reranker(const rb::Json& config);

    auto rerank(
        const rb::String& query,
        rb::Vector<EmbeddingDatabase::SearchResult> results
    ) -> rb::Vector<EmbeddingDatabase::SearchResult>;

    [[nodiscard]] auto isEnabled() const -> bool;

private:
    std::unique_ptr<RerankGenerator> m_generator;
    int  m_topN    { 5 };
    bool m_enabled { false };
};

#endif // RERANKER_H
