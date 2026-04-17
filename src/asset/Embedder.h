#ifndef EMBEDDER_H
#define EMBEDDER_H

#include <memory>
#include <QFile>
#include <QHash>
#include <QJsonObject>
#include "../generation/EmbeddingGenerator.h"
#include "../db/EmbeddingDatabase.h"
#include "../parsers/CDDAResolver.h"
#include "../parsers/Parser.h"

class Embedder
{
public:
    explicit Embedder(const QJsonObject& config);

    // Embeds query and returns the top-K most similar stored chunks.
    auto search(const QString& query, int topK = 10) -> QVector<EmbeddingDatabase::SearchResult>;

    [[nodiscard]] auto isValid() const -> bool { return m_isValid; }

private:
    auto processAllFiles() -> void;

    // Returns true if the file was newly indexed, false if already up-to-date.
    auto fileEmbed(QFile& file) -> bool;

    EmbeddingDatabase                   m_db;
    QString                             m_files;
    std::unique_ptr<EmbeddingGenerator> m_generator;
    std::unique_ptr<Parser>             m_parser;
    CDDAResolver::Registry              m_registry;
    bool                                m_isValid { false };
};

#endif // EMBEDDER_H
