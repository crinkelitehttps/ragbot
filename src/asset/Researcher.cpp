#include "Researcher.h"
#include "../generation/GeneratorFactory.h"
#include "../ConfigKeys.h"
#include "../compat/Logging.h"
#include "../compat/Strings.h"
#include "../compat/Io.h"
#include <cstdio>


Researcher::Researcher(const rb::Json& config)
    : m_generator(GeneratorFactory::createText(config.value(ConfigKeys::Generator)))
    , m_instruction(config.stringValue(ConfigKeys::Instruction,
          "You are a helpful assistant. Answer the question using only the provided context."))
{
    if (!m_generator || !m_generator->isValid()) {
        RAGBOT_LOG_WARN("Researcher: generator failed to initialise — disabled");
        m_generator.reset();
    }
}


auto Researcher::research(
        const rb::String& question,
        const rb::Vector<EmbeddingDatabase::SearchResult>& results,
        const rb::Vector<ConversationTurn>& history,
        const TextGenerator::TokenSink& tokenSink
) -> rb::String
{
    if (!m_generator || !m_generator->isValid()) {
        RAGBOT_LOG_WARN("Researcher::research(): generator not available");
        return {};
    }

    RAGBOT_LOG_INFO("Researcher::research(): context chunks ({})",
                    static_cast<int>(results.size()));

    rb::String context;
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        const rb::String fileName = rb::path_filename(r.sourceFile);
        const bool reranked = r.rerankScore >= 0.0f;
        const rb::String scoreLabel = reranked ? rb::from_std("relevance") : rb::from_std("similarity");
        const float scoreValue = reranked ? r.rerankScore : r.similarity;
        context += rb::format("[{}, {}: {}]\n{}\n\n",
                              rb::to_std(fileName), rb::to_std(scoreLabel),
                              scoreValue, rb::to_std(r.content));
    }
    RAGBOT_LOG_INFO("Researcher::research(): total context {} chars",
                    static_cast<int>(context.size()));

    rb::String prompt;
    if (!history.empty()) {
        prompt += rb::from_std("Prior conversation:\n");
        for (const auto& turn : history)
            prompt += rb::from_std("Q: ") + turn.question + rb::from_std("\nA: ")
                    + turn.researchAnswer + rb::from_std("\n\n");
    }
    prompt += rb::from_std("Context:\n") + context
            + rb::from_std("\nQuestion: ") + question;

    if (!tokenSink) {
        std::fputs("\nResearcher: ", stdout);
        std::fflush(stdout);
    }
    const rb::String answer = m_generator->generateText(m_instruction, true, prompt, tokenSink);
    if (!tokenSink) {
        std::fputc('\n', stdout);
        std::fflush(stdout);
    }

    return answer;
}
