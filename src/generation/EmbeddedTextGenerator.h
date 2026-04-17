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
    static constexpr int DefaultMaxTokenGen  { 512 };
    static constexpr int DefaultBufferLength { 128 };
    static constexpr int DefaultBatch        { 512 };
    static constexpr int DefaultCtx          { 8192 };

    llama_context* m_ctx   {};
    llama_model*   m_model {};
    bool           m_isValid { false };
};

#endif // EMBEDDEDTEXTGENERATOR_H
