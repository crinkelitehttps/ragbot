#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

#include "asset/Embedder.h"
#include "RAGBot.h"

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

    cli.addOption(configOpt);
    cli.addOption(dataOpt);
    cli.addOption(dbOpt);
    cli.addOption(loadOpt);
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
        QJsonObject embedderConfig = root.value("embedder").toObject();
        if (cli.isSet(dataOpt)) {
            embedderConfig["files"] = cli.value(dataOpt);
            qDebug() << "main: data directory overridden to" << cli.value(dataOpt);
        }
        if (cli.isSet(dbOpt)) {
            embedderConfig["name"] = cli.value(dbOpt);
            qDebug() << "main: database path overridden to" << cli.value(dbOpt);
        }
        root["embedder"] = embedderConfig;
    }

    Embedder embedder(root.value("embedder").toObject());
    if (!embedder.isValid()) {
        qWarning() << "main: failed to construct embedder";
        return 1;
    }
    qDebug() << "main: embedder ready";

    // -l / --load: finish once the index is built, don't start the chat loop.
    if (cli.isSet(loadOpt)) {
        qDebug() << "main: load-only mode complete";
        return 0;
    }

    Researcher researcher(root.value("researcher").toObject());
    qDebug() << "main: researcher ready";

    Roleplayer roleplayer(root.value("roleplayer").toObject());
    qDebug() << "main: roleplayer ready";

    RAGBot ragbot(embedder, researcher, roleplayer);

    // Defer start() until after the event loop is running so that
    // QNetworkAccessManager (used inside generators) works correctly.
    QTimer::singleShot(0, [&ragbot]() { ragbot.start(); });

    return app.exec();
}
