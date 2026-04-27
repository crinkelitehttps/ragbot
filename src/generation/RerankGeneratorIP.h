#ifndef RERANKGENERATORIP_H
#define RERANKGENERATORIP_H

#include <QNetworkAccessManager>
#include <QJsonObject>
#include "RerankGenerator.h"

class QNetworkReply;

// Re-ranker that calls a Cohere-compatible /v1/rerank endpoint.
class RerankGeneratorIP : public RerankGenerator
{
public:
    explicit RerankGeneratorIP(const QJsonObject& config);

    auto score(const QString& query, const QStringList& documents) -> QVector<float> override;
    [[nodiscard]] auto isValid() const -> bool override { return m_isValid; }

private:
    auto runLoop(QNetworkReply* reply) -> bool;

    static constexpr int DefaultTimeout { 60000 };

    QNetworkAccessManager m_network;
    QString m_basePath;
    QString m_modelName;
    int     m_timeout;
    bool    m_isValid;
};

#endif // RERANKGENERATORIP_H
