// Modified to use local llama-swap embedding server
#ifndef RESEARCHER_H
#define RESEARCHER_H

#include <QCoreApplication>
#include <QDirIterator>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QDebug>
#include <QTimer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include "config/ConfigResearch.h"
#include "db/EmbeddingDatabase.h"
#include "llama.h"
#include "llm/ClientEmbed.h"

class Researcher
{
public:
    Researcher(const ConfigEmbed &embedderConfig);
    
    ~Researcher()
    {
        delete m_network;
    }

    void lookup();

private:
    QNetworkAccessManager *m_network;
    ConfigResearch m_config;

#ifdef LOCAL_EMBED
    ClientEmbed m_embedClient;
#endif
};

#endif // RESEARCHER_H
