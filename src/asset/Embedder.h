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
    auto fileEmbed(QFile& file) -> void;
    static auto generationEmbed(const QString& generation) -> void;

    [[nodiscard]] auto queryResults(const QString& query)
        -> QString;

    [[nodiscard]] auto isValid() const
        -> bool { return m_isValid; }

    [[nodiscard]] auto textVectors()
        -> QVector<float>&;

private:
    EmbeddingDatabase m_db;
    QString m_files;
    Generator* m_generator {};
    bool m_isValid {};
    Parser* m_parser {};
};

#endif // EMBEDDER_H
