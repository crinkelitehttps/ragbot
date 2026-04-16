#include <QTimer>
#include <QEventLoop>
#include <QNetworkReply>
#include <QTextStream>
#include "GeneratorIP.h"


//--------------------------------------------------------------------------------
GeneratorIP::GeneratorIP(const QJsonObject& config)
    : Generator(config)
    , m_basePath(config.value("basePath").toString(
          config.value("remotePath").toString())) // support both key names
    , m_modelName(config.value("modelName").toString())
    , m_timeout(config.value("timeout").toInt(DefaultTimeout))
    , m_isValid(!m_basePath.isEmpty())
{
    qDebug() << "GeneratorIP::GeneratorIP():" << config;
    if (!m_isValid) {
        qWarning() << "GeneratorIP: config has no 'basePath' or 'remotePath' — generator disabled";
    }
}


//--------------------------------------------------------------------------------
auto GeneratorIP::parseEmbeddingResponse(const QByteArray& responseData) -> QVector<float>
{
    const QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (doc.isNull()) return {};

    const QJsonArray dataArray = doc.object()["data"].toArray();
    if (dataArray.isEmpty()) return {};

    const QJsonArray embArray = dataArray[0].toObject()["embedding"].toArray();

    QVector<float> embedding;
    embedding.reserve(embArray.size());
    for (const auto& val : embArray) {
        embedding.append(static_cast<float>(val.toDouble()));
    }
    return embedding;
}


//--------------------------------------------------------------------------------
auto GeneratorIP::generate(const QString& data) -> QVector<float>
{
    if (!m_isValid) return {};

    const QJsonObject body{{"input", data}, {"model", m_modelName}};
    QNetworkRequest req(QUrl(m_basePath + "v1/embeddings"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(m_timeout);

    QNetworkReply* reply = m_network.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout,         &loop, &QEventLoop::quit);
    timer.start(m_timeout);
    loop.exec();

    QVector<float> result;
    if (timer.isActive()) {
        timer.stop();
        if (reply->error() == QNetworkReply::NoError) {
            result = parseEmbeddingResponse(reply->readAll());
        } else {
            qWarning() << "GeneratorIP::generate() network error:" << reply->errorString();
        }
    } else {
        reply->abort();
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
    if (choices.isEmpty()) return {};

    return choices[0].toObject()["message"].toObject()["content"].toString();
}


//--------------------------------------------------------------------------------
auto GeneratorIP::parseStreamChunk(const QByteArray& data) -> QString
{
    QString result;
    for (const QString& line : QString(data).split('\n')) {
        if (!line.startsWith(SseDataPrefix)) continue;

        const QString jsonStr = line.mid(SseDataPrefix.size()).trimmed();
        if (jsonStr == "[DONE]" || jsonStr.isEmpty()) continue;

        const QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (doc.isNull()) continue;

        const QJsonObject delta =
            doc.object()["choices"].toArray().at(0).toObject()["delta"].toObject();
        if (delta.contains("content")) {
            result += delta["content"].toString();
        }
    }
    return result;
}


//--------------------------------------------------------------------------------
auto GeneratorIP::generateText(
    QString& systemPrompt,
    bool isStream,
    QString& prompt
) -> QString
{
    if (!m_isValid) return {};

    QJsonArray messages;
    if (!systemPrompt.isEmpty()) {
        messages.append(QJsonObject{{"role", "system"}, {"content", systemPrompt}});
    }
    messages.append(QJsonObject{{"role", "user"}, {"content", prompt}});

    const QJsonObject body{
        {"model",       m_modelName},
        {"stream",      isStream},
        {"messages",    messages},
        {"temperature", DefaultTemp},
        {"max_tokens",  static_cast<int>(DefaultMaxTokens)}
    };

    QNetworkRequest req(QUrl(m_basePath + "v1/chat/completions"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(m_timeout);

    QNetworkReply* reply = m_network.post(req, QJsonDocument(body).toJson());

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout,         &loop, &QEventLoop::quit);

    QString streamed;
    if (isStream) {
        QObject::connect(reply, &QNetworkReply::readyRead, [&]() {
            const QString chunk = parseStreamChunk(reply->readAll());
            streamed += chunk;
            QTextStream(stdout) << chunk << Qt::flush;
        });
    }

    timer.start(m_timeout);
    loop.exec();

    QString result;
    if (timer.isActive()) {
        timer.stop();
        if (reply->error() == QNetworkReply::NoError) {
            result = isStream ? streamed : parseStaticResponse(reply->readAll());
        } else {
            qWarning() << "GeneratorIP::generateText() network error:" << reply->errorString();
        }
    } else {
        reply->abort();
        qWarning() << "GeneratorIP::generateText() timed out after" << m_timeout << "ms";
    }
    reply->deleteLater();
    return result;
}
