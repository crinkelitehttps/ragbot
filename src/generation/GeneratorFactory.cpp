#include "GeneratorFactory.h"
#include "GeneratorIP.h"
#include "RerankGeneratorIP.h"
#ifdef RAGBOT_EMBEDDED_INFERENCE
#include "EmbeddedEmbeddingGenerator.h"
#include "EmbeddedTextGenerator.h"
#include "EmbeddedRerankGenerator.h"
#endif
#include "../ConfigKeys.h"
#include <QDebug>


//--------------------------------------------------------------------------------
auto GeneratorFactory::createEmbedding(const QJsonObject& config)
    -> std::unique_ptr<EmbeddingGenerator>
{
    const QString backend = config.value(ConfigKeys::Backend).toString();

    if (backend == ConfigKeys::BackendEmbedded) {
#ifdef RAGBOT_EMBEDDED_INFERENCE
        return std::make_unique<EmbeddedEmbeddingGenerator>(config);
#else
        qCritical() << "GeneratorFactory: backend=embedded requested but binary was built "
                       "without RAGBOT_EMBEDDED_INFERENCE";
        return nullptr;
#endif
    }

    if (backend == ConfigKeys::BackendNetwork || backend.isEmpty()) {
        return std::make_unique<GeneratorIP>(config);
    }

    qCritical() << "GeneratorFactory: unknown backend:" << backend;
    return nullptr;
}


//--------------------------------------------------------------------------------
auto GeneratorFactory::createText(const QJsonObject& config)
    -> std::unique_ptr<TextGenerator>
{
    const QString backend = config.value(ConfigKeys::Backend).toString();

    if (backend == ConfigKeys::BackendEmbedded) {
#ifdef RAGBOT_EMBEDDED_INFERENCE
        return std::make_unique<EmbeddedTextGenerator>(config);
#else
        qCritical() << "GeneratorFactory: backend=embedded requested but binary was built "
                       "without RAGBOT_EMBEDDED_INFERENCE";
        return nullptr;
#endif
    }

    if (backend == ConfigKeys::BackendNetwork || backend.isEmpty()) {
        return std::make_unique<GeneratorIP>(config);
    }

    qCritical() << "GeneratorFactory: unknown backend:" << backend;
    return nullptr;
}


//--------------------------------------------------------------------------------
auto GeneratorFactory::createRerank(const QJsonObject& config)
    -> std::unique_ptr<RerankGenerator>
{
    const QString backend = config.value(ConfigKeys::Backend).toString();

    if (backend == ConfigKeys::BackendEmbedded) {
#ifdef RAGBOT_EMBEDDED_INFERENCE
        return std::make_unique<EmbeddedRerankGenerator>(config);
#else
        qCritical() << "GeneratorFactory: backend=embedded requested but binary was built "
                       "without RAGBOT_EMBEDDED_INFERENCE";
        return nullptr;
#endif
    }

    if (backend == ConfigKeys::BackendNetwork || backend.isEmpty()) {
        return std::make_unique<RerankGeneratorIP>(config);
    }

    qCritical() << "GeneratorFactory: unknown backend:" << backend;
    return nullptr;
}
