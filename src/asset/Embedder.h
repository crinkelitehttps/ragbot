#ifndef EMBEDDER_H
#define EMBEDDER_H

#include <QFileInfo>
#include "../generation/Generator.h"
#include "../db/EmbeddingDatabase.h"
#include "llama.h"
#include "../parsers/Parser.h"

class Embedder
{
    struct SearchResult
    {
    };

public:
    Embedder(const QJsonObject& embedderConfig);

    ~Embedder() = default;

    auto queryResults(const QString& query) -> QString;

    void processAllFiles();
    void fileEmbed(const QString& sourcePath);
    auto textVectors() -> QVector<float>&;
    [[nodiscard]] auto isValid() const -> bool { return m_isValid; }
    
    static auto generationEmbed(const QString& generation) -> void;

private:
    EmbeddingDatabase* m_db;
    Generator* m_generator;
    Parser* m_parser;
    QString m_files;
    bool m_isValid {};

};

#endif // EMBEDDER_H
