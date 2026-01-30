
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <cmath>
class ConversationDatabase
{
public:
    ConversationDatabase(const QString &dbName = "conversations.db")
    {
        m_db = QSqlDatabase::addDatabase("QSQLITE", "conversations");
        m_db.setDatabaseName(dbName);
        
        if (!m_db.open()) {
            qCritical() << "Failed to open conversations database:" << m_db.lastError().text();
            return;
        }
        
        initializeSchema();
    }
    
    bool logConversation(const QVector<float> &queryEmbedding,
                        const QString &query,
                        const QString &researchResponse,
                        const QString &roleplayResponse = "")
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
    
    bool isInitialized() const
    {
        QSqlQuery query(m_db);
        query.prepare("SELECT name FROM sqlite_master WHERE type='table' AND name='conversations'");
        return query.exec() && query.next();
    }

private:
    void initializeSchema()
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

    QSqlDatabase m_db;
};

class EmbeddingDatabase
{
public:
    EmbeddingDatabase(const QString &dbName = "embeddings.db")
    {
        m_db = QSqlDatabase::addDatabase("QSQLITE", "embeddings");
        m_db.setDatabaseName(dbName);
        
        if (!m_db.open()) {
            qCritical() << "Failed to open embeddings database:" << m_db.lastError().text();
        }
    }
    
    struct SearchResult {
        QString content;
        QString sourceFile;
        QString itemId;
        float similarity;
    };
    
    QVector<SearchResult> search(const QVector<float> &queryEmbedding, int topK = 10)
    {
        QSqlQuery query(m_db);
        query.prepare("SELECT content, source_file, item_id, embedding FROM embeddings");
        
        if (!query.exec()) {
            qCritical() << "Query failed:" << query.lastError().text();
            return {};
        }
        
        QVector<SearchResult> results;
        
        while (query.next()) {
            QString content = query.value(0).toString();
            QString sourceFile = query.value(1).toString();
            QString itemId = query.value(2).toString();
            QByteArray embBlob = query.value(3).toByteArray();
            
            const float *embData = reinterpret_cast<const float*>(embBlob.constData());
            int embSize = embBlob.size() / sizeof(float);
            
            if (embSize != queryEmbedding.size()) continue;
            
            float similarity = cosineSimilarity(queryEmbedding, embData, embSize);
            
            SearchResult result;
            result.content = content;
            result.sourceFile = sourceFile;
            result.itemId = itemId;
            result.similarity = similarity;
            results.append(result);
        }
        
        std::sort(results.begin(), results.end(), 
                 [](const SearchResult &a, const SearchResult &b) {
                     return a.similarity > b.similarity;
                 });
        
        if (results.size() > topK) {
            results.resize(topK);
        }
        
        return results;
    }

private:
    float cosineSimilarity(const QVector<float> &a, const float *b, int size)
    {
        float dotProduct = 0.0f;
        float normA = 0.0f;
        float normB = 0.0f;
        
        for (int i = 0; i < size; i++) {
            dotProduct += a[i] * b[i];
            normA += a[i] * a[i];
            normB += b[i] * b[i];
        }
        
        if (normA == 0.0f || normB == 0.0f) return 0.0f;
        
        return dotProduct / (std::sqrt(normA) * std::sqrt(normB));
    }

    QSqlDatabase m_db;
};
