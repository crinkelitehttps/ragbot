#ifndef CLIENTEMBED_H
#define CLIENTEMBED_H

#include <QString>
#include <QNetworkAccessManager>
#include "../config/LLMConfig.h"
#include "../config/ConfigEmbed.h"
#include "../db/EmbeddingDatabase.h"

class ClientEmbed
{
public:
    ClientEmbed(const ConfigEmbed &config);
    ~ClientEmbed() { qDebug() << "~LLMClient()"; }

    QString chat(const QString &systemPrompt, const QString &userMessage, bool stream);

private:
    QNetworkAccessManager *m_manager;
};

#endif // CLIENTEMBED_H
