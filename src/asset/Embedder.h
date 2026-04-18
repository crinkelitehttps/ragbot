#ifndef EMBEDDER_H
#define EMBEDDER_H

#include <memory>
#include <QFile>
#include <QHash>
#include <QStringList>
#include <QJsonObject>
#include "../generation/EmbeddingGenerator.h"
#include "../db/EmbeddingDatabase.h"
#include "../parsers/CDDAResolver.h"
#include "../parsers/Parser.h"

class Embedder
{
public:
    explicit Embedder(const QJsonObject& config);

    // Embeds query and returns the top-K most similar stored chunks above the similarity threshold.
    auto search(const QString& query) -> QVector<EmbeddingDatabase::SearchResult>;

    [[nodiscard]] auto lastQueryEmbedding() const -> const QVector<float>& { return m_lastQueryEmbedding; }

    [[nodiscard]] auto isValid() const -> bool { return m_isValid; }

private:
    auto processAllFiles() -> void;

    // Returns true if the file was newly indexed, false if already up-to-date.
    auto fileEmbed(QFile& file) -> bool;
    auto fileEmbedManPage(const QString& path) -> bool;

    EmbeddingDatabase                   m_db;
    QStringList                         m_files;
    std::unique_ptr<EmbeddingGenerator> m_generator;
    std::unique_ptr<Parser>             m_parser;
    CDDAResolver::Registry              m_registry;
    QVector<float>                      m_lastQueryEmbedding;
    QString                             m_parserType;
    int                                 m_topK { 10 };
    float                               m_similarityThreshold { 0.0f };
    bool                                m_isValid { false };
};

#endif // EMBEDDER_H
