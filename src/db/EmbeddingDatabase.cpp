#include <cmath>
#include <algorithm>
#include <QDebug>
#include <QFile>
#include <QCryptographicHash>
#include <QSqlQuery>
#include <QSqlError>
#include <QByteArray>

#include "EmbeddingDatabase.h"

//--------------------------------------------------------------------------------
void EmbeddingDatabase::initialize(const QString& dbName)
{

    m_db = QSqlDatabase::addDatabase("QSQLITE", "embeddings");
    m_db.setDatabaseName(dbName);
    m_db.open();

    QSqlQuery query(m_db);

    if (!m_db.open()) {
        qCritical() << "Failed to open embeddings database:"
           << m_db.lastError().text();
    }

    QString createSources = R"(
        CREATE TABLE sources (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            Sha256 TEXT UNIQUE NOT NULL,
            source_file TEXT NOT NULL,
            helper_context TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";

    if (!query.exec(createSources)) {
        qCritical()
            << "EmbeddingDatase::EmbeddingDatabase(): Failed to create sources table:"
            << query.lastError().text();
    }

    QString createEmbeddings = R"(
        CREATE TABLE embeddings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            source_id INTEGER NOT NULL,
            embedding BLOB NOT NULL,
            helper_context TEXT,
            FOREIGN KEY (source_id) REFERENCES sources(id)
        )
    )";
    
    if (!query.exec(createEmbeddings)) {
        qCritical()
            << "EmbeddingDatase::EmbeddingDatabase(): Failed to create embedding table:"
            << query.lastError().text();
    }
    
}


//--------------------------------------------------------------------------------
QVector<EmbeddingDatabase::SearchResult> EmbeddingDatabase::search(
        const QVector<float> &queryEmbedding,
        int topK
    )
{

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
bool EmbeddingDatabase::saveEmbedding(
    const QVector<float> embedding,
    const QString& sourcePath)
{
    const auto fcs = fileChecksum(sourcePath).trimmed();

    QSqlQuery getSourceId(m_db);
    getSourceId.prepare("SELECT id FROM sources WHERE Sha256 = ?;");
    getSourceId.addBindValue(fcs);

    if (!getSourceId.exec()) {
        qWarning() << getSourceId.lastError();
        return false;
    }

    if (getSourceId.nextResult()) {
        return true;
    }

    QSqlQuery insertSource(m_db);
    insertSource.prepare(
        "INSERT INTO sources (Sha256, source_file) VALUES (?, ?);"
    );

    insertSource.addBindValue(fcs);
    insertSource.addBindValue(sourcePath);

    if (!insertSource.exec()) {
        qWarning() << insertSource.lastError();
        return false;
    }

    int sourceId = insertSource.lastInsertId().toInt();

    qDebug() << "sourceId =" << sourceId;

    QSqlQuery query(m_db);
    query.prepare(
        "INSERT INTO embeddings (source_id, embedding) VALUES (?, ?);"
    );

    query.addBindValue(sourceId);

    QByteArray blob(
        reinterpret_cast<const char*>(embedding.constData()),
        embedding.size() * sizeof(float)
    );

    query.addBindValue(blob);

    if (!query.exec()) {
        qWarning() << query.lastError();
        return false;
    }

    return true;
}


//--------------------------------------------------------------------------------
QByteArray EmbeddingDatabase::fileChecksum(const QString &fileName) {
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


//--------------------------------------------------------------------------------
bool EmbeddingDatabase::isEmbedded(const QString& sourceFile)
{
    QSqlQuery check(m_db);
    check.prepare("SELECT * FROM sources WHERE Sha256 = ?;");
    check.addBindValue(fileChecksum(sourceFile));
    check.exec();
    const auto result = check.nextResult();
    return result;
};


