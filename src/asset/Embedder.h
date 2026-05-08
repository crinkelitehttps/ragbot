#ifndef EMBEDDER_H
#define EMBEDDER_H

#include <memory>
#include "../compat/Json.h"
#include "../compat/Types.h"
#include "../generation/EmbeddingGenerator.h"
#include "../db/EmbeddingDatabase.h"
#include "../parsers/CDDAResolver.h"
#include "../parsers/Parser.h"

class Embedder
{
public:
    explicit Embedder(const rb::Json& config);

    auto search(const rb::String& query) -> rb::Vector<EmbeddingDatabase::SearchResult>;

    [[nodiscard]] auto lastQueryEmbedding() const -> const rb::Vector<float>&
        { return m_lastQueryEmbedding; }

    [[nodiscard]] auto isValid() const -> bool { return m_isValid; }

private:
    auto processAllFiles() -> void;
    auto processJsonFiles(const rb::Vector<rb::String>& paths) -> std::pair<int, int>;

    EmbeddingDatabase                   m_db;
    rb::Vector<rb::String>              m_files;
    std::unique_ptr<EmbeddingGenerator> m_generator;
    std::unique_ptr<Parser>             m_parser;
    CDDAResolver::Registry              m_registry;
    rb::Vector<float>                   m_lastQueryEmbedding;
    int                                 m_topK { 10 };
    float                               m_similarityThreshold { 0.0f };
    bool                                m_isValid { false };
};

#endif // EMBEDDER_H
