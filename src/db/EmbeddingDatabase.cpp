#include "EmbeddingDatabase.h"
#include <cmath>
#include <QDebug>
#include <QFile>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

auto normalizeVector(float* data, int dim) -> void {
    float norm = 0.0F;
    for (int i = 0; i < dim; i++) norm += data[i] * data[i];
    if (norm <= 0) return;
    norm = std::sqrt(norm);
    for (int i = 0; i < dim; i++) data[i] /= norm;
}


//--------------------------------------------------------------------------------
auto EmbeddingDatabase::initialize(const QString& dbName) -> void
{
    m_db = QSqlDatabase::addDatabase("QSQLITE", "embeddings_connection");
    m_db.setDatabaseName(dbName);

    if (!m_db.open()) {
        qCritical() << "Failed to open database:" << m_db.lastError().text();
        return;
    }

    QSqlQuery query(m_db);

    // Enable foreign key enforcement (important in SQLite)
    query.exec("PRAGMA foreign_keys = ON;");

    // Sources table
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS sources (
            source_file_id INTEGER PRIMARY KEY AUTOINCREMENT,
            sha256 TEXT UNIQUE NOT NULL,
            source_file_path TEXT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
    )");

    // Optional: explicit index (UNIQUE already creates one, but this is self-documenting)
    query.exec(R"(
        CREATE UNIQUE INDEX IF NOT EXISTS idx_sources_sha256
        ON sources(sha256);
    )");

    // Chunks table
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS chunks (
            faiss_id INTEGER PRIMARY KEY,
            source_id INTEGER NOT NULL,
            content TEXT NOT NULL,
            embedding BLOB NOT NULL,
            FOREIGN KEY (source_id) REFERENCES sources(source_file_id)
                ON DELETE CASCADE
        );
    )");

    FaissIndex* rawIndex = nullptr;

    // 0 means success in FAISS C API
    if (faiss_IndexFlatL2_new_with(&rawIndex, Dimensions) == 0) {
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
    if (queryEmbedding.size() != Dimensions || m_index == nullptr) return {};

    // 1. Prepare Query Vector
    QVector<float> normalizedQuery = queryEmbedding;

    normalizeVector(normalizedQuery.data(), Dimensions);

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
            res.content = query.value(0).toString();
            res.sourceFile = query.value(1).toString();
            // L2 to Cosine approx: 1 - (d^2 / 2)
            res.similarity = 1.0F - (distances[i] / L2ToCosineDenominator); 
            results.append(res);
        }
    }

    return results;
}


//--------------------------------------------------------------------------------
auto EmbeddingDatabase::embeddingSave(
    int sourceId,
    const QVector<float>& chunkVector,
    const QString& chunkContent
) -> bool
{
    if (chunkVector.size() != Dimensions) return false;

    int retCode = faiss_Index_add(m_index.get(), 1, chunkVector.constData());
    if (retCode != 0) {
        qWarning() << "EmbeddingDatabase::embeddingSave() retCode :" << retCode;
        return false;
    }

    QSqlQuery query(m_db);

    auto faiss = faiss_Index_add(m_index.get(), 1, chunkVector.constData());

    //auto currentFaissId = faiss_Index_ntotal(m_index.get());

    query.prepare("INSERT INTO chunks (faiss_id, source_id, content) VALUES (?, ?, ?)");
    query.addBindValue(faiss);
    query.addBindValue(sourceId);
    query.addBindValue(chunkContent);
    const auto ret = query.exec();
    if (!ret) {
        qDebug() << "EmbeddingDatabase::embeddingSave():" << query.lastError();
    }

    return ret;
}


//--------------------------------------------------------------------------------
auto EmbeddingDatabase::loadExistingEmbeddings() -> void
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

        if (numElements == Dimensions) {
            // We must normalize because we are using L2 to simulate Cosine
            QVector<float> vec(static_cast<int>(numElements));
            memcpy(vec.data(), data, bytes.size());
            normalizeVector(vec.data(), Dimensions);

            faiss_Index_add(m_index.get(), 1, vec.constData());
            count++;
        }
    }

    qDebug() << "Warm Start complete. Loaded" << count << "embeddings into FAISS.";
}


//--------------------------------------------------------------------------------
auto EmbeddingDatabase::newSourceFileId(
        const QByteArray& contentChecksum,
        const QString& file
        ) -> int
{
    QSqlQuery query(m_db);

    // 1. Check if record already exists
    query.prepare("SELECT source_file_id FROM sources WHERE sha256 = ? LIMIT 1");
    query.addBindValue(contentChecksum.constData());

    if (query.exec() && query.next()) {
        qDebug() <<  "EmbeddingDatabase::newSourceFileId(): Record exists";
        return -1;
    }

    // 2. Insert new record
    QSqlQuery insert(m_db);

    insert.prepare("INSERT INTO sources (sha256, source_file_path) VALUES (?, ?)");
    insert.addBindValue(contentChecksum.constData());
    insert.addBindValue(file);

    if (!insert.exec()) {
        qDebug() << "EmbeddingDatabase::newSourceFileId(): Insert failed" << insert.lastError();
        Q_ASSERT(0);
        return -1;
    }

    // 3. Return the newly created ID
    return insert.lastInsertId().toInt();
}


