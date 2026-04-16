#ifndef GENERATORIP_H
#define GENERATORIP_H

#include <QByteArray>
#include <QNetworkAccessManager>
#include "Generator.h"

// Generator implementation that calls an OpenAI-compatible HTTP endpoint.
// Supports both /v1/embeddings (generate) and /v1/chat/completions (generateText).
class GeneratorIP : public virtual Generator
{
public:
    explicit GeneratorIP(const QJsonObject& config);

    // Returns an embedding vector via POST /v1/embeddings.
    auto generate(const QString& data) -> QVector<float> override;

    // Returns generated text via POST /v1/chat/completions.
    // Streams tokens to stdout if isStream is true.
    auto generateText(
        QString& systemPrompt,
        bool isStream,
        QString& prompt
    ) -> QString override;

    [[nodiscard]] auto isValid() const -> bool override { return m_isValid; }

    [[nodiscard]] static auto parseStaticResponse(const QByteArray& data) -> QString;
    [[nodiscard]] static auto parseStreamChunk(const QByteArray& data) -> QString;

private:
    [[nodiscard]] static auto parseEmbeddingResponse(const QByteArray& data) -> QVector<float>;

    // Default matches the typical llama-swap / llama.cpp server timeout.
    static constexpr int DefaultTimeout { 240000 };
    static inline const QString SseDataPrefix = "data: ";

    QNetworkAccessManager m_network;
    QString m_basePath;   // e.g. "http://127.0.0.1:8080/upstream/qwen2.5/"
    QString m_modelName;  // sent in the "model" field of every request
    int     m_timeout;
    bool    m_isValid;
};

#endif // GENERATORIP_H
