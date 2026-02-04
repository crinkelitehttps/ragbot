#ifndef REMOTELLMCLIENT_H
#define REMOTELLMCLIENT_H

#include <QString>
#include <QNetworkAccessManager>
#include "LLMConfig.h"

class LLMClient
{
public:
    LLMClient(const LLMConfig &config);
    LLMClient(const QString &baseUrl, const QString &model, int timeout = 60000);
    ~LLMClient();

    QString chat(const QString &systemPrompt, const QString &userMessage, bool stream);

private:
    LLMConfig m_config;
    QNetworkAccessManager *m_manager;
};

#endif // REMOTELLMCLIENT_H
