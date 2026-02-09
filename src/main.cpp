#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <QDebug>

#include "config/ConfigResearch.h"
#include "config/ConfigRoleplay.h"
#include "config/ConfigEmbed.h"
#include "config/ConfigGenerator.h"

#include "Embedder.h"

#include "RAGBot.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
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

    ConfigGenerator embedGeneratorConfig;
    embedGeneratorConfig.basePath = url;
    embedGeneratorConfig.modelName = model;
    embedGeneratorConfig.timeout = 4 * 60000;

    ConfigEmbed embedConfig;
    embedConfig.dbName = "embeddings.db";
    embedConfig.generatorConfig = embedGeneratorConfig;
    
    if(isEmbedMode) {
        qInfo() << "Creating Embedder in main";
        Embedder embedder(embedConfig);
    };

    ConfigResearch researchConfig;
    ConfigGenerator researchGeneratorConfig;
    researchGeneratorConfig.basePath = url;
    researchGeneratorConfig.modelName = model;
    researchGeneratorConfig.timeout = 4 * 60000;
    researchConfig.generatorConfig = researchGeneratorConfig;
    
    
    ConfigRoleplay roleplayConfig;
    ConfigGenerator roleplayGeneratorConfig;
    roleplayConfig.characterName = "Survivor";
    roleplayConfig.characterBackground = "PLACEHOLDER BACKGROUN";
    roleplayConfig.generatorConfig = roleplayGeneratorConfig;
    
    RAGBot ragbot(embedConfig, researchConfig, roleplayConfig);
    
    QTimer::singleShot(0, [&ragbot]() {
        ragbot.startChatLoop();
    });
    
    return app.exec();
}
