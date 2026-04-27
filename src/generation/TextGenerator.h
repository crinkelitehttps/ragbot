#ifndef TEXTGENERATOR_H
#define TEXTGENERATOR_H

#include <functional>
#include <QString>
#include <QStringView>

class TextGenerator
{
public:
    using TokenSink = std::function<void(QStringView)>;

    virtual ~TextGenerator() = default;
    virtual auto generateText(
        const QString& systemPrompt,
        bool isStream,
        const QString& prompt,
        const TokenSink& tokenSink = {}
    ) -> QString = 0;
    [[nodiscard]] virtual auto isValid() const -> bool = 0;

protected:
    static constexpr float DefaultTemp      { 0.7f };
    static constexpr int   DefaultMaxTokens { 2000 };
};

#endif // TEXTGENERATOR_H
