#ifndef EMBEDDEDRERANKGENERATOR_H
#define EMBEDDEDRERANKGENERATOR_H

#include "RerankGenerator.h"
#include "../compat/Json.h"

#include "llama.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "common.h"
#pragma GCC diagnostic pop

class EmbeddedRerankGenerator : public RerankGenerator
{
public:
    explicit EmbeddedRerankGenerator(const rb::Json& config);
    ~EmbeddedRerankGenerator() override;

    auto score(const rb::String& query,
               const rb::Vector<rb::String>& documents) -> rb::Vector<float> override;
    [[nodiscard]] auto isValid() const -> bool override { return m_isValid; }

private:
    auto scoreOne(const rb::String& query, const rb::String& document) -> float;

    static constexpr int DefaultBatch { 512 };
    static constexpr int DefaultCtx   { 2048 };

    llama_context* m_ctx   {};
    llama_model*   m_model {};
    bool           m_isValid { false };
};

#endif // EMBEDDEDRERANKGENERATOR_H
