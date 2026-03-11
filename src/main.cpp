#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include "asset/Embedder.h"

#include "RAGBot.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);


    QDir workingDir;
    qDebug() << "main working dir" << workingDir.absoluteFilePath(".");

    QFile configFile("config.json");
    if (!configFile.open(QIODevice::ReadOnly)) {
        qWarning() << "main cannot open config.json";
        return 1;
    }

    const auto& doc = QJsonDocument::fromJson(configFile.readAll());
    if (doc.isObject()) {
        QJsonObject root = doc.object();
        
        Embedder embedder(root.value("embedder").toObject());
        if (!embedder.isValid()) {
            qWarning() << "failed to cosntruct embedder";
            return 1;
        }
        qDebug() << "main embedder object";
        
        Researcher researcher(root.value("researcher").toObject());
        qDebug() << "main researcher object";
        
        Roleplayer roleplayer(root.value("roleplayer").toObject());
        qDebug() << "main roleplayer object";
        RAGBot ragbot(embedder, researcher, roleplayer);
        QTimer::singleShot(0, [&ragbot]() {
            ragbot.init();
        });
    }
    configFile.close();
    return app.exec();
}


