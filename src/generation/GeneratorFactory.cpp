#include "GeneratorFactory.h"
#include "GeneratorIP.h"
#include "RerankGeneratorIP.h"
#ifdef RAGBOT_EMBEDDED_INFERENCE
#include "EmbeddedEmbeddingGenerator.h"
#include "EmbeddedTextGenerator.h"
#include "EmbeddedRerankGenerator.h"
#endif
#include "../ConfigKeys.h"
#include "../compat/Logging.h"


auto GeneratorFactory::createEmbedding(const rb::Json& config)
    -> std::unique_ptr<EmbeddingGenerator>
{
    const rb::String backend = config.stringValue(ConfigKeys::Backend);

    if (backend == ConfigKeys::BackendEmbedded) {
#ifdef RAGBOT_EMBEDDED_INFERENCE
        return std::make_unique<EmbeddedEmbeddingGenerator>(config);
#else
        RAGBOT_LOG_ERROR("GeneratorFactory: backend=embedded requested but binary was built "
                         "without RAGBOT_EMBEDDED_INFERENCE");
        return nullptr;
#endif
    }

    if (backend == ConfigKeys::BackendNetwork || rb::str_empty(backend))
        return std::make_unique<GeneratorIP>(config);

    RAGBOT_LOG_ERROR("GeneratorFactory: unknown backend: {}", rb::to_std(backend));
    return nullptr;
}


auto GeneratorFactory::createText(const rb::Json& config)
    -> std::unique_ptr<TextGenerator>
{
    const rb::String backend = config.stringValue(ConfigKeys::Backend);

    if (backend == ConfigKeys::BackendEmbedded) {
#ifdef RAGBOT_EMBEDDED_INFERENCE
        return std::make_unique<EmbeddedTextGenerator>(config);
#else
        RAGBOT_LOG_ERROR("GeneratorFactory: backend=embedded requested but binary was built "
                         "without RAGBOT_EMBEDDED_INFERENCE");
        return nullptr;
#endif
    }

    if (backend == ConfigKeys::BackendNetwork || rb::str_empty(backend))
        return std::make_unique<GeneratorIP>(config);

    RAGBOT_LOG_ERROR("GeneratorFactory: unknown backend: {}", rb::to_std(backend));
    return nullptr;
}


auto GeneratorFactory::createRerank(const rb::Json& config)
    -> std::unique_ptr<RerankGenerator>
{
    const rb::String backend = config.stringValue(ConfigKeys::Backend);

    if (backend == ConfigKeys::BackendEmbedded) {
#ifdef RAGBOT_EMBEDDED_INFERENCE
        return std::make_unique<EmbeddedRerankGenerator>(config);
#else
        RAGBOT_LOG_ERROR("GeneratorFactory: backend=embedded requested but binary was built "
                         "without RAGBOT_EMBEDDED_INFERENCE");
        return nullptr;
#endif
    }

    if (backend == ConfigKeys::BackendNetwork || rb::str_empty(backend))
        return std::make_unique<RerankGeneratorIP>(config);

    RAGBOT_LOG_ERROR("GeneratorFactory: unknown backend: {}", rb::to_std(backend));
    return nullptr;
}
