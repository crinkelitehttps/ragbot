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

// Configuration for llama-swap embedding server
struct RemoteEmbedConfig {
    bool enabled = false;
    QString baseUrl = "http://127.0.0.1:8080";  // llama-swap default port
    QString model = "nomic-embed";  // Model name for llama-swap
    int timeout = 30000;  // 30 seconds
};


class EmbeddingDatabase
{
public:
    EmbeddingDatabase(const QString &dbName = "embeddings.db")
    {
        m_db = QSqlDatabase::addDatabase("QSQLITE");
        m_db.setDatabaseName(dbName);
        
        if (!m_db.open()) {
            qCritical() << "Failed to open database:" << m_db.lastError().text();
            return;
        }
        
        createTables();
    }
    
    bool createTables()
    {
        QSqlQuery query(m_db);
        
        QString createTable = R"(
            CREATE TABLE IF NOT EXISTS embeddings (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                source_file TEXT NOT NULL,
                item_id TEXT,
                content TEXT NOT NULL,
                embedding BLOB NOT NULL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                UNIQUE(source_file, item_id)
            )
        )";
        
        if (!query.exec(createTable)) {
            qCritical() << "Failed to create table:" << query.lastError().text();
            return false;
        }
        
        query.exec("CREATE INDEX IF NOT EXISTS idx_source ON embeddings(source_file)");
        
        return true;
    }
    
    bool saveEmbedding(const QString &sourceFile, const QString &itemId, 
                      const QString &content, const QVector<float> &embedding);
    int count();
    
private:
    QSqlDatabase m_db;
};


class RemoteEmbedder
{
public:
    RemoteEmbedder(const RemoteEmbedConfig &config)
        : m_config(config), m_manager(new QNetworkAccessManager())
    {
        QString jsonDir = QDir::homePath() + "/source/llama-embedder/json";
        
        //bool embedFile(const QString &inputPath);
        if (!QDir(jsonDir).exists()) {
            qCritical() << "JSON directory not found:" << jsonDir;
        }
        
        // Configure llama-swap embedding server
        RemoteEmbedConfig embedConfig;
        embedConfig.enabled = true;
        embedConfig.baseUrl = "http://127.0.0.1:8080";  // llama-swap default port
        embedConfig.model = "nomic-embed";  // Model name (llama-swap will route to nomic-embed)
        embedConfig.timeout = 30000;
        
        qDebug() << "Using llama-swap embedding server:" << embedConfig.baseUrl;
        qDebug() << "Model:" << embedConfig.model;
        
        EmbeddingDatabase db("embeddings.db");
        RemoteEmbedder remoteEmbed(embedConfig);
    
        QTimer::singleShot(0, [&]() {
            processAllFiles();
        });

        QString m_jsonDir;

    }
    
    QString extractTextFromJson(const QJsonValue &value, const QStringList &keys);
    void extractTextRecursive(const QJsonValue &value, const QStringList &keys, QStringList &texts);
    bool embedFile(const QString &inputPath);
    bool embedAndSave(const QString &text, const QString &sourcePath, const QString &itemId);
    void processAllFiles();

    ~RemoteEmbedder()
    {
        delete m_manager;
    }
    
    QVector<float> generateEmbedding(const QString &text);

private:
    QString m_jsonDir;
    EmbeddingDatabase *m_db;
    RemoteEmbedConfig m_config;
    QNetworkAccessManager *m_manager;
};

#endif // EMBEDDER_H
