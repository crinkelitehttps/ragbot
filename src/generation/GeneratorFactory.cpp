#include "GeneratorFactory.h"
#include "GeneratorIP.h"
#ifdef RAGBOT_EMBEDDED_INFERENCE
#include "EmbeddedEmbeddingGenerator.h"
#include "EmbeddedTextGenerator.h"
#endif
#include <QDebug>


//--------------------------------------------------------------------------------
auto GeneratorFactory::createEmbedding(const QJsonObject& config)
    -> std::unique_ptr<EmbeddingGenerator>
{
    const QString backend = config.value("backend").toString();

    if (backend == "embedded") {
#ifdef RAGBOT_EMBEDDED_INFERENCE
        return std::make_unique<EmbeddedEmbeddingGenerator>(config);
#else
        qCritical() << "GeneratorFactory: backend=embedded requested but binary was built "
                       "without RAGBOT_EMBEDDED_INFERENCE";
        return nullptr;
#endif
    }

    if (backend == "network" || backend.isEmpty()) {
        return std::make_unique<GeneratorIP>(config);
    }

    qCritical() << "GeneratorFactory: unknown backend:" << backend;
    return nullptr;
}


//--------------------------------------------------------------------------------
auto GeneratorFactory::createText(const QJsonObject& config)
    -> std::unique_ptr<TextGenerator>
{
    const QString backend = config.value("backend").toString();

    if (backend == "embedded") {
#ifdef RAGBOT_EMBEDDED_INFERENCE
        return std::make_unique<EmbeddedTextGenerator>(config);
#else
        qCritical() << "GeneratorFactory: backend=embedded requested but binary was built "
                       "without RAGBOT_EMBEDDED_INFERENCE";
        return nullptr;
#endif
    }

    if (backend == "network" || backend.isEmpty()) {
        return std::make_unique<GeneratorIP>(config);
    }

    qCritical() << "GeneratorFactory: unknown backend:" << backend;
    return nullptr;
}
