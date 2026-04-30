#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

#include "RAGBot.h"
#include "RAGBotSession.h"
#include "ConfigKeys.h"
#include "compat/Json.h"

auto main(int argc, char *argv[]) -> int
{
    // Prevent Qt Network's bearer management from loading system Qt DBus libs,
    // which would conflict with this binary's Qt 5.15.2 build.
    qputenv("QT_BEARER_POLL_TIMEOUT", "2147483647");

    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("ragbot");

    QCommandLineParser cli;
    cli.setApplicationDescription("RAG chatbot for Cataclysm: Dark Days Ahead data");
    cli.addHelpOption();

    const QCommandLineOption configOpt(
        {"c", "config"},
        "Path to config JSON file (default: config.json in working directory).",
        "file", "config.json");

    const QCommandLineOption dataOpt(
        {"d", "data"},
        "Data directory to index — overrides embedder.files in config.",
        "dir");

    const QCommandLineOption dbOpt(
        {"b", "db"},
        "Path to the embeddings database file (default: embeddings.db in working directory).",
        "file");

    const QCommandLineOption loadOpt(
        {"l", "load"},
        "Index mode: build the embedding database from the data directory, then exit.");

    const QCommandLineOption skipIndexOpt(
        {"s", "skip-index"},
        "Skip the embedding/indexing pass and go straight to the chat loop.");

    cli.addOption(configOpt);
    cli.addOption(dataOpt);
    cli.addOption(dbOpt);
    cli.addOption(loadOpt);
    cli.addOption(skipIndexOpt);
    cli.process(app);

    qDebug() << "main working dir" << QDir().absolutePath();

    const QString configPath = cli.value(configOpt);
    QFile configFile(configPath);
    if (!configFile.open(QIODevice::ReadOnly)) {
        qWarning() << "main: cannot open config file:" << configPath;
        return 1;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
    configFile.close();

    if (!doc.isObject()) {
        qWarning() << "main: config is not a JSON object:" << configPath;
        return 1;
    }

    QJsonObject root = doc.object();

    // Apply command-line overrides on top of the config file values.
    {
        QJsonObject embedderConfig = root.value(ConfigKeys::Embedder).toObject();
        if (cli.isSet(dataOpt)) {
            embedderConfig[ConfigKeys::Files] = cli.value(dataOpt);
            qDebug() << "main: data directory overridden to" << cli.value(dataOpt);
        }
        if (cli.isSet(dbOpt)) {
            embedderConfig[ConfigKeys::DbName] = cli.value(dbOpt);
            qDebug() << "main: database path overridden to" << cli.value(dbOpt);
        }
        if (cli.isSet(skipIndexOpt)) {
            embedderConfig[ConfigKeys::SkipIndex] = true;
        }
        root[ConfigKeys::Embedder] = embedderConfig;
    }

    const bool loadOnly = cli.isSet(loadOpt);

    RAGBotSession session(rb::Json(root), loadOnly);
    if (!session.isValid()) return 1;
    if (loadOnly) return 0;

    RAGBot ragbot(session);
    ragbot.start();
    return 0;
}
