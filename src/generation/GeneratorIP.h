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
    GeneratorIP(ConfigGenerator generatorConfig);

    const QVector<float> generate(const QString& data) override;

    const QString generateText(
            QString& systemPrompt,
            QString& prompt,
            bool isStream
    ) override;

    const bool isValid() override { return true; };

#if 0
    bool isValid() override { return m_config.isValid(); };
#endif
    
private:

    ConfigGenerator m_config;
    QNetworkAccessManager m_network;
};

#endif // GENERATORIP_H
