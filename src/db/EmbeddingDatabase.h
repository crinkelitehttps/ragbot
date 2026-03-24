#ifndef EMBEDDINGDATABASE_H
#define EMBEDDINGDATABASE_H

#include <QFile>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QString>
#include <QVector>
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
 
    auto newSourceFileId(const QByteArray& contentChecksum) -> int;

    auto embeddingSave(
        const QVector<float>& chunkVector,
        const QByteArray& chunkContent,
        int chunkContextId 
    ) -> bool;

    auto textResults(
        const QVector<float>& queryEmbedding,
        int topK = DefaultTopK
    ) -> QVector<SearchResult>;
    
#if DEBUG_DISABLED
    static auto chunkId(const QString& data) -> QByteArray; 
#endif

private:
    void loadExistingEmbeddings();

    void initialize(const QString& dbName);
    std::unique_ptr<FaissIndex, FaissDeleter> m_index;
    QSqlDatabase m_db;

    const static int m_dimensions { 768 };
    static constexpr int DefaultTopK { 10 };
    static constexpr float L2ToCosineDenominator { 2.0F };
};

#endif // EMBEDDINGDATABASE_H
