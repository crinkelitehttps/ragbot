#include "RoleplayDatabase.h"
#include <QDebug>
#include <cstring>

//--------------------------------------------------------------------------------
RoleplayDatabase::RoleplayDatabase(const QString& dbName)
{
    const int rc = sqlite3_open(dbName.toUtf8().constData(), &m_db);
    if (rc != SQLITE_OK) {
        qCritical() << "RoleplayDatabase: cannot open" << dbName
                    << "—" << sqlite3_errmsg(m_db);
        sqlite3_close(m_db);
        m_db = nullptr;
        return;
    }
    initializeSchema();
}


//--------------------------------------------------------------------------------
RoleplayDatabase::~RoleplayDatabase()
{
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}


//--------------------------------------------------------------------------------
auto RoleplayDatabase::exec(const char* sql) -> bool
{
    char* errmsg = nullptr;
    if (sqlite3_exec(m_db, sql, nullptr, nullptr, &errmsg) != SQLITE_OK) {
        qWarning() << "RoleplayDatabase SQL error:" << errmsg;
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}


//--------------------------------------------------------------------------------
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


//--------------------------------------------------------------------------------
auto RoleplayDatabase::logConversation(
    const QVector<float>& queryEmbedding,
    const QString& query,
    const QString& researchResponse,
    const QString& roleplayResponse
) -> bool
{
    if (!m_db) return false;

    const QByteArray embBlob(
        reinterpret_cast<const char*>(queryEmbedding.constData()),
        queryEmbedding.size() * static_cast<int>(sizeof(float))
    );
    const QByteArray queryUtf8    = query.toUtf8();
    const QByteArray researchUtf8 = researchResponse.toUtf8();
    const QByteArray roleplayUtf8 = roleplayResponse.toUtf8();

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_db,
        "INSERT INTO conversations "
        "(query, query_embedding, research_response, roleplay_response, timestamp) "
        "VALUES (?, ?, ?, ?, datetime('now'))",
        -1, &stmt, nullptr);

    sqlite3_bind_text(stmt, 1, queryUtf8.constData(),    queryUtf8.size(),    SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 2, embBlob.constData(),      embBlob.size(),      SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, researchUtf8.constData(), researchUtf8.size(), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, roleplayUtf8.constData(), roleplayUtf8.size(), SQLITE_STATIC);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        qWarning() << "RoleplayDatabase::logConversation() failed:" << sqlite3_errmsg(m_db);
        return false;
    }

    return true;
}
