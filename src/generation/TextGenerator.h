#ifndef TEXTGENERATOR_H
#define TEXTGENERATOR_H

#include <functional>
#include "../compat/Types.h"

class TextGenerator
{
public:
    using TokenSink = std::function<void(rb::StringView)>;

    virtual ~TextGenerator() = default;
    virtual auto generateText(
        const rb::String& systemPrompt,
        bool isStream,
        const rb::String& prompt,
        const TokenSink& tokenSink = {}
    ) -> rb::String = 0;
    [[nodiscard]] virtual auto isValid() const -> bool = 0;

protected:
    static constexpr float DefaultTemp      { 0.7f };
    static constexpr int   DefaultMaxTokens { 2000 };
};

#endif // TEXTGENERATOR_H
