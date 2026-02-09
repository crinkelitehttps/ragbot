#include <QTimer>
#include <QEventLoop>
#include <QNetworkReply>
#include "GeneratorIP.h"

GeneratorIP::GeneratorIP(ConfigGenerator generatorConfig)
       : Generator(generatorConfig)
       , m_config(generatorConfig) 
{
};

QByteArray GeneratorIP::generate(QByteArray question) 
{
    QNetworkRequest netRequest;
    netRequest.setUrl(QUrl(m_config.modelName + "v1/embeddings"));
    netRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    netRequest.setTransferTimeout(m_config.timeout);
    
    // move netowrk to genreator;
#if 0
    QNetworkReply *reply = m_network.post(netRequest, jsonData);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(m_config.timeout);
    
    loop.exec();
    
    
    if (timer.isActive()) {
        timer.stop();
        
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            parse(responseData, embedding);
        } else {
            qWarning() << "Embedder::generateEmbedding(): Network error:" 
                << reply->errorString();
        }
    } else {
        reply->abort();
        qWarning() << "Embedder::generateEmbedding(): Request timed out";
    }
    
    reply->deleteLater();
#endif
    return QByteArray();
};

