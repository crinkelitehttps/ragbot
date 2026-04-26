#ifndef GENERATORFACTORY_H
#define GENERATORFACTORY_H

#include <memory>
#include <QJsonObject>
#include "EmbeddingGenerator.h"
#include "TextGenerator.h"
#include "RerankGenerator.h"

// Creates generator instances from a config object.
// Config must contain "backend": "embedded" or "backend": "network".
// "embedded" requires RAGBOT_EMBEDDED_INFERENCE to be defined at build time.
class GeneratorFactory
{
public:
    static auto createEmbedding(const QJsonObject& config)
        -> std::unique_ptr<EmbeddingGenerator>;

    static auto createText(const QJsonObject& config)
        -> std::unique_ptr<TextGenerator>;

    static auto createRerank(const QJsonObject& config)
        -> std::unique_ptr<RerankGenerator>;

private:
    GeneratorFactory() = delete;
};

#endif // GENERATORFACTORY_H
