#include "GeneratorIP.h"


GeneratorIP::GeneratorIP(ConfigGenerator generatorConfig)
    : Generator(generatorConfig)
{
    QNetworkRequest netRequest;
#if 0
    netRequest.setUrl(QUrl(m_config.generatorConfig.modelName + "v1/embeddings"));
    netRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    netRequest.setTransferTimeout(m_config.generatorConfig.timeout);
    
    // move netowrk to genreator;
    QNetworkReply *reply = m_network->post(netRequest, jsonData);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(m_config.generatorConfig.timeout);
    
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
};

void GeneratorIP::hello()
{
    qDebug() << "GeneratorIP::hello()";
};
