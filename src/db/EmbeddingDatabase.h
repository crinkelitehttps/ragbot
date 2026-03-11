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
           if (index != nullptr) {
               faiss_Index_free(index);
           }
       }
    };

public:
    struct SearchResult {
        QString sourceFile;
        QString itemId;
        float similarity;
    };

    EmbeddingDatabase(const QJsonObject& embedConfig) 
    {
        const auto databaseName = embedConfig.value("databaseName").toString();
        if (!databaseName.isEmpty()) {
            initialize(databaseName);
        }
    };
 
    auto isEmbedded(const QString& sourceFile) -> bool ;

    auto saveEmbedding(
        const QVector<float> &embedding,
        const QString &sourcePath,
        const QString &helperContext
    ) -> bool ;

    static constexpr int DefaultTopK = 10;
    auto search(
        QVector<float> &queryEmbedding,
        int topK = DefaultTopK 
    ) -> QVector<SearchResult>;

    auto textResults(
        const QString& queryEmbedding,
        int topK = DefaultTopK
    ) -> QVector<SearchResult>;

    void loadExistingEmbeddings();

private:
    auto fileChecksum(const QString& filename) -> QByteArray; 
    void initialize(const QString& dbName);
    std::unique_ptr<FaissIndex, FaissDeleter> m_index;
    const static int m_dimensions { 768 };
    QSqlDatabase m_db;
};

#endif // EMBEDDINGDATABASE_H
