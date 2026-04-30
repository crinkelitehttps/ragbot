#ifndef RAGBOTSESSION_H
#define RAGBOTSESSION_H

#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>
#include "compat/Json.h"
#include "compat/Types.h"
#include "ConversationTurn.h"
#include "generation/TextGenerator.h"

class Embedder;
class Reranker;
class Researcher;
class Roleplayer;
class RoleplayDatabase;

class RAGBotSession
{
public:
    static constexpr int MaxHistoryTurns = 2;

    explicit RAGBotSession(const rb::Json& config, bool loadOnly = false);
    ~RAGBotSession();

    RAGBotSession(const RAGBotSession&)            = delete;
    RAGBotSession& operator=(const RAGBotSession&) = delete;

    [[nodiscard]] auto isValid() const -> bool { return m_valid; }

    auto ask(const rb::String& question,
             const TextGenerator::TokenSink& tokenSink = {}) -> rb::String;

private:
    struct WorkItem {
        rb::String question;
        TextGenerator::TokenSink tokenSink;
    };

    auto workerRun(const rb::Json& config, bool loadOnly) -> void;
    auto workerLoop() -> void;
    auto processQuestion(const rb::String& question,
                         const TextGenerator::TokenSink& tokenSink) -> rb::String;

    std::thread             m_worker;
    std::mutex              m_mutex;
    std::condition_variable m_workReady;
    std::condition_variable m_workDone;
    std::condition_variable m_initCv;
    bool                    m_initDone    { false };

    std::optional<WorkItem> m_pending;
    rb::String              m_lastResult;
    bool                    m_resultReady { false };
    bool                    m_stop        { false };
    bool                    m_valid       { false };

    Embedder*         m_embedder        { nullptr };
    Reranker*         m_reranker        { nullptr };
    Researcher*       m_researcher      { nullptr };
    Roleplayer*       m_roleplayer      { nullptr };
    RoleplayDatabase* m_roleplayDb      { nullptr };
    bool              m_enableRoleplay  { false };

    rb::Vector<ConversationTurn> m_history;
};

#endif // RAGBOTSESSION_H
