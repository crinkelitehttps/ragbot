#ifndef GENERATOR_H
#define GENERATOR_H

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

class Generator 
{
public:
    struct SystemPrompt { QString value; };
    struct Prompt{ QString value; };

    virtual ~Generator() = default;

    virtual auto generate(const QByteArray& data) -> QVector<float> = 0;

    virtual auto generateText(
            SystemPrompt& systemPrompt,
            Prompt& prompt,
            bool isStream
    ) -> QString = 0;

    [[nodiscard]] virtual auto isValid() const -> bool = 0;

protected:
    Generator(const QJsonObject& config) { Q_UNUSED(config) }
    static constexpr float DefaultTemp { 0.7 };
    static constexpr float DefaultMaxTokens { 2000 };
};

#endif // GENERATOR_H
