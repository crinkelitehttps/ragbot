#ifndef ROLEPLAYER_H
#define ROLEPLAYER_H

#include <memory>
#include "../compat/Json.h"
#include "../compat/Types.h"
#include "../ConversationTurn.h"
#include "../generation/TextGenerator.h"

class Roleplayer
{
public:
    explicit Roleplayer(const rb::Json& config);

    auto respond(
        const rb::String& researchAnswer,
        const rb::String& question,
        const rb::Vector<ConversationTurn>& history,
        const TextGenerator::TokenSink& tokenSink = {}
    ) -> rb::String;

    [[nodiscard]] auto characterName() const -> const rb::String& { return m_characterName; }

private:
    std::unique_ptr<TextGenerator> m_generator;
    rb::String                     m_characterName;
    rb::String                     m_characterBackground;
    rb::String                     m_assetsDir;
};

#endif // ROLEPLAYER_H
