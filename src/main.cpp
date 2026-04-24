#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QThread>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

#include "asset/Embedder.h"
#include "db/RoleplayDatabase.h"
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

    const QCommandLineOption skipIndexOpt(
        {"s", "skip-index"},
        "Skip the embedding/indexing pass and go straight to the chat loop using whatever is already in the database.");

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
        QJsonObject embedderConfig = root.value("embedder").toObject();
        if (cli.isSet(dataOpt)) {
            embedderConfig["files"] = cli.value(dataOpt);
            qDebug() << "main: data directory overridden to" << cli.value(dataOpt);
        }
        if (cli.isSet(dbOpt)) {
            embedderConfig["name"] = cli.value(dbOpt);
            qDebug() << "main: database path overridden to" << cli.value(dbOpt);
        }
        if (cli.isSet(skipIndexOpt)) {
            embedderConfig["skipIndex"] = true;
        }
        root["embedder"] = embedderConfig;
    }

    const bool loadOnly = cli.isSet(loadOpt);

    // All heavy objects (and their QNetworkAccessManagers) live on the worker thread.
    // The main thread runs app.exec() unblocked; the worker blocks on stdin between turns.
    QThread* worker = QThread::create([root, loadOnly]() {
        Embedder embedder(root.value("embedder").toObject());
        if (!embedder.isValid()) {
            qWarning() << "main: failed to construct embedder";
            QCoreApplication::exit(1);
            return;
        }
        qDebug() << "main: embedder ready";

        if (loadOnly) {
            qDebug() << "main: load-only mode complete";
            QCoreApplication::quit();
            return;
        }

        Researcher researcher(root.value("researcher").toObject());
        qDebug() << "main: researcher ready";

        Roleplayer roleplayer(root.value("roleplayer").toObject());
        qDebug() << "main: roleplayer ready";

        RoleplayDatabase roleplayDb(root.value("conversationsDb").toString("conversations.db"));
        qDebug() << "main: roleplay database ready";

        const bool enableRoleplay = root.value("enableRoleplay").toBool(false);
        RAGBot ragbot(embedder, researcher, roleplayer, roleplayDb, enableRoleplay);
        ragbot.start();
    });

    QObject::connect(worker, &QThread::finished, &app, &QCoreApplication::quit);
    worker->start();

    const int exitCode = app.exec();
    worker->wait();
    delete worker;
    return exitCode;
}
