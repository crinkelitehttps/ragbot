#ifndef GENERATORFACTORY_H
#define GENERATORFACTORY_H

#include <memory>
#include "../compat/Json.h"
#include "EmbeddingGenerator.h"
#include "TextGenerator.h"
#include "RerankGenerator.h"

class GeneratorFactory
{
public:
    static auto createEmbedding(const rb::Json& config)
        -> std::unique_ptr<EmbeddingGenerator>;

    static auto createText(const rb::Json& config)
        -> std::unique_ptr<TextGenerator>;

    static auto createRerank(const rb::Json& config)
        -> std::unique_ptr<RerankGenerator>;

private:
    GeneratorFactory() = delete;
};

#endif // GENERATORFACTORY_H
