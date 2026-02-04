// Modified to use local llama-swap embedding server
#include "Embedder.h"
#include "db/EmbeddingDatabase.h"

//--------------------------------------------------------------------------------
QVector<float> RemoteEmbedder::generateEmbedding(const QString &text) 
{

    QJsonObject request;
    request["model"] = m_config.model;
    request["input"] = text;
    
    QJsonDocument doc(request);
    QByteArray jsonData = doc.toJson();
    
    QNetworkRequest netRequest;
    netRequest.setUrl(QUrl(m_config.baseUrl + "/v1/embeddings"));
    netRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    netRequest.setTransferTimeout(m_config.timeout);
    
    QNetworkReply *reply = m_manager->post(netRequest, jsonData);
    
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(m_config.timeout);
    
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
            qWarning() << "Network error:" << reply->errorString();
        }
    } else {
        reply->abort();
        qWarning() << "Request timed out";
    }
    
    reply->deleteLater();
    return embedding;
};

bool EmbeddingDatabase::saveEmbedding(const QString &sourceFile, const QString &itemId, 
                      const QString &content, const QVector<float> &embedding) 
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO embeddings (source_file, item_id, content, embedding) "
                 "VALUES (?, ?, ?, ?)");
    query.addBindValue(sourceFile);
    query.addBindValue(itemId);
    query.addBindValue(content.left(5000)); // Truncate very long content
    
    QByteArray embBlob(reinterpret_cast<const char*>(embedding.data()), 
                      embedding.size() * sizeof(float));
    query.addBindValue(embBlob);
    
    if (!query.exec()) {
        qWarning() << "Failed to save embedding:" << query.lastError().text();
        return false;
    }
        return true;
    
};

int EmbeddingDatabase::count()
{
    QSqlQuery query("SELECT COUNT(*) FROM embeddings", m_db);
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
};

void RemoteEmbedder::processAllFiles()
{
    {
        int total = 0, processed = 0;
        
        QDirIterator countIt(m_jsonDir, QStringList() << "*.json", QDir::Files, QDirIterator::Subdirectories);
        while (countIt.hasNext()) {
            countIt.next();
            total++;
        }
        
        qDebug() << "Found" << total << "JSON files";
        
        QDirIterator it(m_jsonDir, QStringList() << "*.json", QDir::Files, QDirIterator::Subdirectories);
        
        while (it.hasNext()) {
            QString filePath = it.next();
            QFileInfo fileInfo(filePath);
            
            processed++;
            qDebug() << QString("[%1/%2] %3").arg(processed).arg(total).arg(fileInfo.fileName());
            
            if (!embedFile(filePath)) {
                qWarning() << "Failed:" << filePath;
            }
        }
        
        qDebug() << "Complete! Processed" << processed << "files";
        qDebug() << "Database now contains" << m_db->count() << "embeddings";
        QCoreApplication::quit();
    }
};

#if 1
#endif


bool RemoteEmbedder::embedFile(const QString &inputPath) 
{
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

QString RemoteEmbedder::extractTextFromJson(const QJsonValue &value, const QStringList &keys) 
{
    QStringList texts;
    extractTextRecursive(value, keys, texts);
    return texts.join(" ");
};

void RemoteEmbedder::extractTextRecursive(const QJsonValue &value, const QStringList &keys, QStringList &texts)
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

bool RemoteEmbedder::embedAndSave(const QString &text, const QString &sourcePath, const QString &itemId) 
{
   // Use remote embedder
   QVector<float> embedding = generateEmbedding(text);
   if (embedding.isEmpty()) {
       return false;
   }
   return m_db->saveEmbedding(sourcePath, itemId, text, embedding);
};

