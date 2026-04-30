#include "RAGBotSession.h"
#include "ConfigKeys.h"
#include "compat/Logging.h"
#include "asset/Embedder.h"
#include "asset/Reranker.h"
#include "asset/Researcher.h"
#include "asset/Roleplayer.h"
#include "db/RoleplayDatabase.h"
#include <cstdio>


RAGBotSession::RAGBotSession(const rb::Json& config, bool loadOnly)
{
    m_worker = std::thread([this, config, loadOnly]() {
        workerRun(config, loadOnly);
    });

    // Wait until the worker signals init complete.
    std::unique_lock<std::mutex> lk(m_mutex);
    m_initCv.wait(lk, [this] { return m_initDone; });
}


RAGBotSession::~RAGBotSession()
{
    if (m_worker.joinable()) {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_stop = true;
        }
        m_workReady.notify_one();
        m_worker.join();
    }
}


auto RAGBotSession::ask(const rb::String& question,
                        const TextGenerator::TokenSink& tokenSink) -> rb::String
{
    std::unique_lock<std::mutex> lk(m_mutex);
    m_pending      = {question, tokenSink};
    m_resultReady  = false;
    m_workReady.notify_one();

    m_workDone.wait(lk, [this] { return m_resultReady; });
    return m_lastResult;
}


auto RAGBotSession::workerRun(const rb::Json& config, bool loadOnly) -> void
{
    m_embedder = new Embedder(config.value(ConfigKeys::Embedder));
    if (!m_embedder->isValid()) {
        RAGBOT_LOG_WARN("RAGBotSession: failed to construct embedder");
        delete m_embedder;
        m_embedder = nullptr;
        m_valid = false;
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_initDone = true;
        }
        m_initCv.notify_one();
        return;
    }
    RAGBOT_LOG_INFO("RAGBotSession: embedder ready");

    if (loadOnly) {
        RAGBOT_LOG_INFO("RAGBotSession: load-only mode complete");
        m_valid = true;
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_initDone = true;
        }
        m_initCv.notify_one();
        return;
    }

    m_reranker   = new Reranker(config.value(ConfigKeys::Reranker));
    m_researcher = new Researcher(config.value(ConfigKeys::Researcher));
    m_roleplayer = new Roleplayer(config.value(ConfigKeys::Roleplayer));
    m_roleplayDb = new RoleplayDatabase(
        config.stringValue(ConfigKeys::ConversationsDb, "conversations.db"));
    m_enableRoleplay =
        config.value(ConfigKeys::Roleplayer).boolValue(ConfigKeys::Enabled, false);

    RAGBOT_LOG_INFO("RAGBotSession: reranker ready (enabled: {})", m_reranker->isEnabled());
    RAGBOT_LOG_INFO("RAGBotSession: researcher, roleplayer, roleplay database ready");

    m_valid = true;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_initDone = true;
    }
    m_initCv.notify_one();

    workerLoop();

    delete m_roleplayDb; m_roleplayDb = nullptr;
    delete m_roleplayer; m_roleplayer = nullptr;
    delete m_researcher; m_researcher = nullptr;
    delete m_reranker;   m_reranker   = nullptr;
    delete m_embedder;   m_embedder   = nullptr;
}


auto RAGBotSession::workerLoop() -> void
{
    while (true) {
        std::unique_lock<std::mutex> lk(m_mutex);
        m_workReady.wait(lk, [this] { return m_pending.has_value() || m_stop; });

        if (m_stop) return;

        WorkItem item = std::move(*m_pending);
        m_pending.reset();
        lk.unlock();

        const rb::String result = processQuestion(item.question, item.tokenSink);

        lk.lock();
        m_lastResult  = result;
        m_resultReady = true;
        m_workDone.notify_one();
    }
}


auto RAGBotSession::processQuestion(const rb::String& question,
                                    const TextGenerator::TokenSink& tokenSink) -> rb::String
{
    RAGBOT_LOG_INFO("RAGBotSession::processQuestion(): {}", rb::to_std(question));

    auto results = m_embedder->search(question);
    if (results.empty()) {
        if (!tokenSink) {
            std::fputs("\nNo relevant documents found.\n", stdout);
            std::fflush(stdout);
        }
        return {};
    }
    RAGBOT_LOG_INFO("RAGBotSession::processQuestion(): {} chunks retrieved",
                    static_cast<int>(results.size()));

    if (m_reranker->isEnabled())
        results = m_reranker->rerank(question, results);

    const rb::String researchAnswer =
        m_researcher->research(question, results, m_history, tokenSink);
    if (rb::str_empty(researchAnswer)) {
        RAGBOT_LOG_WARN("RAGBotSession::processQuestion(): researcher returned empty answer");
        return {};
    }

    rb::String roleplayAnswer;
    if (m_enableRoleplay)
        roleplayAnswer = m_roleplayer->respond(researchAnswer, question, m_history, tokenSink);

    m_roleplayDb->logConversation(
        m_embedder->lastQueryEmbedding(), question, researchAnswer, roleplayAnswer);

    m_history.push_back({question, researchAnswer, roleplayAnswer});
    if (static_cast<int>(m_history.size()) > MaxHistoryTurns)
        m_history.erase(m_history.begin());

    return m_enableRoleplay ? roleplayAnswer : researchAnswer;
}
