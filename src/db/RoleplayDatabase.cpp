#include "RoleplayDatabase.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

RoleplayDatabase::RoleplayDatabase(const QString &dbName)
{
    m_db = QSqlDatabase::addDatabase("QSQLITE", "conversations");
    m_db.setDatabaseName(dbName);
    
    if (!m_db.open()) {
        qCritical() << "Failed to open conversations database:" << m_db.lastError().text();
        return;
    }
    
    initializeSchema();
}

bool RoleplayDatabase::logConversation(const QVector<float> &queryEmbedding,
                    const QString &query,
                    const QString &researchResponse,
                    const QString &roleplayResponse)
{
    QSqlQuery insertQuery(m_db);
    insertQuery.prepare(
        "INSERT INTO conversations (query, query_embedding, research_response, roleplay_response, timestamp) "
        "VALUES (:query, :query_embedding, :research_response, :roleplay_response, datetime('now'))"
    );
    
    // Convert embedding to binary blob
    QByteArray embBlob(reinterpret_cast<const char*>(queryEmbedding.constData()),
                      queryEmbedding.size() * sizeof(float));
    
    insertQuery.addBindValue(query);
    insertQuery.addBindValue(embBlob);
    insertQuery.addBindValue(researchResponse);
    insertQuery.addBindValue(roleplayResponse);
    
    if (!insertQuery.exec()) {
        qWarning() << "Failed to log conversation:" << insertQuery.lastError().text();
        return false;
    }
    
    return true;
}

bool RoleplayDatabase::isInitialized() const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT name FROM sqlite_master WHERE type='table' AND name='conversations'");
    return query.exec() && query.next();
}

void RoleplayDatabase::initializeSchema()
{
    QSqlQuery query(m_db);
    
    // Create conversations table if it doesn't exist
    if (!query.exec(
        "CREATE TABLE IF NOT EXISTS conversations ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "query TEXT NOT NULL,"
        "query_embedding BLOB,"
        "research_response TEXT,"
        "roleplay_response TEXT,"
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")"
    )) {
        qCritical() << "Failed to create conversations table:" << query.lastError().text();
    }
    
    // Create index on timestamp for efficient queries
    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_timestamp ON conversations(timestamp)")) {
        qCritical() << "Failed to create timestamp index:" << query.lastError().text();
    }
}
