#ifndef PARSERJSON_H
#define PARSERJSON_H

#include "Parser.h"
#include <QJsonValue>

// Converts a single (pre-resolved) JSON object into a Parser::Chunk.
// embedText is a flattened natural-language representation for embedding.
// content is the compact JSON string for storage and LLM context.
class ParserJSON : public Parser
{
public:
    ParserJSON() = default;
    auto objectToChunk(const QJsonObject& obj) -> Chunk override;

private:
    auto stringifyObject(const QJsonObject& obj) -> QString;
    auto extractTextRecursive(
        const QJsonValue& value,
        QStringList& texts,
        const QString& prefix = QString()
    ) -> void;
};

#endif // PARSERJSON_H
