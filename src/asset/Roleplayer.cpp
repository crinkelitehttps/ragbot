#include "Roleplayer.h"
#include "../generation/GeneratorFactory.h"
#include "../ConfigKeys.h"
#include "../compat/Logging.h"
#include "../compat/Strings.h"
#include "../compat/Io.h"
#include <cstdio>


Roleplayer::Roleplayer(const rb::Json& config)
    : m_generator(GeneratorFactory::createText(config.value(ConfigKeys::Generator)))
    , m_characterName(config.stringValue(ConfigKeys::CharacterName, "Survivor"))
    , m_characterBackground(config.stringValue(ConfigKeys::CharacterBackground))
    , m_assetsDir(config.stringValue(ConfigKeys::AssetsDir, "inputs"))
{
    if (!m_generator || !m_generator->isValid()) {
        RAGBOT_LOG_WARN("Roleplayer: generator failed to initialise — disabled");
        m_generator.reset();
    }
}


auto Roleplayer::respond(
        const rb::String& researchAnswer,
        const rb::String& question,
        const rb::Vector<ConversationTurn>& history,
        const TextGenerator::TokenSink& tokenSink
) -> rb::String
{
    if (!m_generator || !m_generator->isValid()) {
        RAGBOT_LOG_WARN("Roleplayer::respond(): generator not available");
        return {};
    }

    bool promptOk = false;
    rb::String promptTemplate = rb::read_file_text(
        m_assetsDir + rb::from_std("/roleplayPrompt.txt"), &promptOk);
    if (!promptOk) {
        promptTemplate =
            rb::from_std("You are %1.\n\nHere is some factual information to help you answer:\n%2\n\n"
                         "Someone asks you: \"%3\"\n");
    }

    rb::String prompt;
    if (!history.empty()) {
        prompt += rb::from_std("Prior conversation:\n");
        for (const auto& turn : history)
            prompt += rb::from_std("User: ") + turn.question + rb::from_std("\n")
                    + m_characterName + rb::from_std(": ") + turn.roleplayAnswer
                    + rb::from_std("\n\n");
    }
    rb::Vector<rb::String> placeholders { m_characterName, researchAnswer, question };
    prompt += rb::replace_placeholders(promptTemplate, placeholders);

    RAGBOT_LOG_INFO("Roleplayer::respond(): character background:\n{}",
                    rb::to_std(m_characterBackground));
    RAGBOT_LOG_INFO("Roleplayer::respond(): user prompt:\n{}",
                    rb::to_std(prompt));

    if (!tokenSink) {
        std::fputc('\n', stdout);
        std::fputs(m_characterName.c_str(), stdout);
        std::fputs(": ", stdout);
        std::fflush(stdout);
    }
    const rb::String answer = m_generator->generateText(m_characterBackground, true, prompt, tokenSink);
    if (!tokenSink) {
        std::fputc('\n', stdout);
        std::fflush(stdout);
    }

    return answer;
}
