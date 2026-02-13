#include <cmath>
#include <algorithm>
#include <QDebug>
#include <QFile>
#include <QCryptographicHash>
#include <QSqlQuery>
#include <QSqlError>

#include "EmbeddingDatabase.h"

//--------------------------------------------------------------------------------
void EmbeddingDatabase::initialize(const QString& dbName)
{
    qDebug() << "EmbeddingDatabase::EmbeddingDatabase() dbName " << dbName;

    m_db = QSqlDatabase::addDatabase("QSQLITE", "embeddings");
    m_db.setDatabaseName(dbName);
    m_db.open();

    QSqlQuery query(m_db);

    QString createSources = R"(
        CREATE TABLE sources (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        Sha256 TEXT UNIQUE NOT NULL,
        source_file TEXT NOT NULL,
        helper_context TEXT,
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)
    )";

    if (!query.exec(createSources)) {
        qCritical()
            << "EmbeddingDatase::EmbeddingDatabase(): Failed to create sources table:"
            << query.lastError().text();
    }

    query.exec(
        "CREATE INDEX IF NOT EXISTS idx_source ON embeddings(source_file)"
    );
    if (!m_db.open()) {
        qCritical() << "Failed to open embeddings database:"
           << m_db.lastError().text();
    }

    // TODO add cryptographic has as primary key;
    QString createEmbeddings = R"(
        CREATE TABLE embeddings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            item_id TEXT,
            content TEXT NOT NULL,
            helper_context TEXT,
            source_id INTEGER NOT NULL,
            FOREIGN KEY (source_id) REFERENCES sources (id)
        )
    )";
    
    if (!query.exec(createEmbeddings)) {
        qCritical()
            << "EmbeddingDatase::EmbeddingDatabase(): Failed to create embedding table:"
            << query.lastError().text();
    }
        
    query.exec(
        "CREATE INDEX IF NOT EXISTS idx_source ON embeddings(source_file)"
    );
    
}


//--------------------------------------------------------------------------------
const QVector<EmbeddingDatabase::SearchResult> EmbeddingDatabase::search(
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
const bool EmbeddingDatabase::createSourceRecord(
        const QString &sourceFile
    ) 
{
    QSqlQuery query(m_db);

    query.prepare(
        "INSERT INTO sources"
        "(Sha256, source_file) "
        "VALUES (?,?)"
    );

    query.addBindValue(fileChecksum(sourceFile));
    query.addBindValue(sourceFile);

    if(query.exec()) {
        qDebug() << "EmbeddingDatase::saveEmbedding() [ new file ]";
        return true;
    };
    return false;
};


//--------------------------------------------------------------------------------
const bool EmbeddingDatabase::saveEmebeding(const QVector<float>& embedding) 
{
    return true;
};


//--------------------------------------------------------------------------------
const QByteArray EmbeddingDatabase::fileChecksum(const QString &fileName) {
    QFile f(fileName);
    if (f.open(QFile::ReadOnly)) {
        QCryptographicHash hash(QCryptographicHash::Algorithm::Md5);
        if (hash.addData(&f)) {
            return hash.result().toHex();
        }
    }
    return QByteArray();
};


//--------------------------------------------------------------------------------
const float EmbeddingDatabase::cosineSimilarity(
        const QVector<float> &a,
        const float *b,
        int size
    )
{
// TODO move to Embedder
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


