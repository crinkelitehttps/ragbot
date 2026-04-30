#ifndef PARSERJSON_H
#define PARSERJSON_H

#include "Parser.h"

class ParserJSON : public Parser
{
public:
    ParserJSON() = default;
    auto objectToChunk(const rb::Json& obj) -> Chunk override;

private:
    auto stringifyObject(const rb::Json& obj) -> rb::String;
    auto extractTextRecursive(
        const rb::Json& value,
        rb::Vector<rb::String>& texts,
        const rb::String& prefix = {}
    ) -> void;
};

#endif // PARSERJSON_H
