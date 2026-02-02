#include "RemoteLLMClient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QTextStream>
#include <QDebug>
#include <QUrl>

RemoteLLMClient::RemoteLLMClient(const RemoteLLMConfig &config)
    : m_config(config), m_manager(new QNetworkAccessManager())
{
}

RemoteLLMClient::RemoteLLMClient(const QString &baseUrl, const QString &model, int timeout)
    : m_manager(new QNetworkAccessManager())
{
    m_config.enabled = true;
    m_config.baseUrl = baseUrl;
    m_config.model = model;
    m_config.timeout = timeout;
}

RemoteLLMClient::~RemoteLLMClient()
{
    delete m_manager;
}

QString RemoteLLMClient::chat(const QString &systemPrompt, const QString &userMessage, bool stream)
{
    QJsonObject request;
    request["model"] = m_config.model;
    request["stream"] = stream;
    
    QJsonArray messages;
    
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
    request["max_tokens"] = 20000;
    
    QJsonDocument doc(request);
    QByteArray jsonData = doc.toJson();
    
    QNetworkRequest netRequest;
    netRequest.setUrl(QUrl(m_config.baseUrl + "/v1/chat/completions"));
    netRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    netRequest.setTransferTimeout(m_config.timeout);
    
    QNetworkReply *reply = m_manager->post(netRequest, jsonData);
    
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(m_config.timeout);
    
    qDebug() << "stream"<< stream;
    if (stream) {
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
}
