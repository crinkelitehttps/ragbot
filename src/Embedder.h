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
#include <QSqlError>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include "db/EmbeddingDatabase.h"

// Configuration for llama-swap embedding server
struct EmbedConfig {
    bool enabled = false;
    QString baseUrl = "http://127.0.0.1:8080";  // llama-swap default port
    QString model = "nomic-embed";  // Model name for llama-swap
    int timeout = 30000;  // 30 seconds
};

class Embedder
{
public:
    Embedder(const EmbedConfig &config);
    
    QString extractTextFromJson(const QJsonValue &value, const QStringList &keys);
    void extractTextRecursive(const QJsonValue &value, const QStringList &keys, QStringList &texts);
    bool embedFile(const QString &inputPath);
    bool embedAndSave(const QString &text, const QString &sourcePath, const QString &itemId);
    void processAllFiles();

    ~Embedder()
    {
        delete m_manager;
    }
    
    QVector<float> generateEmbedding(const QString &text);

private:
    QString m_jsonDir;
    EmbeddingDatabase *m_db;
    EmbedConfig m_config;
    QNetworkAccessManager *m_manager;
};

#endif // EMBEDDER_H
