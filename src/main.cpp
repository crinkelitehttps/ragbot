#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <QDebug>

#include "config/LLMConfig.h"
#include "config/LLMConfigRoleplay.h"
#include "db/EmbeddingDatabase.h"
#include "db/ConversationDatabase.h"
#include "Embedder.h"
#include "RAGBot.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    QString dbPath = "embeddings.db";
    QString convDbPath = "conversations.db";

    QString model;
    bool isLocal {};
    bool isEmbedMode {};

    for (int i = 1; i < argc; i++) {
        if (QString(argv[i]).startsWith("-m") && argv[i+1]) {
            i++;
            qDebug() << "Using model" << argv[i];
            model = argv[i];
        } else if (QString(argv[i]).startsWith("-l")) {
            qDebug() << "Running in local mode";
            isLocal = true;
        } else if (QString(argv[i]).startsWith("--embed")) {
            qDebug() << "Running in embdedding mode";
            isEmbedMode = true;
        };
    }

    const QString port = "8080";
    const QString host = isLocal ? "127.0.0.1" : "192.168.0.97";
    const QString url = QString("http://%1:%2/upstream/%3/").arg(host).arg(port).arg(model);
    
    if(isEmbedMode) {
        EmbedConfig embedConfig;
        embedConfig.enabled = true;
        embedConfig.baseUrl = url;
        embedConfig.model = model;
        embedConfig.timeout = 4 * 60000;
        qInfo() << "Creating Embedder in main";
        Embedder embedder(embedConfig);
    };
    
    // Configure research LLM
    LLMConfig researchConfig;
    researchConfig.enabled = true;
    researchConfig.baseUrl = url;
    researchConfig.model = model;
    researchConfig.timeout = 4 * 60000;
    
    // Configure roleplay
    LLMConfigRoleplay roleplayConfig;
    roleplayConfig.enabled = true;
    roleplayConfig.characterName = "Survivor";
    
    if (!roleplayConfig.loadFromFile("characterBackground.txt")) {
        qWarning() << "Failed to load character background from file, using default";
        roleplayConfig.characterBackground = "";
    }

    roleplayConfig.baseUrl = url;
    roleplayConfig.model = model;
    
    EmbeddingDatabase db(dbPath);
    ConversationDatabase convDb(convDbPath);
    RAGBot ragbot("placeholder", &db, &convDb, researchConfig, roleplayConfig);
    
    QTimer::singleShot(0, [&ragbot]() {
        ragbot.startChatLoop();
    });
    
    return app.exec();
}
