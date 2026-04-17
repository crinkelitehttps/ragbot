#ifndef EMBEDDEDTEXTGENERATOR_H
#define EMBEDDEDTEXTGENERATOR_H

#include "TextGenerator.h"
#include <QJsonObject>

#include "llama.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "common.h"
#pragma GCC diagnostic pop

class EmbeddedTextGenerator : public TextGenerator
{
public:
    explicit EmbeddedTextGenerator(const QJsonObject& config);
    ~EmbeddedTextGenerator() override;

    auto generateText(
        const QString& systemPrompt,
        bool isStream,
        const QString& prompt
    ) -> QString override;

    [[nodiscard]] auto isValid() const -> bool override { return m_isValid; }

private:
    static constexpr int   DefaultMaxTokenGen  { 512 };
    static constexpr int   DefaultBufferLength { 128 };
    static constexpr int   DefaultBatch        { 512 };
    static constexpr int   DefaultCtx          { 8192 };
    static constexpr float DefaultTemperature  { 0.7f };
    static constexpr float DefaultTopP         { 0.9f };
    static constexpr float DefaultRepeatPenalty{ 1.1f };

    llama_context* m_ctx   {};
    llama_model*   m_model {};
    bool           m_isValid { false };
    float          m_temperature  { DefaultTemperature };
    float          m_topP         { DefaultTopP };
    float          m_repeatPenalty{ DefaultRepeatPenalty };
};

#endif // EMBEDDEDTEXTGENERATOR_H
