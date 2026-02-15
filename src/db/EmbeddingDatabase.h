#ifndef EMBEDDINGDATABASE_H
#define EMBEDDINGDATABASE_H

#include <QString>
#include <QVector>
#include <QSqlDatabase>
#include "../generation/Generator.h"

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
 
    bool createSourceRecord(const QString& sourcePath);
    bool saveEmbedding(const Generator::ContentEmbedding& ContentEmbedding);

    QVector<SearchResult> search(
        const QVector<float> &queryEmbedding,
        const int topK = 10
    );

private:
    float cosineSimilarity(const QVector<float> &a, const float *b, int size);
    QByteArray fileChecksum(const QString& filename); 
    void initialize(const QString& dbName);

private:
    QSqlDatabase m_db;

};

#endif // EMBEDDINGDATABASE_H
