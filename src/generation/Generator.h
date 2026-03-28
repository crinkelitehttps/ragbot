#ifndef GENERATOR_H
#define GENERATOR_H

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

class Generator 
{
public:
    virtual ~Generator() = default;

    virtual auto generate(const QString& data) -> QVector<float> = 0;

    virtual auto generateText(
            QString& systemPrompt,
            bool isStream,
            QString& prompt
    ) -> QString = 0;

    [[nodiscard]] virtual auto isValid() const -> bool = 0;

protected:
    Generator(const QJsonObject& config) { Q_UNUSED(config) }
    static constexpr float DefaultTemp { 0.7 };
    static constexpr float DefaultMaxTokens { 2000 };
};

#endif // GENERATOR_H
