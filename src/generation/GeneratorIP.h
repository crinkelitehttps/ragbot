#ifndef GENERATORIP_H
#define GENERATORIP_H

#include <QByteArray>
#include <QNetworkAccessManager>
#include "../config/ConfigGenerator.h"
#include "Generator.h"
#include "llama.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "common.h"
#pragma GCC diagnostic pop


//--------------------------------------------------------------------------------
class GeneratorIP : public virtual Generator
{
public:
    GeneratorIP(ConfigGenerator generatorConfig);
    void hello() override { qDebug() << "hello from GeneratorIP"; }
    bool isValid() override { return m_config.isValid(); };
    QByteArray generate(QByteArray question) override;

    ConfigGenerator m_config;
    QNetworkAccessManager m_network;
};

#endif // GENERATORIP_H
