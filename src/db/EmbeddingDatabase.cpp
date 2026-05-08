#include "EmbeddingDatabase.h"
#include "../ConfigKeys.h"
#include "../compat/Logging.h"
#include <cmath>
#include <cstring>


auto EmbeddingDatabase::normalizeVector(rb::Vector<float>& vec) -> void
{
    float norm = 0.0f;
    for (float v : vec) norm += v * v;
    if (norm <= 0.0f) return;
    norm = std::sqrt(norm);
    for (float& v : vec) v /= norm;
}


EmbeddingDatabase::EmbeddingDatabase(const rb::Json& embedConfig)
    : m_index(Dimensions)
{
    const rb::String dbName = embedConfig.stringValue(ConfigKeys::DbName, "embeddings.db");
    initialize(dbName);
}


EmbeddingDatabase::~EmbeddingDatabase()
{
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}


auto EmbeddingDatabase::exec(const char* sql) -> bool
{
    char* errmsg = nullptr;
    if (sqlite3_exec(m_db, sql, nullptr, nullptr, &errmsg) != SQLITE_OK) {
        RAGBOT_LOG_WARN("EmbeddingDatabase SQL error: {}", errmsg);
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}


void EmbeddingDatabase::initialize(const rb::String& dbName)
{
    const int rc = sqlite3_open(dbName.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        RAGBOT_LOG_ERROR("EmbeddingDatabase: cannot open {} — {}",
                         rb::to_std(dbName), sqlite3_errmsg(m_db));
        sqlite3_close(m_db);
        m_db = nullptr;
        return;
    }

    exec("PRAGMA foreign_keys = ON;");
    exec("PRAGMA journal_mode = WAL;");

    exec("CREATE TABLE IF NOT EXISTS schema_version (version INTEGER NOT NULL);");

    int storedVersion = 0;
    {
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, "SELECT version FROM schema_version LIMIT 1", -1, &stmt, nullptr);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            storedVersion = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    if (storedVersion != SchemaVersion) {
        if (storedVersion != 0) {
            RAGBOT_LOG_WARN("EmbeddingDatabase: schema version {} → {} — all data will be re-indexed",
                            storedVersion, SchemaVersion);
        }
        exec("DROP TABLE IF EXISTS chunks;");
        exec("DROP TABLE IF EXISTS sources;");
        exec("DELETE FROM schema_version;");

        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, "INSERT INTO schema_version (version) VALUES (?)", -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, SchemaVersion);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    exec(R"(
        CREATE TABLE IF NOT EXISTS sources (
            source_file_id   INTEGER PRIMARY KEY AUTOINCREMENT,
            sha256           TEXT    UNIQUE NOT NULL,
            source_file_path TEXT    NOT NULL,
            created_at       TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
    )");
    exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_sources_sha256 ON sources(sha256);");
    exec(R"(
        CREATE TABLE IF NOT EXISTS chunks (
            faiss_id  INTEGER PRIMARY KEY,
            source_id INTEGER NOT NULL,
            content   TEXT    NOT NULL,
            embedding BLOB    NOT NULL,
            FOREIGN KEY (source_id) REFERENCES sources(source_file_id)
                ON DELETE CASCADE
        );
    )");

    loadExistingEmbeddings();
}


void EmbeddingDatabase::loadExistingEmbeddings()
{
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_db,
        "SELECT faiss_id, embedding FROM chunks ORDER BY faiss_id ASC",
        -1, &stmt, nullptr);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const int64_t id    = sqlite3_column_int64(stmt, 0);
        const int     bytes = sqlite3_column_bytes(stmt, 1);
        const void*   blob  = sqlite3_column_blob(stmt, 1);

        if (bytes / static_cast<int>(sizeof(float)) != Dimensions) {
            RAGBOT_LOG_WARN("EmbeddingDatabase::loadExistingEmbeddings(): malformed embedding at id {}", id);
            continue;
        }

        rb::Vector<float> vec(static_cast<size_t>(Dimensions));
        std::memcpy(vec.data(), blob, static_cast<size_t>(bytes));
        m_index.load(id, vec);
        ++count;
    }
    sqlite3_finalize(stmt);

    RAGBOT_LOG_INFO("EmbeddingDatabase: warm-start loaded {} embeddings", count);
}


auto EmbeddingDatabase::newSourceFileId(
    const rb::String& contentChecksum,
    const rb::String& file
) -> int
{
    if (!m_db) return -1;

    const std::string& checksumStd = contentChecksum;
    const std::string& fileStd     = file;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_db,
        "SELECT source_file_id FROM sources WHERE sha256 = ? LIMIT 1",
        -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, checksumStd.data(), static_cast<int>(checksumStd.size()), SQLITE_STATIC);

    const bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);

    if (exists) {
        RAGBOT_LOG_INFO("EmbeddingDatabase::newSourceFileId(): already indexed — {}", rb::to_std(file));
        return -1;
    }

    sqlite3_prepare_v2(m_db,
        "INSERT INTO sources (sha256, source_file_path) VALUES (?, ?)",
        -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, checksumStd.data(), static_cast<int>(checksumStd.size()), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, fileStd.data(),     static_cast<int>(fileStd.size()),     SQLITE_STATIC);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        RAGBOT_LOG_WARN("EmbeddingDatabase::newSourceFileId(): insert failed — {}", sqlite3_errmsg(m_db));
        return -1;
    }
    return static_cast<int>(sqlite3_last_insert_rowid(m_db));
}


