#ifndef EMBEDDINGDATABASE_H
#define EMBEDDINGDATABASE_H

#include <QString>
#include <QVector>
#include <QSqlDatabase>

class EmbeddingDatabase
{
public:
    struct SearchResult {
        QString content;
        QString sourceFile;
        QString itemId;
        float similarity;
    };

    EmbeddingDatabase(const QString &dbName = "embeddings.db") 
        : m_db()
    {
        initialize(dbName);
    };
 
    const bool createSourceRecord(const QString& sourcePath);
    const bool saveEmebedding(const QVector<float>& embeddings);

    const QVector<SearchResult> search(
        const QVector<float> &queryEmbedding,
        const int topK = 10
    );

private:
    const float cosineSimilarity(const QVector<float> &a, const float *b, int size);
    const QByteArray fileChecksum(const QString& filename); 
    void initialize(const QString& dbName);

private:
    QSqlDatabase m_db;

};

#endif // EMBEDDINGDATABASE_H
