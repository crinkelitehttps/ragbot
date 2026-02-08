// Modified to use local llama-swap embedding server
#ifndef EMBEDDER_H
#define EMBEDDER_H

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
#include "config/ConfigEmbed.h"
#include "db/EmbeddingDatabase.h"
#include "llama.h"
#include "llm/ClientEmbed.h"
#include "Generator.h"

class Embedder
{
public:
    Embedder(const ConfigEmbed &embedderConfig);
    
    ~Embedder()
    {
        delete m_network;
    }

    void processAllFiles();

private:
    bool embedAndSave(const QString &text, const QString &sourcePath, const QString &itemId);
    bool embedFile(const QString &inputPath);
    QString extractTextFromJson(const QJsonValue &value, const QStringList &keys);
    void extractTextRecursive(const QJsonValue &value, const QStringList &keys, QStringList &texts);
    QVector<float> generateEmbedding(const QString &text);

private:
    QNetworkAccessManager *m_network;
    ConfigEmbed m_config;
    EmbeddingDatabase m_embed_db;
    Generator m_generator;

    llama_context *m_embedCtx;
    llama_model *m_embedModel;

#ifdef LOCAL_EMBED
    ClientEmbed m_embedClient;
#endif
};

#endif // EMBEDDER_H
