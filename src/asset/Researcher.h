#ifndef RESEARCHER_H
#define RESEARCHER_H

#include <memory>
#include "../compat/Json.h"
#include "../compat/Types.h"
#include "../ConversationTurn.h"
#include "../db/EmbeddingDatabase.h"
#include "../generation/TextGenerator.h"

class Researcher
{
public:
    explicit Researcher(const rb::Json& config);

    auto research(
        const rb::String& question,
        const rb::Vector<EmbeddingDatabase::SearchResult>& results,
        const rb::Vector<ConversationTurn>& history,
        const TextGenerator::TokenSink& tokenSink = {}
    ) -> rb::String;

private:
    std::unique_ptr<TextGenerator> m_generator;
    rb::String                     m_instruction;
};

#endif // RESEARCHER_H
