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
    : m_db(EmbeddingDatabase(config))
    , m_files(config.value("files").toString())
    , m_isValid(true)
    , m_parser(new ParserJSON())
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

    if (m_generator != nullptr) {
        processAllFiles();
        return;
    }

    qWarning() << "Embedder::Embedder() [ failed to create the generator ]";
}


//--------------------------------------------------------------------------------
void Embedder::processAllFiles()
{
    int total = 0;
    int processed = 0;
    
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
    
    QDirIterator dIt(
            m_files,
            QStringList()
            << "*.json",
            QDir::Files,
            QDirIterator::Subdirectories
    );
    
    while (dIt.hasNext()) {
        QString filePath = dIt.next();
        QFileInfo fileInfo(filePath);
        
        processed++;

        qDebug() << QString("[%1/%2] %3").arg(processed)
            .arg(total)
            .arg(fileInfo.fileName());
        

        QFile file(filePath);
        fileEmbed(file);
    }
    
    qDebug() << "Embedder::processAllFiles(): Complete! Processed" 
        << processed
        << "files";

};


//--------------------------------------------------------------------------------
auto Embedder::fileEmbed(QFile& file) -> void
{
    
    if (file.open(QIODevice::ReadOnly)) {

        const auto fileData = file.readAll();
        const auto fileName = file.fileName();
        QCryptographicHash hash(QCryptographicHash::Md5);
        hash.addData(fileData);
        const auto fileHash = hash.result().toHex();

        const auto newSourceFileId = m_db.newSourceFileId(fileHash, fileName);
        qDebug() << "Embedder::fileEmbed() " << fileHash << newSourceFileId;
        if (newSourceFileId > 0) {
            qDebug() << "Embedder::fileEmbed() new:" << fileHash << fileName;
            return;
        };

        for (const auto& chunk : m_parser->toChunks(QVariant(QString::fromUtf8(fileData)))) {
            const auto embedding = m_generator->generate(chunk);

            Q_ASSERT(chunk.length());
            Q_ASSERT(embedding.length());

            m_db.embeddingSave(newSourceFileId, embedding, chunk); 
        };

        file.close();

    } else {
        qWarning() << "Embedder::embedderFile() [ saveEmbeeding returned false ]";
    };

};


//--------------------------------------------------------------------------------
auto Embedder::generationEmbed(const QString &generation) -> void
{
    qDebug() << "Embedder::generationEmbed() [ not implemented ]" << generation.length(); 
};
