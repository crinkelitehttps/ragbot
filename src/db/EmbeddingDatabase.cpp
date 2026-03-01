#include "EmbeddingDatabase.h"
#include <cmath>
#include <algorithm>
#include <QDebug>
#include <QFile>
#include <QCryptographicHash>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

// Helper to normalize vectors for Cosine Similarity using L2 Index
void normalizeVector(float* data, int dim) {
    float norm = 0.0f;
    for (int i = 0; i < dim; i++) norm += data[i] * data[i];
    if (norm <= 0) return;
    norm = std::sqrt(norm);
    for (int i = 0; i < dim; i++) data[i] /= norm;
}

//--------------------------------------------------------------------------------
void EmbeddingDatabase::initialize(const QString& dbName)
{
    m_db = QSqlDatabase::addDatabase("QSQLITE", "embeddings_connection");
    m_db.setDatabaseName(dbName);

    if (!m_db.open()) {
        qCritical() << "Failed to open database:" << m_db.lastError().text();
        return;
    }

    QSqlQuery query(m_db);

    // Table for files/sources
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS sources (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            Sha256 TEXT UNIQUE NOT NULL,
            source_file TEXT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
    )");

    // Table for chunks (maps FAISS index ID to text)
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS chunks (
            faiss_id INTEGER PRIMARY KEY,
            source_id INTEGER NOT NULL,
            content TEXT NOT NULL,
            FOREIGN KEY (source_id) REFERENCES sources(id)
        );
    )");

    // Initialize FAISS Index (Flat L2)
    // Note: If m_dimension is 384 or 768, ensure it's set in the constructor/header
    FaissIndex* rawIndex = nullptr;
    int errorCode = faiss_IndexFlatL2_new_with(&rawIndex, m_dimension);

    if (errorCode) {
        qCritical() << "FAISS Error: Could not create index. Code:" << errorCode;
    }
    
}


//--------------------------------------------------------------------------------
QVector<EmbeddingDatabase::SearchResult> EmbeddingDatabase::search(
        const QVector<float> &queryEmbedding, 
        int topK
    )
{
    if (queryEmbedding.size() != m_dimension || m_index == nullptr) return {};

    // 1. Prepare Query Vector
    QVector<float> normalizedQuery = queryEmbedding;
    normalizeVector(normalizedQuery.data(), m_dimension);

    // 2. Search FAISS
    QVector<float> distances(topK);
    
    // FIX: Use idx_t instead of long long to match the C API signature
    QVector<idx_t> labels(topK); 
    
    // Pass the pointer to distances and labels
    faiss_Index_search(m_index.get(), 1, normalizedQuery.constData(), topK, distances.data(), labels.data());

    // 3. Fetch Metadata
    QVector<SearchResult> results;
    QSqlQuery query(m_db);

    for (int i = 0; i < topK; ++i) {
        idx_t faissId = labels[i]; // idx_t handles the ID
        if (faissId < 0) continue; 

        query.prepare(R"(
            SELECT c.content, s.source_file 
            FROM chunks c
            JOIN sources s ON c.source_id = s.id
            WHERE c.faiss_id = ?
        )");
        
        // QVariant will handle the conversion from idx_t (long) to SQL integer
        query.addBindValue(static_cast<qlonglong>(faissId));

        if (query.exec() && query.next()) {
            SearchResult res;
            res.content = query.value(0).toString();
            res.sourceFile = query.value(1).toString();
            // L2 to Cosine approx: 1 - (d^2 / 2)
            res.similarity = 1.0f - (distances[i] / 2.0f); 
            results.append(res);
        }
    }

    return results;
}


//--------------------------------------------------------------------------------
bool EmbeddingDatabase::saveEmbedding(
    const QVector<float> &embedding,
    const QString &sourcePath,
    const QString &textContent)
{
    if (embedding.size() != m_dimension) return false;

    // 1. Ensure Source exists or Get ID
    const QByteArray fcs = fileChecksum(sourcePath);
    int sourceId = -1;

    QSqlQuery q(m_db);
    q.prepare("SELECT id FROM sources WHERE Sha256 = ?");
    q.addBindValue(fcs);
    
    if (q.exec() && q.next()) {
        sourceId = q.value(0).toInt();
    } else {
        q.prepare("INSERT INTO sources (Sha256, source_file) VALUES (?, ?)");
        q.addBindValue(fcs);
        q.addBindValue(sourcePath);
        if (!q.exec()) return false;
        sourceId = q.lastInsertId().toInt();
    }

    // 2. Add to FAISS Index
    auto currentFaissId = faiss_Index_ntotal(m_index.get());
    QVector<float> normalizedEmb = embedding;
    normalizeVector(normalizedEmb.data(), m_dimension);

    faiss_Index_add(m_index.get(), 1, normalizedEmb.constData());

    // 3. Save mapping to SQLite
    q.prepare("INSERT INTO chunks (faiss_id, source_id, content) VALUES (?, ?, ?)");
    q.addBindValue(static_cast<qlonglong>(currentFaissId));
    q.addBindValue(sourceId);
    q.addBindValue(textContent);

    return q.exec();
}

//--------------------------------------------------------------------------------
QByteArray EmbeddingDatabase::fileChecksum(const QString &fileName) {
    QFile f(fileName);
    if (!f.open(QFile::ReadOnly)) return QByteArray();
    
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (hash.addData(&f)) return hash.result().toHex();
    
    return QByteArray();
}

//--------------------------------------------------------------------------------
bool EmbeddingDatabase::isEmbedded(const QString& sourceFile)
{
    QSqlQuery check(m_db);
    check.prepare("SELECT 1 FROM sources WHERE Sha256 = ? LIMIT 1");
    check.addBindValue(fileChecksum(sourceFile));
    
    return check.exec() && check.next();
}
