#ifndef EMBEDDEDEMBEDDINGGENERATOR_H
#define EMBEDDEDEMBEDDINGGENERATOR_H

#include "EmbeddingGenerator.h"
#include "../compat/Json.h"

#include "llama.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "common.h"
#pragma GCC diagnostic pop

class EmbeddedEmbeddingGenerator : public EmbeddingGenerator
{
public:
    explicit EmbeddedEmbeddingGenerator(const rb::Json& config);
    ~EmbeddedEmbeddingGenerator() override;

    [[nodiscard]] auto generate(const rb::String& data) -> rb::Vector<float> override;
    [[nodiscard]] auto isValid() const -> bool override { return m_isValid; }

private:
    static constexpr int DefaultBatch { 512 };
    static constexpr int DefaultCtx   { 2048 };

    llama_context* m_ctx   {};
    llama_model*   m_model {};
    bool           m_isValid { false };
};

#endif // EMBEDDEDEMBEDDINGGENERATOR_H
