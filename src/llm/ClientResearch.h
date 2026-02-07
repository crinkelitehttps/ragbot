#ifndef CLIENTRESEARCH_H
#define CLIENTRESEARCH_H

#include <QString>
#include <QNetworkAccessManager>
#include "../config/LLMConfig.h"
#include "../config/ConfigResearch.h"
#include "../db/EmbeddingDatabase.h"

class ClientResearch
{
public:
    ClientResearch(const ConfigResearch &config);
    ~ClientResearch() { qDebug() << "~LLMClient()"; }

    QString chat(const QString &systemPrompt, const QString &userMessage, bool stream);

private:
    QNetworkAccessManager *m_manager;
};

#endif // CLIENTRESEARCH_H
