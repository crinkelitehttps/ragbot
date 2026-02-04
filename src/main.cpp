#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <QDebug>
#include "RAGBot.h"
#include "db/EmbeddingDatabase.h"
#include "db/ConversationDatabase.h"
#include "llm/RemoteLLMConfig.h"
#include "RoleplayConfig.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    QString embedModelPath = QDir::homePath() + "/.ollama/models/blobs/nomic-embed-text-v1.5.f32.gguf";
    QString dbPath = "embeddings.db";
    QString convDbPath = "conversations.db";

    bool isLocal{};

    QString model;

    for (int i = 1; i < argc; i++) {
        if (QString(argv[i]).startsWith("-m") && argv[i+1]) {
            i++;
            qDebug() << "got arg" << argv[i];
            model = argv[i];
            break;
        } 
        if (QString(argv[i]).startsWith("-l")) {
            isLocal = true;
            break;
        };
    }

    isLocal = true;

    const QString port = "8080";
    const QString host = isLocal ? "127.0.0.1" : "192.168.0.97";
    const QString url = QString("http://%1:%2/upstream/%3/").arg(host).arg(port).arg(model);
    
    if (!QFile::exists(embedModelPath)) {
        qCritical() << "Embedding model not found:" << embedModelPath;
        return 1;
    }
    
    if (!QFile::exists(dbPath)) {
        qCritical() << "Database not found:" << dbPath;
        return 1;
    }
    
    // Configure research LLM
    RemoteLLMConfig llmConfig;
    llmConfig.enabled = true;
    llmConfig.baseUrl = url;
    llmConfig.model = model;
    llmConfig.timeout = 4 * 60000;
    
    // Configure roleplay
    RoleplayConfig rpConfig;
    rpConfig.enabled = true;
    rpConfig.characterName = "Survivor";
    
#if 0
    if (!rpConfig.loadFromFile("characterBackground.txt")) {
        qWarning() << "Failed to load character background from file, using default";
        rpConfig.characterBackground = "";
    }
#endif

    rpConfig.baseUrl = url;
    rpConfig.model = model;
    
    EmbeddingDatabase db(dbPath);
    ConversationDatabase convDb(convDbPath);
    RAGBot bot(embedModelPath, &db, &convDb, llmConfig, rpConfig);
    
    QTimer::singleShot(0, [&bot]() {
        bot.startChatLoop();
    });
    
    return app.exec();
}
