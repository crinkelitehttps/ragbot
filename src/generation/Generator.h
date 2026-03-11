#ifndef GENERATOR_H
#define GENERATOR_H

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

class Generator 
{
public:
    virtual ~Generator() = default;

    virtual QVector<float> generate(const QString& data) = 0;

    virtual QString generateText(
            QString& systemPrompt,
            QString& prompt,
            bool isStream
    ) = 0;

    virtual bool isValid() = 0;

protected:
    Generator(const QJsonObject&) {}
};

#endif // GENERATOR_H
