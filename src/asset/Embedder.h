#ifndef EMBEDDER_H
#define EMBEDDER_H

#include <QHash>
#include <QJsonObject>
#include "../generation/Generator.h"
#include "../db/EmbeddingDatabase.h"
#include "../parsers/Parser.h"

class Embedder
{
public:
    Embedder(const QJsonObject& embedderConfig);
    ~Embedder() = default;

    auto processAllFiles() -> void;

    // Embeds query and returns the top-K most similar stored chunks.
    auto search(const QString& query, int topK = 10)
        -> QVector<EmbeddingDatabase::SearchResult>;

    // Returns true if the file was newly indexed, false if already up-to-date.
    auto fileEmbed(QFile& file) -> bool;

    [[nodiscard]] auto isValid() const -> bool { return m_isValid; }

private:
    // First pass: scan all JSON files and build a complete id→object registry
    // so the parser can resolve copy-from inheritance chains.
    auto buildObjectRegistry() -> void;

    EmbeddingDatabase m_db;
    QString           m_files;
    Generator*        m_generator {};
    bool              m_isValid {};
    Parser*           m_parser {};
};

#endif // EMBEDDER_H
