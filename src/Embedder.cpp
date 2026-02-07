// Modified to use local llama-swap embedding server
#include "Embedder.h"
#include "db/EmbeddingDatabase.h"
#include "config/ConfigEmbed.h"


//--------------------------------------------------------------------------------
Embedder::Embedder(const ConfigEmbed &config)
    : m_network(new QNetworkAccessManager())
    , m_db("embeddings.db")
{
    qDebug() << "Embedder::Embedder()";
    QString jsonDir = QDir::homePath() + "/source/Cataclysm-DDA/data/json";
    if (!QDir(jsonDir).exists()) {
        qCritical() << "Embedder::Embedder(): JSON directory not found:" << jsonDir;
    }
    EmbeddingDatabase db("embedder.db");
    processAllFiles();
}


//--------------------------------------------------------------------------------
void Embedder::processAllFiles()
{
    qDebug() << "EmbeddingDatabase::processAllFiles()";
    {
        int total = 0, processed = 0;

        m_jsonDir = "/home/joe/source/Cataclysm-DDA/data/json";
        
        QDirIterator countIt(
                m_jsonDir,
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
                m_jsonDir,
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

        QCoreApplication::quit();
    }
};


//--------------------------------------------------------------------------------
bool Embedder::embedAndSave(
        const QString &text,
        const QString &sourcePath,
        const QString &itemId
    ) 
{
    qDebug() << "Embedder::embedAndSave()";
    // Use remote embedder
    QVector<float> embedding = generateEmbedding(text);
    if (embedding.isEmpty()) {
        return false;
    }
    return m_db.saveEmbedding(sourcePath, itemId, text, embedding);
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
    qDebug() << "Embedder::extractTextFromJson()";
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
QVector<float> Embedder::generateEmbedding(const QString &text) 
{

    const auto config = m_config.llmConfig;
    qDebug() << "Embedder::generateEmbedding()" << text;
    QJsonObject request;
    request["model"] = config.model;
    request["input"] = text;
    
    QJsonDocument doc(request);
    if (doc.isEmpty()) {
        qWarning() << "Embedder::generateEmbedding() doc.isEmpty()";
    }

    QByteArray jsonData = doc.toJson();
    if (jsonData.isEmpty()) {
        qWarning() << "Embedder::generateEmbedding() jsonData.isEmpty()";
    }
    
    QNetworkRequest netRequest;
    qInfo() << "Embedder::generateEmbedding() [ passed network created ]";
    netRequest.setUrl(QUrl(config.baseUrl + "v1/embeddings"));
    netRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    netRequest.setTransferTimeout(config.timeout);

    QNetworkReply *reply = m_network->post(netRequest, jsonData);
    
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(config.timeout);
    
    loop.exec();
    
    QVector<float> embedding;
    
    if (timer.isActive()) {
        timer.stop();
        
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
            
            if (!responseDoc.isNull()) {
                QJsonObject obj = responseDoc.object();
                
                // Parse OpenAI-compatible embeddings response
                if (obj.contains("data")) {
                    QJsonArray dataArray = obj["data"].toArray();
                    if (!dataArray.isEmpty()) {
                        QJsonObject firstItem = dataArray[0].toObject();
                        if (firstItem.contains("embedding")) {
                            QJsonArray embArray = firstItem["embedding"].toArray();
                            for (const QJsonValue &val : embArray) {
                                embedding.append(val.toDouble());
                            }
                        }
                    }
                }
            }
        } else {
            qWarning() << "Embedder::generateEmbedding(): Network error:" 
                << reply->errorString();
        }
    } else {
        reply->abort();
        qWarning() << "Embedder::generateEmbedding(): Request timed out";
    }
    
    reply->deleteLater();
    return embedding;
};
