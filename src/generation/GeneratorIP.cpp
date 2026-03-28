#include <QTimer>
#include <QEventLoop>
#include <QFile>
#include <QNetworkReply>
#include "GeneratorIP.h"


//--------------------------------------------------------------------------------
GeneratorIP::GeneratorIP(const QJsonObject& config)
    : Generator(config.value("generator").toObject())
    , m_modelPath(config.value("modelPath").toString())
    , m_timeout(DefaultTimeout)
{
    qDebug() << "GeneratorIP::GeneratorIP(): " << config;
};


//--------------------------------------------------------------------------------
auto GeneratorIP::parseResponse(const QByteArray& responseData) -> QVector<float> {
    QVector<float> embedding;
    QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
    
    if (responseDoc.isNull()) return {};

    QJsonObject obj = responseDoc.object();
    QJsonArray dataArray = obj["data"].toArray();
    
    if (dataArray.isEmpty()) return {};

    QJsonObject firstItem = dataArray[0].toObject();
    QJsonArray embArray = firstItem["embedding"].toArray();

    embedding.reserve(embArray.size());
    for (const auto& val : embArray) {
        embedding.append(static_cast<float>(val.toDouble()));
    }
    
    return embedding;
}


//--------------------------------------------------------------------------------
auto GeneratorIP::generate(const QString& data) -> QVector<float>
{
    QJsonObject request{{"input", data}};
    QNetworkRequest netRequest(QUrl(m_modelPath + "v1/embeddings"));
    netRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    netRequest.setTransferTimeout(m_timeout);

    QNetworkReply *reply = m_network.post(netRequest, QJsonDocument(request).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    timer.start(m_timeout);
    loop.exec();

    QVector<float> embedding;
    if (timer.isActive()) {
        timer.stop();
        if (reply->error() == QNetworkReply::NoError) {
            embedding = parseResponse(reply->readAll());
        } else {
            qWarning() << "Network error:" << reply->errorString();
        }
    } else {
        reply->abort();
        qWarning() << "Request timed out";
    }

    reply->deleteLater();

    if (embedding.isEmpty()) {
        qWarning() << "No embedding data";
    }

    return embedding;
}


//--------------------------------------------------------------------------------
auto GeneratorIP::parseStaticResponse(const QByteArray& data) -> QString {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) return {};

    QJsonArray choices = doc.object()["choices"].toArray();
    if (choices.isEmpty()) return {};

    return choices[0].toObject()["message"].toObject()["content"].toString();
}


//--------------------------------------------------------------------------------
auto GeneratorIP::parseStreamChunk(const QByteArray& data) -> QString {
    QString fullChunkText;
    QString rawText(data);
    QStringList lines = rawText.split("\n");

    for (const QString &line : lines) {
        if (!line.startsWith("data: ")) continue;
        
        QString jsonStr = line.mid(SseDataPrefix.size()).trimmed();
        if (jsonStr == "[DONE]" || jsonStr.isEmpty()) continue;

        QJsonDocument streamDoc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (streamDoc.isNull()) continue;

        // Flattened JSON traversal
        QJsonObject obj = streamDoc.object();
        QJsonArray choices = obj["choices"].toArray();
        if (choices.isEmpty()) continue;

        QJsonObject delta = choices[0].toObject()["delta"].toObject();
        if (delta.contains("content")) {
            fullChunkText += delta["content"].toString();
        }
    }
    return fullChunkText;
}


//--------------------------------------------------------------------------------
auto GeneratorIP::generateText(
    QString& systemPrompt,
    bool isStream,
    QString& prompt
) -> QString 
{
    QJsonArray messages;
    if (!systemPrompt.isEmpty()) {
        messages.append(QJsonObject{{"role", "system"}, {"content", systemPrompt}});
    }
    messages.append(QJsonObject{{"role", "user"}, {"content", prompt}});

    QJsonObject request{
        {"model", m_modelPath},
        {"stream", isStream},
        {"messages", messages},
        {"temperature", DefaultTemp},
        {"max_tokens", DefaultMaxTokens}
    };

    QNetworkRequest netRequest(QUrl(m_modelPath + "/chat/completions"));
    netRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    netRequest.setTransferTimeout(m_timeout);

    QNetworkReply *reply = m_network.post(netRequest, QJsonDocument(request).toJson());
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    QString fullResponse;
    if (isStream) {
        QObject::connect(reply, &QNetworkReply::readyRead, [&]() {
            QString content = parseStreamChunk(reply->readAll());
            fullResponse += content;
            QTextStream(stdout) << content << Qt::flush;
        });
    }

    timer.start(m_timeout);
    loop.exec();

    // 4. Cleanup and Return
    QString finalResult;
    if (timer.isActive()) {
        timer.stop();
        if (reply->error() == QNetworkReply::NoError) {
            finalResult = isStream ? fullResponse : parseStaticResponse(reply->readAll());
        } else {
            qWarning() << "Network error:" << reply->errorString();
        }
    } else {
        reply->abort();
        qWarning() << "Request timed out";
    }

    reply->deleteLater();
    return finalResult;
}


