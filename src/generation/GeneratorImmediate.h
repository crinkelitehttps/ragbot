#ifndef GENERATORIMMEDIATE_H
#define GENERATORIMMEDIATE_H
#include <QByteArray>

#include "Generator.h"
#include <QDir>

#include "llama.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "common.h"
#pragma GCC diagnostic pop

class GeneratorImmediate : public virtual Generator 
{
public:
    GeneratorImmediate(const QJsonObject& config);
    ~GeneratorImmediate() override;

    [[nodiscard]] auto isValid() const -> bool override { return m_isValid; };

    [[nodiscard]] auto generate(const QString& data) -> QVector<float> override;

    auto generateText(
            QString& systemPrompt,
            bool isStream,
            QString& prompt
        ) -> QString override;

private:
    static constexpr int DefaultMaxTokenGen { 512 };
    static constexpr int DefaultBufferLength { 128 };
    static constexpr int DefaultBatch { 2048 };
    static constexpr int DefaultCtx { 2048 };
    
    llama_context *m_embedCtx;
    llama_model *m_embedModel;
    bool m_isValid;
};

#endif // GENERATORIMMEDIATE_H
