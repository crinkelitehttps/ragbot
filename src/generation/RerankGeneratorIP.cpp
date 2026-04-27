#include "RerankGeneratorIP.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>
#include <QEventLoop>
#include <QDebug>
#include "../ConfigKeys.h"


//--------------------------------------------------------------------------------
auto RerankGeneratorIP::runLoop(QNetworkReply* reply) -> bool
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply,  &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout,         &loop, &QEventLoop::quit);
    timer.start(m_timeout);
    loop.exec();
    if (timer.isActive()) { timer.stop(); return true; }
    reply->abort();
    return false;
}


//--------------------------------------------------------------------------------
RerankGeneratorIP::RerankGeneratorIP(const QJsonObject& config)
    : m_modelName(config.value(ConfigKeys::ModelName).toString())
    , m_timeout(config.value(ConfigKeys::Timeout).toInt(DefaultTimeout))
    , m_isValid(false)
{
    m_basePath = config.value(ConfigKeys::BasePath).toString();
    if (m_basePath.isEmpty()) {
        const QString legacy = config.value(ConfigKeys::RemotePath).toString();
        if (!legacy.isEmpty()) {
            qWarning() << "RerankGeneratorIP: 'remotePath' is deprecated — use 'basePath'";
            m_basePath = legacy;
        }
    }
    m_isValid = !m_basePath.isEmpty();
    if (!m_isValid)
        qWarning() << "RerankGeneratorIP: no basePath in config — disabled";
}


//--------------------------------------------------------------------------------
auto RerankGeneratorIP::score(const QString& query, const QStringList& documents) -> QVector<float>
{
    if (!m_isValid) return {};

    QJsonArray docs;
    for (const auto& doc : documents) docs.append(doc);

    const QJsonObject body{
        {"model",            m_modelName},
        {"query",            query},
        {"documents",        docs},
        {"return_documents", false}
    };

    QNetworkRequest req(QUrl(m_basePath + "v1/rerank"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(m_timeout);

    QNetworkReply* reply = m_network.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));

    QVector<float> scores(documents.size(), 0.0f);
    if (runLoop(reply)) {
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            const QJsonArray results = doc.object()["results"].toArray();
            if (results.isEmpty())
                qWarning() << "RerankGeneratorIP::score(): empty results in response — returning zero scores";
            else if (results.size() < documents.size())
                qWarning() << "RerankGeneratorIP::score(): got" << results.size()
                           << "results for" << documents.size() << "documents";
            for (const auto& r : results) {
                const QJsonObject obj = r.toObject();
                const int idx = obj["index"].toInt(-1);
                if (idx >= 0 && idx < scores.size())
                    scores[idx] = static_cast<float>(obj["relevance_score"].toDouble());
            }
        } else {
            qWarning() << "RerankGeneratorIP::score() network error:" << reply->errorString();
        }
    } else {
        qWarning() << "RerankGeneratorIP::score() timed out after" << m_timeout << "ms";
    }
    reply->deleteLater();
    return scores;
}
