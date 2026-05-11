#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include "db.h"
#include "ragbot.h"

// Configuration for remote LLM
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    QString embedModelPath = QDir::homePath() + "/.ollama/models/blobs/nomic-embed-text-v1.5.f32.gguf";
    QString dbPath = "embeddings.db";
    QString convDbPath = "conversations.db";
    
    if (!QFile::exists(embedModelPath)) {
        qCritical() << "Embedding model not found:" << embedModelPath;
        return 1;
    }
    
    if (!QFile::exists(dbPath)) {
        qCritical() << "Database not found:" << dbPath;
        return 1;
    }
    
    bool isLocal{ true };
    QString host;
    QString model;

    for (int i = 1; i < argc; i++) {
        if (QString(argv[i]).startsWith("-m") && argv[i+1]) {
            i++;
            qDebug() << "got arg" << argv[i];
            model = argv[i];
            break;
        }
        if (QString(argv[i]).startsWith("-l")) {
            qDebug() << "is local";
            isLocal = true;
            break;
        }
    }
    if (model.isEmpty()) {
        qDebug() << "did not get model arg";
        return 0;
    }

    // Configure research LLM
    RemoteLLMConfig llmConfig;

    if (isLocal) {
        host = "http://127.0.0.1:8080/upstream/";
    } else {
        host = "http://192.168.0.97:8080/upstream/";
    }

    const auto url = model;

    llmConfig.enabled = true;
    llmConfig.baseUrl = QStringLiteral("%1%2").arg(host).arg(model);
    llmConfig.model = model;

    llmConfig.timeout = 4 * 60000;
    
    // Configure roleplay
    RoleplayConfig rpConfig;
    rpConfig.enabled = true;
    rpConfig.characterName = "Survivor";
    
    if (model.isEmpty()) {
        rpConfig.baseUrl = QString("%1%2").arg(host).arg(model);
        rpConfig.model = model;
    } else {
        rpConfig.baseUrl = "http://127.0.0.1:8080/upstream/llama-3.2-1b";
        rpConfig.model = "llama-3.2-1b";
    }

    
    EmbeddingDatabase db(dbPath);
    ConversationDatabase convDb(convDbPath);
    RAGBot bot(embedModelPath, &db, &convDb, llmConfig, rpConfig);
    
    QTimer::singleShot(0, [&bot]() {
        bot.startChatLoop();
    });
    
    return app.exec();
}
