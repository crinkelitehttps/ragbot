#ifndef GENERATOR_H
#define GENERATOR_H

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "../config/ConfigGenerator.h"

class Generator 
{
public:
    virtual ~Generator() = default;

    virtual QVector<float> generate(const QString data) = 0;

    virtual QString generateText(
            QString& systemPrompt,
            QString& prompt,
            bool isStream
    ) = 0;

    virtual bool isValid() = 0;

protected:
    Generator(ConfigGenerator) {}
};

#endif // GENERATOR_H
