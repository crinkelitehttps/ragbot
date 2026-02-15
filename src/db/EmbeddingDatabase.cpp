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

    // TODO add cryptographic has as primary key;
    QString createEmbeddings = R"(
        CREATE TABLE embeddings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            source_id INTEGER NOT NULL,
            content TEXT NOT NULL,
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
bool EmbeddingDatabase::createSourceRecord(
        const QString &sourceFile
    ) 
{


    QSqlQuery check(m_db);

    check.prepare(
        "SELECT * FROM sources WHERE Sha256 = ?;"
    );

    const auto hash = fileChecksum(sourceFile);

    check.addBindValue(hash);
    check.exec();
    
    qDebug() << "EmbedddingDatabase::createSourceRecord()";

    if (check.nextResult()) {
        return false;
    }
    return true;
};


//--------------------------------------------------------------------------------
bool EmbeddingDatabase::saveEmbedding(
        const Generator::ContentEmbedding& contentEmbedding
    )
{
    qDebug() << "EmbeddingDatabase::saveEmbedding() [ contentEmbedding ]"; 
    
    QSqlQuery getSourceId(m_db);
    getSourceId.prepare("SELECT id FROM sources WHERE Sha256 = ?;");
    getSourceId.addBindValue(fileChecksum(contentEmbedding.sourceFile));
    
    if (getSourceId.exec() && getSourceId.next()) {
        qDebug() << "EmbeddingDatabase::saveEmbedding() [ sourceId found ]";
        return true;
    }
    
    int sourceId = getSourceId.value(0).toInt();
    
    QSqlQuery query(m_db);

    query.prepare(
        "INSERT INTO embeddings "
        "(source_id, content, embedding) "
        "VALUES (?, ?, ?);"
    );

    query.addBindValue(sourceId);
    query.addBindValue(contentEmbedding.content);

    QByteArray blob(
        reinterpret_cast<const char*>(
            contentEmbedding.embedding.data()
        ),
       contentEmbedding.embedding.size() * sizeof(float)
    );

    query.addBindValue(blob);
    
    if (!query.exec()) {
        qWarning() << "EmbeddingDatabase::saveEmbedding() [ failed ]";
        qDebug() << query.lastError();
        return false;
    }
    
    return true;
}


#if 0
//--------------------------------------------------------------------------------
bool EmbeddingDatabase::saveEmbedding(
        const Generator::ContentEmbedding& contentEmbedding
    )
{
    qDebug() << "EmbeddingDatabase::saveEmbedding() [ contentEmbedding ]"; 
    QSqlQuery query(m_db);

    query.prepare(
        "INSERT INTO embeddings"
        "(content, embedding) "
        "VALUES (?,?)"
    );

    query.addBindValue(contentEmbedding.content);

    QByteArray blob(reinterpret_cast<const char*>(contentEmbedding.embedding.data()), 
                    contentEmbedding.embedding.size() * sizeof(float));

    query.addBindValue(blob);
    if (!query.exec()) {
        qWarning() << "EmbeddingDatabase::saveEmbedding() [ failed ]";
        qDebug() << m_db.lastError();
        return false;
    };
    return true;
};
#endif

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


