#include <QTimer>
#include <QEventLoop>
#include <QNetworkReply>
#include "GeneratorIP.h"


//--------------------------------------------------------------------------------
GeneratorIP::GeneratorIP(ConfigGenerator generatorConfig)
       : Generator(generatorConfig)
       , m_config(generatorConfig) 
{
};


//--------------------------------------------------------------------------------
const QVector<float> GeneratorIP::generate(const QString& data) 
{
    QJsonObject request;
    request["input"] = data;
    QJsonDocument doc(request);
    if (doc.isEmpty()) {
        qWarning() << "GeneratorIP::generate() doc.isEmpty()";
    }
    QByteArray jsonData = doc.toJson();
    if (jsonData.isEmpty()) {
        qWarning() << "GeneratorIP::generate() jsonData.isEmpty()";
    }

    QNetworkRequest netRequest;
    netRequest.setUrl(QUrl(m_config.basePath+ "v1/embeddings"));
    netRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    netRequest.setTransferTimeout(m_config.timeout);

    qDebug().noquote() << "GeneratorIP::generate()" << data;

    QNetworkReply *reply = m_network.post(netRequest, QString(QJsonDocument(request).toJson(QJsonDocument::Compact)).toUtf8());
    QEventLoop loop;

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    QTimer timer;
    timer.setSingleShot(true);

    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    timer.start(m_config.timeout);
    loop.exec();

    const auto parse = [] (QByteArray &responseData, QVector<float>& embedding)  {
        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
        if (!responseDoc.isNull()) {
            QJsonObject obj = responseDoc.object();
            if (obj.contains("data")) {
                QJsonArray dataArray = obj["data"].toArray();
                if (!dataArray.isEmpty()) {
                    QJsonObject firstItem = dataArray[0].toObject();
                    if (firstItem.contains("embedding")) {
                        QJsonArray embArray = firstItem["embedding"].toArray();
                        for (const QJsonValue &val : embArray) {
                            embedding.append(val.toDouble());
                        }
                    }
                }
            }
        }
    };

    QVector<float> embedding;
    if (timer.isActive()) {
        timer.stop();
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            parse(responseData, embedding);
        } else {
            qWarning() << "GeneratorIP::generate(): Network error:" 
                << reply->errorString();
        }
    } else {
        reply->abort();
        qWarning() << "GeneratorIP::generate(): Request timed out";
    }

    return embedding;
};


//--------------------------------------------------------------------------------
const QString GeneratorIP::generateText(
    QString& systemPrompt,
    QString& prompt,
    bool isStream
    ) 
{
    QJsonObject request;
    request["model"] = m_config.modelName;
    request["stream"] = isStream;
    
    QJsonArray messages;
    QString userMessage = prompt;
    userMessage = prompt;
    
    if (!systemPrompt.isEmpty()) {
        QJsonObject sysMsg;
        sysMsg["role"] = "system";
        sysMsg["content"] = systemPrompt;
        messages.append(sysMsg);
    }
    
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userMessage;
    messages.append(userMsg);
    
    request["messages"] = messages;
    request["temperature"] = 0.7;
    request["max_tokens"] = 2000;
    
    QJsonDocument doc(request);
    QByteArray jsonData = doc.toJson();
    
    QNetworkRequest netRequest;
    netRequest.setUrl(QUrl(m_config.basePath+ "/chat/completions"));

    qDebug() << "GeneratorIP::generateText" << m_config.basePath;
    qDebug() << "GeneratorIP::generateText" << netRequest.url();
 
    netRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    netRequest.setTransferTimeout(m_config.timeout);
    
    QNetworkReply *reply = m_network.post(netRequest, jsonData);
    
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(m_config.timeout);
    
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
        }
    } else {
        reply->abort();
        qWarning() << "Request timed out";
    }
    
    reply->deleteLater();
    return response;
};
