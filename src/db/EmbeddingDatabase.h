#ifndef EMBEDDINGDATABASE_H
#define EMBEDDINGDATABASE_H

#include <QString>
#include <QVector>
#include <QSqlDatabase>

class EmbeddingDatabase
{
public:
    EmbeddingDatabase(const QString &dbName = "embeddings.db");
    
    struct SearchResult {
        QString content;
        QString sourceFile;
        QString itemId;
        float similarity;
    };
    
    QVector<SearchResult> search(const QVector<float> &queryEmbedding, int topK = 10);
    int count();
    bool saveEmbedding(const QString &sourceFile, const QString &itemId, 
                      const QString &content, const QVector<float> &embedding);

private:
    float cosineSimilarity(const QVector<float> &a, const float *b, int size);
    QSqlDatabase m_db;
};

#endif // EMBEDDINGDATABASE_H
