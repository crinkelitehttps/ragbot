#ifndef EMBEDDINGDATABASE_H
#define EMBEDDINGDATABASE_H

#include <QString>
#include <QVector>
#include <QSqlDatabase>
#include <faiss_c.h>
#include <Index_c.h>
#include <IndexFlat_c.h>
#include "../generation/Generator.h"


class EmbeddingDatabase
{

private:
   struct FaissDeleter {
       void operator()(FaissIndex* index) const {
           if (index) {
               faiss_Index_free(index);
           }
       }
    };

public:

    struct SearchResult {
        QString content;
        QString sourceFile;
        QString itemId;
        float similarity;
    };

    EmbeddingDatabase(const QString &dbName = "rag_metadata.db") 
        : m_db()
    {
        initialize(dbName);
    };
 
    bool isEmbedded(const QString& sourceFile);

    bool saveEmbedding(
        const QVector<float> &embedding,
        const QString &sourcePath,
        const QString &helperContext
    );

    QVector<SearchResult> search(
        const QVector<float> &queryEmbedding,
        const int topK = 10
    );

    void loadExistingEmbeddings();

private:
    QByteArray fileChecksum(const QString& filename); 
    void initialize(const QString& dbName);

private:
    std::unique_ptr<FaissIndex, FaissDeleter> m_index;
    int m_dimension = 768;
    QSqlDatabase m_db;
};

#endif // EMBEDDINGDATABASE_H
