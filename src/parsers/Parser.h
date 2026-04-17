#ifndef PARSER_H
#define PARSER_H

#include <QString>
#include <QJsonValue>

class Parser
{
public:
    struct Chunk {
        QString embedText; // flattened natural-language text — used to generate the embedding vector
        QString content;   // verbatim content stored in the DB and retrieved for the LLM context
    };

    virtual ~Parser() = default;
    virtual auto toChunks(const QVariant& dataVariant) -> QVector<Chunk> = 0;

protected:
    Parser() {}
};

#endif // PARSER_H
