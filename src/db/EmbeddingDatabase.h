#ifndef EMBEDDINGDATABASE_H
#define EMBEDDINGDATABASE_H

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <sqlite3.h>
#include "VectorIndex.h"

class EmbeddingDatabase
{
public:
    struct SearchResult {
        QString sourceFile;
        QString content;
        float   similarity;
    };

    explicit EmbeddingDatabase(const QJsonObject& embedConfig);
    ~EmbeddingDatabase();

    // Non-copyable: owns a raw sqlite3* handle.
    EmbeddingDatabase(const EmbeddingDatabase&)            = delete;
    EmbeddingDatabase& operator=(const EmbeddingDatabase&) = delete;

    // Returns a new source_file_id for the given file, or -1 if the checksum
    // is already present (file unchanged since last run).
    auto newSourceFileId(const QByteArray& contentChecksum, const QString& file) -> int;

    // Normalizes chunkVector, adds it to the in-memory index, and persists
    // both the vector and its text content to the database.
    auto embeddingSave(
        int sourceId,
        const QVector<float>& chunkVector,
        const QString& chunkContent
    ) -> bool;

    // Returns the top-K most similar stored chunks for the given embedding.
    // The query vector is normalized internally before searching.
    auto textResults(
        const QVector<float>& queryEmbedding,
        int topK = DefaultTopK
    ) -> QVector<SearchResult>;

    [[nodiscard]] auto isOpen() const -> bool { return m_db != nullptr; }

private:
    void initialize(const QString& dbName);
    void loadExistingEmbeddings();

    // Executes a SQL statement and logs any error. Returns true on success.
    auto exec(const char* sql) -> bool;

    // Normalizes vec in-place to unit length. No-op if the norm is zero.
    static auto normalizeVector(QVector<float>& vec) -> void;

    VectorIndex m_index;
    sqlite3*    m_db {};

    // Increment this whenever the table definitions change.
    // A mismatch triggers a full schema rebuild (all data is re-indexed).
    static constexpr int SchemaVersion { 3 };

    static constexpr int Dimensions { 768 };
    static constexpr int DefaultTopK { 10 };
};

#endif // EMBEDDINGDATABASE_H
