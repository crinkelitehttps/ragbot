#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include "config/ConfigResearch.h"
#include "config/ConfigRoleplay.h"
#include "config/ConfigEmbed.h"
#include "config/ConfigGenerator.h"

#include "asset/Embedder.h"

#include "RAGBot.h"

static void applyGeneratorConfig(const QJsonObject &obj, ConfigGenerator &cfg)
{
    if (obj.contains("basePath") && obj["basePath"].isString()) {
        cfg.basePath = obj["basePath"].toString();
    }
    if (obj.contains("modelName") && obj["modelName"].isString()) {
        cfg.modelName = obj["modelName"].toString();
    }
    if (obj.contains("timeout") && obj["timeout"].isDouble()) {
        cfg.timeout = obj["timeout"].toInt();
    }
    if (obj.contains("isImmediate") && obj["isImmediate"].isBool()) {
        cfg.isImmediate = obj["isImmediate"].toBool();
    }
}

static void applyEmbedConfig(const QJsonObject &obj, ConfigEmbed &cfg)
{
    if (obj.contains("dbName") && obj["dbName"].isString()) {
        cfg.dbName = obj["dbName"].toString();
    }
    if (obj.contains("sourceFiles") && obj["sourceFiles"].isString()) {
        cfg.sourceFiles = obj["sourceFiles"].toString();
    }
    if (obj.contains("generator") && obj["generator"].isObject()) {
        applyGeneratorConfig(obj["generator"].toObject(), cfg.generatorConfig);
    }
    if (obj.contains("generatorConfig") && obj["generatorConfig"].isObject()) {
        applyGeneratorConfig(obj["generatorConfig"].toObject(), cfg.generatorConfig);
    }
}

static void applyResearchConfig(const QJsonObject &obj, ConfigResearch &cfg)
{
    if (obj.contains("instruction") && obj["instruction"].isString()) {
        cfg.instruction = obj["instruction"].toString();
    }
    if (obj.contains("generator") && obj["generator"].isObject()) {
        applyGeneratorConfig(obj["generator"].toObject(), cfg.generatorConfig);
    }
    if (obj.contains("generatorConfig") && obj["generatorConfig"].isObject()) {
        applyGeneratorConfig(obj["generatorConfig"].toObject(), cfg.generatorConfig);
    }
}

static void applyRoleplayConfig(const QJsonObject &obj, ConfigRoleplay &cfg)
{
    if (obj.contains("characterName") && obj["characterName"].isString()) {
        cfg.characterName = obj["characterName"].toString();
    }
    if (obj.contains("characterBackground") && obj["characterBackground"].isString()) {
        cfg.characterBackground = obj["characterBackground"].toString();
    }
    if (obj.contains("generator") && obj["generator"].isObject()) {
        applyGeneratorConfig(obj["generator"].toObject(), cfg.generatorConfig);
    }
    if (obj.contains("generatorConfig") && obj["generatorConfig"].isObject()) {
        applyGeneratorConfig(obj["generatorConfig"].toObject(), cfg.generatorConfig);
    }
}

static void loadConfigFromJson(
    const QString &path,
    ConfigEmbed &embedConfig,
    ConfigResearch &researchConfig,
    ConfigRoleplay &roleplayConfig
)
{
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open config file:" << path;
        return;
    }

    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "Failed to parse config JSON:" << error.errorString();
        return;
    }

    const QJsonObject root = doc.object();
    if (root.contains("embedder") && root["embedder"].isObject()) {
        applyEmbedConfig(root["embedder"].toObject(), embedConfig);
    }
    if (root.contains("researcher") && root["researcher"].isObject()) {
        applyResearchConfig(root["researcher"].toObject(), researchConfig);
    }
    if (root.contains("roleplayer") && root["roleplayer"].isObject()) {
        applyRoleplayConfig(root["roleplayer"].toObject(), roleplayConfig);
    }
}

static void finalizeGeneratorDefaults(
    ConfigGenerator &cfg,
    const QString &modelOverride,
    const QString &host,
    const QString &port
)
{
    if (cfg.modelName.isEmpty() && !modelOverride.isEmpty()) {
        cfg.modelName = modelOverride;
    }
    if (cfg.basePath.isEmpty() && !cfg.modelName.isEmpty()) {
        cfg.basePath = QString("http://%1:%2/upstream/%3/")
            .arg(host)
            .arg(port)
            .arg(cfg.modelName);
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    QString model;
    QString embedderInputsPath;
    QString configPath;
    bool isLocal {};
    bool isEmbedMode {};

    for (int i = 1; i < argc; i++) {
        if (QString(argv[i]).startsWith("-m") && argv[i+1]) {
            i++;
            qDebug() << "Using model" << argv[i];
            model = argv[i];
        } else if (QString(argv[i]).startsWith("-d") && argv[i+1]){
            i++;
            qDebug() << "embedder inputs path" << argv[i];
            embedderInputsPath = argv[i];
        } else if (QString(argv[i]).startsWith("-l")) {
            qDebug() << "Running in local mode";
            isLocal = true;
        } else if (QString(argv[i]).startsWith("--embed")) {
            qDebug() << "Running in embdedding mode";
            isEmbedMode = true;
        } else if ((QString(argv[i]) == "-c" || QString(argv[i]) == "--config") && argv[i+1]) {
            i++;
            qDebug() << "Using config file" << argv[i];
            configPath = argv[i];
        };
    }

    const QString port = "8080";
    const QString host = isLocal ? "127.0.0.1" : "192.168.0.97";

    ConfigEmbed embedConfig;
    ConfigResearch researchConfig;
    ConfigRoleplay roleplayConfig;

    embedConfig.dbName = "embeddings.db";
    roleplayConfig.characterName = "Survivor";
    roleplayConfig.characterBackground = "PLACEHOLDER BACKGROUN";

    loadConfigFromJson(configPath, embedConfig, researchConfig, roleplayConfig);

    if (!embedderInputsPath.isEmpty()) {
        embedConfig.sourceFiles = embedderInputsPath;
    }

    finalizeGeneratorDefaults(embedConfig.generatorConfig, model, host, port);
    finalizeGeneratorDefaults(researchConfig.generatorConfig, model, host, port);
    finalizeGeneratorDefaults(roleplayConfig.generatorConfig, model, host, port);

    embedConfig.generatorConfig.timeout =
        embedConfig.generatorConfig.timeout > 0 ? embedConfig.generatorConfig.timeout : 4 * 60000;
    researchConfig.generatorConfig.timeout =
        researchConfig.generatorConfig.timeout > 0 ? researchConfig.generatorConfig.timeout : 4 * 60000;
    roleplayConfig.generatorConfig.timeout =
        roleplayConfig.generatorConfig.timeout > 0 ? roleplayConfig.generatorConfig.timeout : 4 * 60000;

    if (isEmbedMode) {
        qInfo() << "Creating Embedder in main";
        Embedder embedder(embedConfig);
    }

    RAGBot ragbot(embedConfig, researchConfig, roleplayConfig);
    
    QTimer::singleShot(0, [&ragbot]() {
        ragbot.startChatLoop();
    });
    
    return app.exec();
}
