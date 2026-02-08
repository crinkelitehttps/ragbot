
#include <algorithm>
#include <cmath>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>

#include "EmbeddingDatabase.h"

//--------------------------------------------------------------------------------
void EmbeddingDatabase::initialize(QString dbName)
{
    qDebug() << "EmbeddingDatabase::EmbeddingDatabase() dbName " << dbName;

    m_db = QSqlDatabase::addDatabase("QSQLITE", "embeddings");
    m_db.setDatabaseName(dbName);
    m_db.open();

    QSqlQuery query(m_db);

    QString createTable = R"(
        CREATE TABLE IF NOT EXISTS embeddings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            source_file TEXT NOT NULL,
            item_id TEXT,
            content TEXT NOT NULL,
            embedding BLOB NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(source_file, item_id)
        )
    )";
    
    if (!query.exec(createTable)) {
        qCritical()
            << "EmbeddingDatase::EmbeddingDatabase(): Failed to create table:"
            << query.lastError().text();
    }
        
    query.exec(
        "CREATE INDEX IF NOT EXISTS idx_source ON embeddings(source_file)"
    );
    
    if (!m_db.open()) {
        qCritical() << "Failed to open embeddings database:"
           << m_db.lastError().text();
    }
}


//--------------------------------------------------------------------------------
QVector<EmbeddingDatabase::SearchResult> EmbeddingDatabase::search(
        const QVector<float> &queryEmbedding,
        int topK
    )
{
    qDebug() << "EmbeddingDatabase::search():" << queryEmbedding.count() << topK;

    QSqlQuery query(m_db);

    query.prepare(
        "SELECT content, source_file, item_id, embedding FROM embeddings"
    );
    
    if (!query.exec()) {
        qCritical() << "EmbeddingDatabase::search(): Query failed:"
           << query.lastError().text();

        return {};
    }
    
    QVector<SearchResult> results;
    
    while (query.next()) {
        QString content = query.value(0).toString();
        QString sourceFile = query.value(1).toString();
        QString itemId = query.value(2).toString();
        QByteArray embBlob = query.value(3).toByteArray();
        
        const float *embData = reinterpret_cast<const float*>(
                embBlob.constData()
        );

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


//--------------------------------------------------------------------------------
int EmbeddingDatabase::count()
{
    qDebug() << "EmbeddingDatabase::count()";
    QSqlQuery query("SELECT COUNT(*) FROM embeddings", m_db);
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
};


//--------------------------------------------------------------------------------
bool EmbeddingDatabase::saveEmbedding(
        const QString &sourceFile,
        const QString &itemId,
        const QString &content,
        const QVector<float> &embedding
    ) 
{
    QSqlQuery query(m_db);

    query.prepare(
        "INSERT OR REPLACE INTO embeddings " 
        "(source_file, item_id, content, embedding) "
        "VALUES (?, ?, ?, ?)"
    );

    query.addBindValue(sourceFile);
    query.addBindValue(itemId);
    query.addBindValue(content.left(5000)); // Truncate very long content
    
    QByteArray embBlob(
            reinterpret_cast<const char*>(embedding.data()), 
            embedding.size() * sizeof(float)
    );

    query.addBindValue(embBlob);
    
    if (!query.exec()) {
        qWarning() << "Failed to save embedding:" << query.lastError().text();
        return false;
    }
    return true;
};


//--------------------------------------------------------------------------------
float EmbeddingDatabase::cosineSimilarity(
        const QVector<float> &a,
        const float *b,
        int size
    )
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
