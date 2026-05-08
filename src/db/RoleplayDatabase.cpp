#include "RoleplayDatabase.h"
#include "../compat/Logging.h"
#include <cstring>

RoleplayDatabase::RoleplayDatabase(const rb::String& dbName)
{
    const int rc = sqlite3_open(dbName.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        RAGBOT_LOG_ERROR("RoleplayDatabase: cannot open {} — {}",
                         rb::to_std(dbName), sqlite3_errmsg(m_db));
        sqlite3_close(m_db);
        m_db = nullptr;
        return;
    }
    initializeSchema();
}

RoleplayDatabase::~RoleplayDatabase()
{
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

auto RoleplayDatabase::exec(const char* sql) -> bool
{
    char* errmsg = nullptr;
    if (sqlite3_exec(m_db, sql, nullptr, nullptr, &errmsg) != SQLITE_OK) {
        RAGBOT_LOG_WARN("RoleplayDatabase SQL error: {}", errmsg);
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

void RoleplayDatabase::initializeSchema()
{
    exec(R"(
        CREATE TABLE IF NOT EXISTS conversations (
            id                INTEGER PRIMARY KEY AUTOINCREMENT,
            query             TEXT    NOT NULL,
            query_embedding   BLOB,
            research_response TEXT,
            roleplay_response TEXT,
            timestamp         DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )");
    exec("CREATE INDEX IF NOT EXISTS idx_timestamp ON conversations(timestamp);");
}

auto RoleplayDatabase::logConversation(
    const rb::Vector<float>& queryEmbedding,
    const rb::String& query,
    const rb::String& researchResponse,
    const rb::String& roleplayResponse
) -> bool
{
    if (!m_db) return false;

    const int blobSize = static_cast<int>(queryEmbedding.size() * sizeof(float));

    const std::string& queryStd    = query;
    const std::string& researchStd = researchResponse;
    const std::string& roleplayStd = roleplayResponse;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_db,
        "INSERT INTO conversations "
        "(query, query_embedding, research_response, roleplay_response, timestamp) "
        "VALUES (?, ?, ?, ?, datetime('now'))",
        -1, &stmt, nullptr);

    sqlite3_bind_text(stmt, 1, queryStd.data(),    static_cast<int>(queryStd.size()),    SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 2, queryEmbedding.data(), blobSize,                           SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, researchStd.data(), static_cast<int>(researchStd.size()), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, roleplayStd.data(), static_cast<int>(roleplayStd.size()), SQLITE_STATIC);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        RAGBOT_LOG_WARN("RoleplayDatabase::logConversation() failed: {}", sqlite3_errmsg(m_db));
        return false;
    }
    return true;
}
