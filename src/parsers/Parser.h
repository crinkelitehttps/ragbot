#ifndef PARSER_H
#define PARSER_H

#include "../compat/Json.h"
#include "../compat/Types.h"

class Parser
{
public:
    struct Chunk {
        rb::String embedText; // flattened natural-language text for embedding generation
        rb::String content;   // verbatim content stored in the DB and shown to the LLM
    };

    virtual ~Parser() = default;
    virtual auto objectToChunk(const rb::Json& obj) -> Chunk = 0;

protected:
    Parser() = default;
};

#endif // PARSER_H
