#ifndef REMOTELLMCLIENT_H
#define REMOTELLMCLIENT_H

#include <QString>
#include <QNetworkAccessManager>
#include "RemoteLLMConfig.h"

class RemoteLLMClient
{
public:
    RemoteLLMClient(const RemoteLLMConfig &config);
    RemoteLLMClient(const QString &baseUrl, const QString &model, int timeout = 60000);
    ~RemoteLLMClient();

    QString chat(const QString &systemPrompt, const QString &userMessage, bool stream);

private:
    RemoteLLMConfig m_config;
    QNetworkAccessManager *m_manager;
};

#endif // REMOTELLMCLIENT_H
