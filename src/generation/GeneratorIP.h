#ifndef GENERATORIP_H
#define GENERATORIP_H

#include <QByteArray>
#include <QNetworkAccessManager>


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "common.h"
#pragma GCC diagnostic pop

#include "Generator.h"
#include "llama.h"


//--------------------------------------------------------------------------------
class GeneratorIP : public virtual Generator
{

public:

    GeneratorIP(const QJsonObject& config);

    auto generate(const QString& data) -> QVector<float> override;

    auto generateText(
            QString& systemPrompt,
            bool isStream,
            QString& prompt
    ) -> QString override;

    [[nodiscard]] auto isValid() const -> bool override { return m_isValid; };
    [[nodiscard]] static auto parseStaticResponse(const QByteArray& data) -> QString; 
    [[nodiscard]] static auto parseStreamChunk(const QByteArray& data) -> QString;

private:
    [[nodiscard]] static auto parseResponse(
        const QByteArray& responseData
    ) -> QVector<float>;
    
    static constexpr int DefaultTimeout { 1000 };
    static inline const QString SseDataPrefix = "data: ";

    QNetworkAccessManager m_network;
    QString m_modelPath;
    const int m_timeout;
    bool m_isValid;
};

#endif // GENERATORIP_H
