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
    , m_embed_db("embeddings.db")
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
        
        if (m_embed_db.isEmbedded(filePath)) {
            qDebug() << "Embedder::embedFile(): isEmbedded" << true;
            return;
        } else {
            qDebug() << "Embedder::embedFile(): isEmedded" << false;
        };

        if (!embedFile(filePath)) {
            qWarning() << "Embedder::processAllFiles(): Failed:" << filePath;
        }
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
Parser* Embedder::initParser()
{
    return static_cast<ParserJSON*>(new ParserJSON());
}; 


//--------------------------------------------------------------------------------
bool Embedder::embedFile(const QString &sourcePath) 
{
    qDebug() << "Embedder::embedFile()" << sourcePath;
    
    if (m_embed_db.isEmbedded(sourcePath)){
        qDebug() << "Embedder::updateOrCreate() " << sourcePath;
        return true;
    }

    QFile file(sourcePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (doc.isNull()) return false;
    
    if (doc.isArray()) {
        QJsonArray arr = doc.array();
        std::atomic<int> success = 0;
        for (int i = 0; i < arr.size(); i++) {
            QJsonValue item = arr[i];
            QString itemId = "item_" + QString::number(i);
            
            if (item.isObject() && item.toObject().contains("id")) {
                itemId = item.toObject()["id"].toString();
            }
            
            QString text = m_parser->extractTextFromJson(item);  // No keys parameter
            if (text.isEmpty()) text = "empty";
            
            if (updateOrCreate(text, sourcePath, itemId)) {
                success++;
            }
        }
        
        qDebug() << "  Saved" << success << "items";
        return success > 0;
    } else {
        QString text = m_parser->extractTextFromJson(doc.object());  // No keys parameter
        if (text.isEmpty()) text = "empty";
        return updateOrCreate(text, sourcePath, "");
    }
};


//--------------------------------------------------------------------------------
bool Embedder::updateOrCreate (
        const QString &text,
        const QString &sourcePath,
        const QString &itemId
    ) 
{

    QVector<float> embedding = m_generator->generate(text);
    if (embedding.isEmpty()) {
        return false;
    }
    return m_embed_db.saveEmbedding(sourcePath, itemId, text, embedding);
};


