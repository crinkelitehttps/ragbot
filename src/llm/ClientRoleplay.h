#ifndef CLIENTROLEPLAY_H
#define CLIENTROLEPLAY_H

#include <QString>
#include <QNetworkAccessManager>
#include "../config/LLMConfig.h"
#include "../config/ConfigRoleplay.h"
#include "../db/EmbeddingDatabase.h"

class ClientRoleplay
{
public:
    ClientRoleplay(const ConfigRoleplay &config);
    ~ClientRoleplay() { qDebug() << "~LLMClient()"; }

    QString chat(const QString &systemPrompt, const QString &userMessage, bool stream);

private:
    QNetworkAccessManager *m_manager;
};

#endif // CLIENTROLEPLAY_H
