#ifndef GENERATORIMMEDIATE_H
#define GENERATORIMMEDIATE_H
#include <QByteArray>

#include "Generator.h"
#include <QDir>

#include "llama.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "common.h"
#pragma GCC diagnostic pop

class GeneratorImmediate : public virtual Generator 
{
public:
    GeneratorImmediate(const QJsonObject& config);
    ~GeneratorImmediate() override;

    bool isValid() override;

    QVector<float> generate(const QString& data) override;

    QString generateText(
            QString& systemMessage,
            QString& prompt,
            bool isStream) override;
    
    llama_context *m_embedCtx;
    llama_model *m_embedModel;
};

#endif // GENERATORIMMEDIATE_H
