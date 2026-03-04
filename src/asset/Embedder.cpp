// Modified to use local llama-swap embedding server
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
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

//    QString jsonDir = QDir::homePath() + "/source/Cataclysm-DDA/data/json";
    QString jsonDir = QFileInfo(config.sourceFiles).isAbsolute()
        ? config.sourceFiles
        : QDir::homePath() + config.sourceFiles;
    if (!QDir(jsonDir).exists()) {
        qCritical() << "Embedder::Embedder(): JSON directory not found:" << jsonDir;
    }
    m_config.sourceFiles = jsonDir;
    processAllFiles();
}


//--------------------------------------------------------------------------------
void Embedder::processAllFiles()
{
    int total = 0, processed = 0;
    
    QDirIterator countIt(
            m_config.sourceFiles,
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
            m_config.sourceFiles,
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
    if (m_embedDB.isEmbedded(sourcePath)) {
        qDebug() << "Embedder::embedFile() already embedded" << sourcePath;
        return;
    };
    
    QFile sourceFile(sourcePath);

    if (sourceFile.open(QIODevice::ReadOnly)) {
        const auto fileContents = sourceFile.readAll();
        const auto extractedText = m_parser->extractText(fileContents);

        qDebug() << "Embedder::embedFile()" << extractedText;
        const auto embedding = m_generator->generate(extractedText);
        sourceFile.close();
        if(m_embedDB.saveEmbedding(embedding, sourcePath, extractedText)) {
            qDebug() << "Embedder::embedderFile() [ saveEmbeeding returned true ]";
            return;
        };
        qWarning() << "Embedder::embedderFile() [ saveEmbeeding returned false ]";
    };

};