#if DEBUG_DISABLE

//--------------------------------------------------------------------------------
auto GeneratorIP::generateText(
    SystemPrompt& systemPrompt,
    Prompt& prompt,
    bool isStream
) -> QString 
{
    QJsonObject request;
    request["model"] = m_modelPath;
    request["stream"] = isStream;
    
    QJsonArray messages;
    Prompt userMessage = prompt;
    userMessage = prompt;
    
    if (!systemPrompt.value.isEmpty()) {
        QJsonObject sysMsg;
        sysMsg["role"] = "system";
        sysMsg["content"] = systemPrompt.value;
        messages.append(sysMsg);
    }
    
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userMessage.value;
    messages.append(userMsg);
    
    request["messages"] = messages;
    request["temperature"] = 0.7;
    request["max_tokens"] = 2000;
    
    QJsonDocument doc(request);
    QByteArray jsonData = doc.toJson();
    
    QNetworkRequest netRequest;
    netRequest.setUrl(QUrl(m_modelPath + "/chat/completions"));

    netRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    netRequest.setTransferTimeout(m_timeout);
    
    QNetworkReply *reply = m_network.post(netRequest, jsonData);
    
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(m_timeout);
    
    if (isStream) {
        QString fullResponse;
        QObject::connect(reply, &QNetworkReply::readyRead, [&]() {
            QByteArray data = reply->readAll();
            QString text(data);
            
            // Parse SSE format
            QStringList lines = text.split("\n");
            for (const QString &line : lines) {
                if (line.startsWith("data: ")) {
                    QString jsonStr = line.mid(6).trimmed();
                    if (jsonStr == "[DONE]") continue;
                    
                    QJsonDocument streamDoc = QJsonDocument::fromJson(jsonStr.toUtf8());
                    if (!streamDoc.isNull()) {
                        QJsonObject obj = streamDoc.object();
                        if (obj.contains("choices")) {
                            QJsonArray choices = obj["choices"].toArray();
                            if (!choices.isEmpty()) {
                                QJsonObject choice = choices[0].toObject();
                                if (choice.contains("delta")) {
                                    QJsonObject delta = choice["delta"].toObject();
                                    if (delta.contains("content")) {
                                        QString content = delta["content"].toString();
                                        fullResponse += content;
                                        QTextStream(stdout) << content << Qt::flush;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        });
        
        loop.exec();
        
        if (timer.isActive()) {
            timer.stop();
            reply->deleteLater();
            return fullResponse;
        } else {
            reply->abort();
            reply->deleteLater();
            qWarning() << "Request timed out";
            return QString();
        }
    }
    
    loop.exec();
    
    QString response;
    
    if (timer.isActive()) {
        timer.stop();
        
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
            
            if (!responseDoc.isNull()) {
                QJsonObject obj = responseDoc.object();
                if (obj.contains("choices")) {
                    QJsonArray choices = obj["choices"].toArray();
                    if (!choices.isEmpty()) {
                        QJsonObject choice = choices[0].toObject();
                        if (choice.contains("message")) {
                            QJsonObject message = choice["message"].toObject();
                            response = message["content"].toString();
                        }
                    }
                }
            }
        } else {
            qWarning() << "Network error:" << reply->errorString();
            Q_ASSERT(reply->errorString().isEmpty());
        }
    } else {
        reply->abort();
        qWarning() << "Request timed out";
    }
    
    reply->deleteLater();
    return response;
};
#endif
