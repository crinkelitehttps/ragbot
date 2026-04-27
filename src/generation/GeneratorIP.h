#ifndef GENERATORIP_H
#define GENERATORIP_H

#include <QNetworkAccessManager>
#include "EmbeddingGenerator.h"
#include "TextGenerator.h"

// Generator that calls an OpenAI-compatible HTTP server.
// Implements both EmbeddingGenerator (/v1/embeddings) and TextGenerator (/v1/chat/completions).
class GeneratorIP : public EmbeddingGenerator, public TextGenerator
{
public:
    explicit GeneratorIP(const QJsonObject& config);

    [[nodiscard]] auto generate(const QString& data) -> QVector<float> override;

    auto generateText(
        const QString& systemPrompt,
        bool isStream,
        const QString& prompt,
        const TokenSink& tokenSink = {}
    ) -> QString override;

    [[nodiscard]] auto isValid() const -> bool override { return m_isValid; }

    [[nodiscard]] static auto parseStaticResponse(const QByteArray& data) -> QString;
    [[nodiscard]] static auto parseStreamChunk(const QByteArray& data) -> QString;

private:
    [[nodiscard]] static auto parseEmbeddingResponse(const QByteArray& data) -> QVector<float>;
    auto runLoop(QNetworkReply* reply) -> bool;

    static constexpr int DefaultTimeout { 240000 };
    static inline const QString SseDataPrefix = "data: ";

    QNetworkAccessManager m_network;
    QString m_basePath;
    QString m_modelName;
    int     m_timeout;
    bool    m_isValid;
};

#endif // GENERATORIP_H
