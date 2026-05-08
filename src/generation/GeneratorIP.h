#ifndef GENERATORIP_H
#define GENERATORIP_H

#include "../compat/Http.h"
#include "../compat/Json.h"
#include "EmbeddingGenerator.h"
#include "TextGenerator.h"

class GeneratorIP : public EmbeddingGenerator, public TextGenerator
{
public:
    explicit GeneratorIP(const rb::Json& config);

    [[nodiscard]] auto generate(const rb::String& data) -> rb::Vector<float> override;
    [[nodiscard]] auto generateBatch(
        const rb::Vector<rb::String>& inputs,
        const ProgressCallback& onProgress = {}
    ) -> rb::Vector<rb::Vector<float>> override;

    auto generateText(
        const rb::String& systemPrompt,
        bool isStream,
        const rb::String& prompt,
        const TokenSink& tokenSink = {}
    ) -> rb::String override;

    [[nodiscard]] auto isValid() const -> bool override { return m_isValid; }

    [[nodiscard]] static auto parseStaticResponse(const rb::Bytes& data) -> rb::String;
    [[nodiscard]] static auto parseStreamChunk(std::string_view data) -> rb::String;

private:
    [[nodiscard]] static auto parseEmbeddingResponse(const rb::Bytes& data) -> rb::Vector<float>;

    static constexpr int DefaultTimeout { 240000 };

    rb::HttpClient m_http;
    rb::String     m_basePath;
    rb::String     m_modelName;
    int            m_timeout;
    bool           m_isValid;
};

#endif // GENERATORIP_H