auto EmbeddingDatabase::sourceFileExists(const rb::String& contentChecksum) -> bool
{
    if (!m_db) return false;

    const std::string& checksumStd = contentChecksum;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_db,
        "SELECT 1 FROM sources WHERE sha256 = ? LIMIT 1",
        -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, checksumStd.data(), static_cast<int>(checksumStd.size()), SQLITE_STATIC);

    const bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}


auto EmbeddingDatabase::embeddingSave(
    int sourceId,
    const rb::Vector<float>& chunkVector,
    const rb::String& chunkContent
) -> bool
{
    if (!m_db || static_cast<int>(chunkVector.size()) != Dimensions) {
        RAGBOT_LOG_WARN("EmbeddingDatabase::embeddingSave(): precondition failed");
        return false;
    }

    rb::Vector<float> normalized = chunkVector;
    normalizeVector(normalized);

    const int64_t vectorId = m_index.add(normalized);
    if (vectorId < 0) {
        RAGBOT_LOG_WARN("EmbeddingDatabase::embeddingSave(): vector index rejected the vector");
        return false;
    }

    const int blobSize = static_cast<int>(normalized.size()) * static_cast<int>(sizeof(float));

    const std::string& contentStd = chunkContent;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_db,
        "INSERT INTO chunks (faiss_id, source_id, content, embedding) VALUES (?, ?, ?, ?)",
        -1, &stmt, nullptr);

    sqlite3_bind_int64(stmt, 1, vectorId);
    sqlite3_bind_int  (stmt, 2, sourceId);
    sqlite3_bind_text (stmt, 3, contentStd.data(), static_cast<int>(contentStd.size()), SQLITE_STATIC);
    sqlite3_bind_blob (stmt, 4, normalized.data(),  blobSize,                            SQLITE_STATIC);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        RAGBOT_LOG_WARN("EmbeddingDatabase::embeddingSave(): {}", sqlite3_errmsg(m_db));
        return false;
    }
    return true;
}


auto EmbeddingDatabase::beginBatch()  -> bool { return exec("BEGIN;"); }
auto EmbeddingDatabase::commitBatch() -> bool { return exec("COMMIT;"); }

auto EmbeddingDatabase::beginFileTransaction() -> bool
{
    m_txIndexSnapshot = m_index.ntotal();
    return exec("SAVEPOINT file_tx;");
}

auto EmbeddingDatabase::commitFileTransaction() -> bool
{
    m_txIndexSnapshot = -1;
    return exec("RELEASE SAVEPOINT file_tx;");
}

auto EmbeddingDatabase::rollbackFileTransaction() -> void
{
    exec("ROLLBACK TO SAVEPOINT file_tx;");
    exec("RELEASE SAVEPOINT file_tx;");
    if (m_txIndexSnapshot >= 0)
        m_index.rollbackTo(m_txIndexSnapshot);
    m_txIndexSnapshot = -1;
}


auto EmbeddingDatabase::textResults(
    const rb::Vector<float>& queryEmbedding,
    int   topK,
    float minSimilarity
) -> rb::Vector<SearchResult>
{
    if (!m_db || static_cast<int>(queryEmbedding.size()) != Dimensions) return {};

    rb::Vector<float> normalizedQuery = queryEmbedding;
    normalizeVector(normalizedQuery);

    const rb::Vector<VectorIndex::Hit> hits = m_index.search(normalizedQuery, topK);
    if (hits.empty()) return {};

    rb::Vector<VectorIndex::Hit> filteredHits;
    filteredHits.reserve(hits.size());
    for (const auto& hit : hits)
        if (hit.similarity >= minSimilarity)
            filteredHits.push_back(hit);
    if (filteredHits.empty()) return {};

    rb::Vector<SearchResult> results;
    results.reserve(filteredHits.size());

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_db,
        "SELECT c.content, s.source_file_path "
        "FROM   chunks  c "
        "JOIN   sources s ON c.source_id = s.source_file_id "
        "WHERE  c.faiss_id = ?",
        -1, &stmt, nullptr);

    for (const auto& hit : filteredHits) {
        sqlite3_reset(stmt);
        sqlite3_bind_int64(stmt, 1, hit.id);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const auto* content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const auto* source  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            results.push_back({
                rb::from_std(source),
                rb::from_std(content),
                hit.similarity
            });
        }
    }

    sqlite3_finalize(stmt);
    return results;
}
