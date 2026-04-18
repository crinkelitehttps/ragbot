#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QEventLoop>
#include <QNetworkReply>
#include <QTextStream>
#include <QDebug>
#include "GeneratorIP.h"


//--------------------------------------------------------------------------------
auto GeneratorIP::runLoop(QNetworkReply* reply) -> bool
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
GeneratorIP::GeneratorIP(const QJsonObject& config)
    : m_basePath(config.value("basePath").toString(
          config.value("remotePath").toString()))
    , m_modelName(config.value("modelName").toString())
    , m_timeout(config.value("timeout").toInt(DefaultTimeout))
    , m_isValid(!m_basePath.isEmpty())
{
    if (!m_isValid)
        qWarning() << "GeneratorIP: no basePath/remotePath in config — disabled";
}


//--------------------------------------------------------------------------------
auto GeneratorIP::parseEmbeddingResponse(const QByteArray& data) -> QVector<float>
{
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) return {};

    const QJsonArray dataArr = doc.object()["data"].toArray();
    if (dataArr.isEmpty()) {
        qWarning() << "GeneratorIP::parseEmbeddingResponse(): empty data array";
        return {};
    }
    const QJsonArray embArray = dataArr.at(0).toObject()["embedding"].toArray();

    QVector<float> result;
    result.reserve(embArray.size());
    for (const auto& v : embArray) result.append(static_cast<float>(v.toDouble()));
    return result;
}


//--------------------------------------------------------------------------------
auto GeneratorIP::generate(const QString& data) -> QVector<float>
{
    if (!m_isValid) return {};

    QNetworkRequest req(QUrl(m_basePath + "v1/embeddings"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(m_timeout);

    const QJsonObject body{{"input", data}, {"model", m_modelName}};
    QNetworkReply* reply = m_network.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));

    QVector<float> result;
    if (runLoop(reply)) {
        if (reply->error() == QNetworkReply::NoError)
            result = parseEmbeddingResponse(reply->readAll());
        else
            qWarning() << "GeneratorIP::generate() network error:" << reply->errorString();
    } else {
        qWarning() << "GeneratorIP::generate() timed out after" << m_timeout << "ms";
    }
    reply->deleteLater();
    return result;
}


//--------------------------------------------------------------------------------
auto GeneratorIP::parseStaticResponse(const QByteArray& data) -> QString
{
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) return {};

    const QJsonArray choices = doc.object()["choices"].toArray();
    if (choices.isEmpty()) {
        qWarning() << "GeneratorIP::parseStaticResponse(): empty choices array";
        return {};
    }
    return choices.at(0).toObject()["message"].toObject()["content"].toString();
}


//--------------------------------------------------------------------------------
auto GeneratorIP::parseStreamChunk(const QByteArray& data) -> QString
{
    QString result;
    for (const QString& line : QString(data).split('\n')) {
        if (!line.startsWith(SseDataPrefix)) continue;
        const QString json = line.mid(SseDataPrefix.size()).trimmed();
        if (json == "[DONE]" || json.isEmpty()) continue;

        const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        if (doc.isNull()) continue;

        const QJsonArray choices = doc.object()["choices"].toArray();
        if (choices.isEmpty()) continue;
        const QJsonObject delta = choices.at(0).toObject()["delta"].toObject();
        if (delta.contains("content")) result += delta["content"].toString();
    }
    return result;
}


//--------------------------------------------------------------------------------
auto GeneratorIP::generateText(
        const QString& systemPrompt,
        bool isStream,
        const QString& prompt
) -> QString
{
    if (!m_isValid) return {};

    QJsonArray messages;
    if (!systemPrompt.isEmpty())
        messages.append(QJsonObject{{"role", "system"}, {"content", systemPrompt}});
    messages.append(QJsonObject{{"role", "user"}, {"content", prompt}});

    const QJsonObject body{
        {"model",       m_modelName},
        {"stream",      isStream},
        {"messages",    messages},
        {"temperature", DefaultTemp},
        {"max_tokens",  DefaultMaxTokens}
    };

    QNetworkRequest req(QUrl(m_basePath + "v1/chat/completions"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(m_timeout);

    QNetworkReply* reply = m_network.post(req, QJsonDocument(body).toJson());

    QString streamed;
    if (isStream) {
        QObject::connect(reply, &QNetworkReply::readyRead, [&]() {
            const QString chunk = parseStreamChunk(reply->readAll());
            streamed += chunk;
            QTextStream(stdout) << chunk << Qt::flush;
        });
    }

    QString result;
    if (runLoop(reply)) {
        if (reply->error() == QNetworkReply::NoError)
            result = isStream ? streamed : parseStaticResponse(reply->readAll());
        else
            qWarning() << "GeneratorIP::generateText() network error:" << reply->errorString();
    } else {
        qWarning() << "GeneratorIP::generateText() timed out after" << m_timeout << "ms";
    }
    reply->deleteLater();
    return result;
}
