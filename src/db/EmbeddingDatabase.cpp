#include "EmbeddingDatabase.h"
#include <cmath>
#include <QDebug>
#include <QFile>
#include <QCryptographicHash>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

// Helper to normalize vectors for Cosine Similarity using L2 Index
void normalizeVector(float* data, int dim) {
    float norm = 0.0F;
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

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS sources (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            Sha256 TEXT UNIQUE NOT NULL,
            source_file TEXT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
    )");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS chunks (
            faiss_id INTEGER PRIMARY KEY,
            source_id INTEGER NOT NULL,
            content TEXT NOT NULL,
            FOREIGN KEY (source_id) REFERENCES sources(id)
        );
    )");

    FaissIndex* rawIndex = nullptr;

    // 0 means success in FAISS C API
    if (faiss_IndexFlatL2_new_with(&rawIndex, m_dimensions) == 0) {
        m_index.reset(rawIndex);
        loadExistingEmbeddings();
    } else {
        qCritical() << "Failed to create FAISS index!";
    }
    
}


//--------------------------------------------------------------------------------
 auto EmbeddingDatabase::textResults(
        const QVector<float> &queryEmbedding, 
        int topK
    ) -> QVector<EmbeddingDatabase::SearchResult>
{
    if (queryEmbedding.size() != m_dimensions || m_index == nullptr) return {};

    // 1. Prepare Query Vector
    QVector<float> normalizedQuery = queryEmbedding;
    normalizeVector(normalizedQuery.data(), m_dimensions);

    // 2. Search FAISS
    QVector<float> distances(topK);
    
    // FIX: Use idx_t instead of long long to match the C API signature
    QVector<idx_t> labels(topK); 
    
    // Pass the pointer to distances and labels
    faiss_Index_search(
        m_index.get(),
        1,
        normalizedQuery.constData(),
        topK,
        distances.data(),
        labels.data()
    );

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
#if DEBUG_DISABLE
            res.content = query.value(0).toString();
#endif
            res.sourceFile = query.value(1).toString();
            // L2 to Cosine approx: 1 - (d^2 / 2)
            res.similarity = 1.0F - (distances[i] / 2.0F); 
            results.append(res);
        }
    }

    return results;
}


//--------------------------------------------------------------------------------
auto EmbeddingDatabase::saveEmbedding(
    const QVector<float> &embedding,
    const SourcePath &sourcePath,
    const HelperContext &helperContext
) -> bool
{
    if (embedding.size() != m_dimensions) return false;

    const QByteArray fcs = fileChecksum(sourcePath.value);
    int sourceId = -1;

    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM sources WHERE Sha256 = ?");
    query.addBindValue(fcs);
    
    if (query.exec() && query.next()) {
        sourceId = query.value(0).toInt();
    } else {
        query.prepare("INSERT INTO sources (Sha256, source_file) VALUES (?, ?)");
        query.addBindValue(fcs);
        query.addBindValue(sourcePath.value);
        if (!query.exec()) return false;
        sourceId = query.lastInsertId().toInt();
    }

    auto currentFaissId = faiss_Index_ntotal(m_index.get());
    QVector<float> normalizedEmb = embedding;
    normalizeVector(normalizedEmb.data(), m_dimensions);

    faiss_Index_add(m_index.get(), 1, normalizedEmb.constData());

    query.prepare("INSERT INTO chunks (faiss_id, source_id, content) VALUES (?, ?, ?)");
    query.addBindValue(static_cast<qlonglong>(currentFaissId));
    query.addBindValue(sourceId);
    query.addBindValue(helperContext.value);

    return query.exec();
}


//--------------------------------------------------------------------------------
void EmbeddingDatabase::loadExistingEmbeddings()
{
    if (!m_index) return;

    QSqlQuery query(m_db);
    // We only need the embeddings to populate the FAISS index.
    // We assume the faiss_id in the DB matches the order/count of the index.
    query.prepare("SELECT embedding FROM embeddings ORDER BY id ASC");

    if (!query.exec()) {
        qCritical() << "Warm Start failed:" << query.lastError().text();
        return;
    }

    int count = 0;
    while (query.next()) {
        QByteArray bytes = query.value(0).toByteArray();
        const auto* data = reinterpret_cast<const float*>(bytes.constData());
        auto numElements = bytes.size() / sizeof(float);

        if (numElements == m_dimensions) {
            // We must normalize because we are using L2 to simulate Cosine
            QVector<float> vec(static_cast<int>(numElements));
            memcpy(vec.data(), data, bytes.size());
            normalizeVector(vec.data(), m_dimensions);

            faiss_Index_add(m_index.get(), 1, vec.constData());
            count++;
        }
    }

    qDebug() << "Warm Start complete. Loaded" << count << "embeddings into FAISS.";
}


//--------------------------------------------------------------------------------
auto EmbeddingDatabase::fileChecksum(const QString &fileName) -> QByteArray
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly)) return {};
    
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (hash.addData(&file)) return hash.result().toHex();
    
    return {};
}


//--------------------------------------------------------------------------------
auto EmbeddingDatabase::isEmbedded(const QString& sourceFile) -> bool 
{
    QSqlQuery check(m_db);
    check.prepare("SELECT 1 FROM sources WHERE Sha256 = ? LIMIT 1");
    check.addBindValue(fileChecksum(sourceFile));
    
    return check.exec() && check.next();
}


