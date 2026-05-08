#include <iostream>
#include <string>
#include "RAGBot.h"
#include "RAGBotSession.h"
#include "compat/Logging.h"
#include "compat/Strings.h"

RAGBot::RAGBot(RAGBotSession& session)
    : m_session(session)
{
}

void RAGBot::start()
{
    std::string line;
    while (true) {
        std::cout << "\nYou: " << std::flush;
        if (!std::getline(std::cin, line)) break;
        const rb::String question = rb::trim(line);
        if (question.empty()) continue;
        if (rb::to_lower(question) == "quit" || rb::to_lower(question) == "exit") {
            RAGBOT_LOG_INFO("Goodbye!");
            break;
        }
        m_session.ask(question);
    }
}
