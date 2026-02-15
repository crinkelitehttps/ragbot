// Modified to use local llama-swap embedding server
#include <QDir>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QTimer>
#include "Embedder.h"
#include "../parsers/ParserJSON.h"
#include "../generation/GeneratorImmediate.h"
#include "../generation/GeneratorIP.h"


//--------------------------------------------------------------------------------
Embedder::Embedder(ConfigEmbed config)
    : m_config(config)
    , m_embedDB("embeddings.db")
    , m_generator(initGenerator())
    , m_parser(new ParserJSON())
{
    qDebug() << "Embedder::Embedder()";

    QString jsonDir = QDir::homePath() + "/source/Cataclysm-DDA/data/json";
    if (!QDir(jsonDir).exists()) {
        qCritical() << "Embedder::Embedder(): JSON directory not found:" << jsonDir;
    }
    processAllFiles();
}


//--------------------------------------------------------------------------------
void Embedder::processAllFiles()
{
    qDebug() << "Embedder::processAllFiles()";
    int total = 0, processed = 0;

    const auto jsonDir = "/home/joe/source/Cataclysm-DDA/data/json";
    
    QDirIterator countIt(
            jsonDir,
            QStringList()
            << "*.json",
            QDir::Files,
            QDirIterator::Subdirectories
    );

    while (countIt.hasNext()) {
        countIt.next();
        total++;
    }
    
    qDebug() << "Embedder::processAllFiles(): Found" << total << "JSON files";
    
    QDirIterator it(
            jsonDir,
            QStringList()
            << "*.json",
            QDir::Files,
            QDirIterator::Subdirectories
    );
    
    while (it.hasNext()) {
        QString filePath = it.next();
        QFileInfo fileInfo(filePath);
        
        processed++;

        qDebug() << QString("[%1/%2] %3").arg(processed)
            .arg(total)
            .arg(fileInfo.fileName());
        

        embedFile(filePath);
    }
    
    qDebug() << "Embedder::processAllFiles(): Complete! Processed" 
        << processed
        << "files";

};


//--------------------------------------------------------------------------------
Generator* Embedder::initGenerator()
{
    auto driverLoaded = [](Generator* generator) {
        if (generator->isValid() ) {
            return generator;
        }
        delete generator;
        return static_cast<Generator*>(nullptr);
    };

    if (auto* generator = driverLoaded(new GeneratorImmediate(m_config.generatorConfig))) {
        qDebug() << "Embedder::initGenerator() [ GeneratorImmediate ]";
        return generator;
    }
    if (auto* generator = driverLoaded(new GeneratorIP(m_config.generatorConfig))) {
        qDebug() << "Embedder::initGenerator() [ GeneratorIP ]";
        return generator;
    }
    qDebug() << "Embedder::initGenerator() [ nullptr ]";

    return static_cast<Generator*>(nullptr);
}; 


//--------------------------------------------------------------------------------
void Embedder::embedFile(const QString &sourcePath) 
{
    qDebug() << "Embedder::embedFile()" << sourcePath;
    
    QFile file(sourcePath);

    if (!file.open(QIODevice::ReadOnly)) { 
        qWarning() << "Embedder::embedFile() [ could not read file ]" << sourcePath;
    };

    if (m_embedDB.createSourceRecord(sourcePath)) {
        qDebug() << "Embedder::embedFile() created " << sourcePath;
        QFile sourceFile(sourcePath);

        if (sourceFile.open(QIODevice::ReadOnly)) {
            const auto embedding = m_generator->generate(sourceFile.readAll());
            if(m_embedDB.saveEmbedding(embedding)) {
                qDebug() << "Embedder::embedderFile() [ saveEmbeeding returned true ]";
            };
            qWarning() << "Embedder::embedderFile() [ saveEmbeeding returned false ]";
        };

    } else {
        qDebug() << "Emedder::embedFile() [ embedding likely exists ]";
    };

};

