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
    faiss_IndexFlatL2_new_with(&m_index, m_dimension);
    
    // TODO: On startup, you should ideally load existing embeddings from a 
    // separate BLOB table into the FAISS index to persist state across restarts.
}

//--------------------------------------------------------------------------------
QVector<EmbeddingDatabase::SearchResult> EmbeddingDatabase::search(
        const QVector<float> &queryEmbedding, 
        int topK
    )
{
    if (queryEmbedding.size() != m_dimension || m_index == nullptr) return {};

    // 1. Prepare Query Vector (FAISS expects float*)
    QVector<float> normalizedQuery = queryEmbedding;
    normalizeVector(normalizedQuery.data(), m_dimension);

    // 2. Search FAISS
    QVector<float> distances(topK);
    QVector<long long> labels(topK); // FAISS IDs
    
    faiss_Index_search(m_index, 1, normalizedQuery.constData(), topK, distances.data(), labels.data());

    // 3. Fetch Metadata from SQLite for the specific IDs found
    QVector<SearchResult> results;
    QSqlQuery query(m_db);

    for (int i = 0; i < topK; ++i) {
        long long faissId = labels[i];
        if (faissId < 0) continue; // FAISS returns -1 if not enough results

        query.prepare(R"(
            SELECT c.content, s.source_file 
            FROM chunks c
            JOIN sources s ON c.source_id = s.id
            WHERE c.faiss_id = ?
        )");
        query.addBindValue(faissId);

        if (query.exec() && query.next()) {
            SearchResult res;
            res.content = query.value(0).toString();
            res.sourceFile = query.value(1).toString();
            res.similarity = 1.0f - (distances[i] / 2.0f); // Convert L2 distance to approx similarity
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
    long long currentFaissId = faiss_Index_ntotal(m_index);
    QVector<float> normalizedEmb = embedding;
    normalizeVector(normalizedEmb.data(), m_dimension);

    faiss_Index_add(m_index, 1, normalizedEmb.constData());

    // 3. Save mapping to SQLite
    q.prepare("INSERT INTO chunks (faiss_id, source_id, content) VALUES (?, ?, ?)");
    q.addBindValue(currentFaissId);
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
