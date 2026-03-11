#ifndef GENERATORIP_H
#define GENERATORIP_H

#include <QByteArray>
#include <QNetworkAccessManager>

#include "../config/ConfigGenerator.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "common.h"
#pragma GCC diagnostic pop

#include "Generator.h"
#include "llama.h"


//--------------------------------------------------------------------------------
class GeneratorIP : public virtual Generator
{
public:
    GeneratorIP(const QJsonObject& config);

    QVector<float> generate(const QString& data) override;

    QString generateText(
            QString& systemPrompt,
            QString& prompt,
            bool isStream
    ) override;

    bool isValid() override { return true; };

private:

    QNetworkAccessManager m_network;
    QString m_modelPath;
    const unsigned int m_timeout;
};

#endif // GENERATORIP_H
