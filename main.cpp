#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <QDebug>
#include "RAGBot.h"
#include "EmbeddingDatabase.h"
#include "ConversationDatabase.h"
#include "RemoteLLMConfig.h"
#include "RoleplayConfig.h"

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
    
    // Configure research LLM
    RemoteLLMConfig llmConfig;
    llmConfig.enabled = true;
    llmConfig.baseUrl = "http://192.168.0.97:8080/upstream/llama-3.2-8b-instruct";
    llmConfig.model = "llama-3.2-8b-instruct";
    llmConfig.timeout = 4 * 60000;
    
    // Configure roleplay
    RoleplayConfig rpConfig;
    rpConfig.enabled = true;
    rpConfig.characterName = "Survivor";
    
    if (!rpConfig.loadFromFile("characterBackground.txt")) {
        qWarning() << "Failed to load character background from file, using default";
        rpConfig.characterBackground = "You are a survivor in the post-apocalyptic world of Cataclysm: Dark Days Ahead.";
    }
    
    rpConfig.baseUrl = "http://192.168.0.97:8080/upstream/llama-3.2-8b-instruct";
    rpConfig.model = "llama-3.2-8b-instruct";
    
    EmbeddingDatabase db(dbPath);
    ConversationDatabase convDb(convDbPath);
    RAGBot bot(embedModelPath, &db, &convDb, llmConfig, rpConfig);
    
    QTimer::singleShot(0, [&bot]() {
        bot.startChatLoop();
    });
    
    return app.exec();
}
