#ifndef EMBEDDINGDATABASE_H
#define EMBEDDINGDATABASE_H

#include <QFile>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <QFileInfo>
#include <faiss_c.h>
#include <Index_c.h>
#include <IndexFlat_c.h>



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
        QString content;
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
 
    auto newSourceFileId(const QByteArray& contentChecksum, const QString& file) -> int;

    auto embeddingSave(
        int sourceId,
        const QVector<float>& chunkVector,
        const QString& chunkContent
    ) -> bool;

    auto textResults(
        const QVector<float>& queryEmbedding,
        int topK = DefaultTopK
    ) -> QVector<SearchResult>;
    
private:
    void loadExistingEmbeddings();

    void initialize(const QString& dbName);
    std::unique_ptr<FaissIndex, FaissDeleter> m_index;
    QSqlDatabase m_db;

    static constexpr int Dimensions { 768 };
    static constexpr int DefaultTopK { 10 };
    static constexpr float L2ToCosineDenominator { 2.0F };
};

#endif // EMBEDDINGDATABASE_H
