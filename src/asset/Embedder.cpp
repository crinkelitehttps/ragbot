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
Embedder::Embedder(const QJsonObject& config)
    : m_db(new EmbeddingDatabase(config))
    , m_parser(new ParserJSON())
    , m_files(config.value("files").toString())
    , m_isValid(true)
{
    qDebug() << "Embedder::Embedder()" << m_files;

    if (config.isEmpty()) {
        qDebug() << "Embedder::Embedder() [ no valid config ]";
        return;
    }

    const auto& name = config.value("name").toString();
    if (name.isEmpty()) {
        qWarning() << "Embedder::Embedder() [ invalid config ]";
    }
    
    const auto generatorConfig = config.value("generator").toObject();

    if (config.value("isNetworkHost").toBool()) {
        m_generator = new GeneratorIP(generatorConfig);
    } else {
        m_generator = new GeneratorImmediate(generatorConfig);
    }

    if (m_generator) {
        processAllFiles();
        return;
    }

    qWarning() << "Embedder::Embedder() [ failed to create the generator ]";
}


//--------------------------------------------------------------------------------
void Embedder::processAllFiles()
{
    int total = 0, processed = 0;
    
    QDirIterator countIt(
            m_files,
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
            m_files,
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
        

        fileEmbed(filePath);
    }
    
    qDebug() << "Embedder::processAllFiles(): Complete! Processed" 
        << processed
        << "files";

};


//--------------------------------------------------------------------------------
void Embedder::fileEmbed(const QString &sourcePath) 
{
    if (m_db->isEmbedded(sourcePath)) {
        qDebug() << "Embedder::embedFile() already embedded" << sourcePath;
        return;
    };
    
    QFile sourceFile(sourcePath);

    if (sourceFile.open(QIODevice::ReadOnly)) {
        const auto fileChunks = m_parser->toChunks(sourceFile.readAll());
        for (const auto &chunk : fileChunks) {
            const auto embedding = m_generator->generate(chunk);
            if(m_db->saveEmbedding(embedding, sourcePath, chunk)) {
                qDebug() << "Embedder::embedderFile() [ saveEmbeeding returned true ]";
                return;
            };
        };
        sourceFile.close();
    } else {
        qWarning() << "Embedder::embedderFile() [ saveEmbeeding returned false ]";
    };

};


