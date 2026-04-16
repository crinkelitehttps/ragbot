#ifndef EMBEDDER_H
#define EMBEDDER_H

#include <QFileInfo>
#include "../generation/Generator.h"
#include "../db/EmbeddingDatabase.h"
#include "llama.h"
#include "../parsers/Parser.h"

class Embedder
{
public:
    struct SearchResult
    {
    };

    struct SourceFile { QString value; };
    struct HelperContext { QString value; };

    Embedder(const QJsonObject& embedderConfig);

    ~Embedder() = default;

    auto processAllFiles() -> void;

    // Embeds query and returns the top-K most similar stored chunks.
    auto search(const QString& query, int topK = 10)
        -> QVector<EmbeddingDatabase::SearchResult>;

    // Returns true if the file was newly indexed, false if already up-to-date.
    auto fileEmbed(QFile& file) -> bool;
    static auto generationEmbed(const QString& generation) -> void;

    [[nodiscard]] auto isValid() const -> bool { return m_isValid; }

private:
    EmbeddingDatabase m_db;
    QString m_files;
    Generator* m_generator {};
    bool m_isValid {};
    Parser* m_parser {};
};

#endif // EMBEDDER_H
