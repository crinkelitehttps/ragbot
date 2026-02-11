#ifndef EMBEDDINGDATABASE_H
#define EMBEDDINGDATABASE_H

#include <QString>
#include <QVector>
#include <QSqlDatabase>

class EmbeddingDatabase
{
public:
    EmbeddingDatabase(const QString &dbName = "embeddings.db") 
        : m_db()
    {
        initialize(dbName);
    };
 
    struct SearchResult {
        QString content;
        QString sourceFile;
        QString itemId;
        float similarity;
    };

    float cosineSimilarity(const QVector<float> &a, const float *b, int size);
    QByteArray fileChecksum(const QString& filename); 

    bool isEmbedded(const QString& fileName);
    int count();

    QVector<SearchResult> search(
            const QVector<float> &queryEmbedding,
            int topK = 10
    );

    bool saveEmbedding(
            const QString &sourceFile,
            const QString &itemId, 
            const QString &content,
            const QVector<float> &embedding
    );

private:
    void initialize(QString dbName);
    QSqlDatabase m_db;
};

#endif // EMBEDDINGDATABASE_H
