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

    struct SourcePath { QString value; };
    struct HelperContext { QString value; };

    auto saveEmbedding(
        const QVector<float> &embedding,
        const SourcePath &sourcePath,
        const HelperContext &helperContext
    ) -> bool;

    auto textResults(
        const QVector<float>& queryEmbedding,
        int topK = DefaultTopK
    ) -> QVector<SearchResult>;

private:
    void loadExistingEmbeddings();
    static auto fileChecksum(const QString& filename) -> QByteArray; 

    void initialize(const QString& dbName);
    std::unique_ptr<FaissIndex, FaissDeleter> m_index;
    QSqlDatabase m_db;

    const static int m_dimensions { 768 };
    static constexpr int DefaultTopK { 10 };
    static constexpr float L2ToCosineDenominator { 2.0F };
};

#endif // EMBEDDINGDATABASE_H
