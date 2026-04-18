#ifndef PARSER_H
#define PARSER_H

#include <QString>
#include <QJsonObject>

class Parser
{
public:
    struct Chunk {
        QString embedText; // flattened natural-language text for embedding generation
        QString content;   // verbatim content stored in the DB and shown to the LLM
    };

    virtual ~Parser() = default;
    virtual auto objectToChunk(const QJsonObject& obj) -> Chunk = 0;

protected:
    Parser() = default;
};

#endif // PARSER_H
