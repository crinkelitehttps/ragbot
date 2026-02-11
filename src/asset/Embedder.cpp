// Modified to use local llama-swap embedding server
#include <QDir>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QTimer>
#include "Embedder.h"
#include "../generation/GeneratorImmediate.h"
#include "../generation/GeneratorIP.h"


//--------------------------------------------------------------------------------
Embedder::Embedder(ConfigEmbed config)
    : m_config(config)
    , m_embed_db("embeddings.db")
    , m_generator(initGenerator())
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
    {
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
            
            if (!embedFile(filePath)) {
                qWarning() << "Embedder::processAllFiles(): Failed:" << filePath;
            }
        }
        
        qDebug() << "Embedder::processAllFiles(): Complete! Processed" 
            << processed
            << "files";
    }

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
bool Embedder::embedAndSave(
        const QString &text,
        const QString &sourcePath,
        const QString &itemId
    ) 
{
    QVector<float> embedding = generateEmbedding(text);
    if (embedding.isEmpty()) {
        return false;
    }
    return m_embed_db.saveEmbedding(sourcePath, itemId, text, embedding);
};


//--------------------------------------------------------------------------------
bool Embedder::embedFile(const QString &inputPath) 
{
    qDebug() << "Embedder::embedFile()" << inputPath;
    QFile file(inputPath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (doc.isNull()) return false;
    
    QStringList keys = {"name", "description", "id", "type", "species", "flags",
                       "messages", "text", "category", "title", "str", "str_sp"};
    
    if (doc.isArray()) {
        QJsonArray arr = doc.array();
        std::atomic<int> success = 0;

        for (int i = 0; i < arr.size(); i++) {
            QJsonValue item = arr[i];
            QString itemId = "item_" + QString::number(i);
            
            if (item.isObject() && item.toObject().contains("id")) {
                itemId = item.toObject()["id"].toString();
            }
            
            QString text = extractTextFromJson(item, keys);
            if (text.isEmpty()) text = "empty";
            
            if (embedAndSave(text, inputPath, itemId)) {
                success++;
            }
        }
        
        qDebug() << "  Saved" << success << "items";
        return success > 0;
    } else {
        QString text = extractTextFromJson(doc.object(), keys);
        if (text.isEmpty()) text = "empty";
        return embedAndSave(text, inputPath, "");
    }

};


//--------------------------------------------------------------------------------
QString Embedder::extractTextFromJson(
        const QJsonValue &value,
        const QStringList &keys
    ) 
{
    //qDebug() << "Embedder::extractTextFromJson()";
    QStringList texts;
    extractTextRecursive(value, keys, texts);
    return texts.join(" ");
};


//--------------------------------------------------------------------------------
void Embedder::extractTextRecursive(
        const QJsonValue &value,
        const QStringList &keys,
        QStringList &texts
    )
{
    if (value.isObject()) {
        QJsonObject obj = value.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            QString key = it.key();
            QJsonValue val = it.value();
            
            if (keys.contains(key)) {
                if (val.isString()) {
                    texts.append(val.toString());
                } else if (val.isDouble()) {
                    texts.append(QString::number(val.toDouble()));
                } else if (val.isObject()) {
                    QJsonObject nested = val.toObject();
                    if (nested.contains("str")) {
                        texts.append(nested["str"].toString());
                    }
                } else if (val.isArray()) {
                    for (const auto &item : val.toArray()) {
                        if (item.isString()) {
                            texts.append(item.toString());
                        }
                    }
                }
            }
            
            if (val.isObject() || val.isArray()) {
                extractTextRecursive(val, keys, texts);
            }
        }
    } else if (value.isArray()) {
        for (const auto &item : value.toArray()) {
            extractTextRecursive(item, keys, texts);
        }
    }
};


//--------------------------------------------------------------------------------
QVector<float> Embedder::generateEmbedding(QString input) 
{
    QJsonObject request;
    request["input"] = input;

    QJsonDocument doc(request);
    if (doc.isEmpty()) {
        qWarning() << "Embedder::generateEmbedding() doc.isEmpty()";
    }

    QByteArray jsonData = doc.toJson();
    if (jsonData.isEmpty()) {
        qWarning() << "Embedder::generateEmbedding() jsonData.isEmpty()";
    }

    QVector<float> embedding;
    embedding = m_generator->generate(doc.toJson());

    return embedding;
};

