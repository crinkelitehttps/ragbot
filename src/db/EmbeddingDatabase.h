#ifndef EMBEDDINGDATABASE_H
#define EMBEDDINGDATABASE_H

#include "../compat/Json.h"
#include "../compat/Types.h"
#include <sqlite3.h>
#include "VectorIndex.h"

class EmbeddingDatabase
{
public:
    struct SearchResult {
        rb::String sourceFile;
        rb::String content;
        float   similarity;
        float   rerankScore { -1.0f }; // set by Reranker; negative means not reranked
    };

    explicit EmbeddingDatabase(const rb::Json& embedConfig);
    ~EmbeddingDatabase();

    EmbeddingDatabase(const EmbeddingDatabase&)            = delete;
    EmbeddingDatabase& operator=(const EmbeddingDatabase&) = delete;

    // Returns a new source_file_id or -1 if checksum already indexed.
    auto newSourceFileId(const rb::String& contentChecksum, const rb::String& file) -> int;

    // Read-only check: true if a source row with this checksum already exists.
    [[nodiscard]] auto sourceFileExists(const rb::String& contentChecksum) -> bool;

    auto embeddingSave(
        int sourceId,
        const rb::Vector<float>& chunkVector,
        const rb::String& chunkContent
    ) -> bool;

    auto textResults(
        const rb::Vector<float>& queryEmbedding,
        int   topK          = DefaultTopK,
        float minSimilarity = 0.0f
    ) -> rb::Vector<SearchResult>;

    auto beginBatch()    -> bool;
    auto commitBatch()   -> bool;
    auto beginFileTransaction()    -> bool;
    auto commitFileTransaction()   -> bool;
    auto rollbackFileTransaction() -> void;

    [[nodiscard]] auto isOpen() const -> bool { return m_db != nullptr; }

private:
    void initialize(const rb::String& dbName);
    void loadExistingEmbeddings();
    auto exec(const char* sql) -> bool;
    static auto normalizeVector(rb::Vector<float>& vec) -> void;

    VectorIndex m_index;
    sqlite3*    m_db {};
    int64_t     m_txIndexSnapshot { -1 };

    static constexpr int SchemaVersion { 5 };
    static constexpr int Dimensions { 768 };
    static constexpr int DefaultTopK { 10 };
};

#endif // EMBEDDINGDATABASE_H
