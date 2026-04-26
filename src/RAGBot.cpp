#include <QDebug>
#include <QTextStream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "RAGBot.h"

//--------------------------------------------------------------------------------
RAGBot::RAGBot(Embedder& embedder, Reranker& reranker, Researcher& researcher,
               Roleplayer& roleplayer, RoleplayDatabase& roleplayDb, bool enableRoleplay)
    : m_embedder(embedder)
    , m_reranker(reranker)
    , m_researcher(researcher)
    , m_roleplayer(roleplayer)
    , m_roleplayDb(roleplayDb)
    , m_enableRoleplay(enableRoleplay)
{
    qDebug() << "RAGBot::RAGBot()";
}


//--------------------------------------------------------------------------------
void RAGBot::start()
{
    QTextStream tin(stdin);

    while (true) {
        QTextStream(stdout) << "\nYou: " << Qt::flush;
        const QString question = tin.readLine().trimmed();

        if (question.isEmpty()) continue;
        if (question.toLower() == "quit" || question.toLower() == "exit") {
            qDebug() << "Goodbye!";
            break;
        }
        processQuestion(question);
    }
}


//--------------------------------------------------------------------------------
auto RAGBot::processQuestion(const QString& question) -> void
{
    qDebug() << "RAGBot::processQuestion():" << question;

    // Stage 1: embed the question and retrieve the most similar indexed chunks.
    auto results = m_embedder.search(question);
    if (results.isEmpty()) {
        QTextStream(stdout) << "\nNo relevant documents found.\n" << Qt::flush;
        return;
    }
    qDebug() << "RAGBot::processQuestion():" << results.size() << "chunks retrieved";

    // Stage 1b: rerank retrieved chunks by cross-encoder relevance score.
    if (m_reranker.isEnabled())
        results = m_reranker.rerank(question, results);

    QString researchAnswer;
    QString roleplayAnswer;
    bool researcherFailed = false;
    bool roleplayerFailed = false;

    std::condition_variable researcherDone;
    std::mutex researcherMutex;
    bool researcherFinished = false;

    // Stage 2: synthesise a factual answer in a background thread.
    std::thread researcherThread([this, &question, &results, &researchAnswer, &researcherFailed,
                                   &researcherDone, &researcherMutex, &researcherFinished]() {
        researchAnswer = m_researcher.research(question, results, m_history);
        if (researchAnswer.isEmpty()) {
            researcherFailed = true;
        }

        {
            std::lock_guard<std::mutex> lock(researcherMutex);
            researcherFinished = true;
        }
        researcherDone.notify_one();
    });

    // Stage 3: wait for researcher to finish, then run roleplayer in parallel if enabled.
    std::thread roleplayerThread;
    if (m_enableRoleplay) {
        roleplayerThread = std::thread([this, &question, &researchAnswer, &roleplayAnswer, &roleplayerFailed,
                                         &researcherDone, &researcherMutex, &researcherFinished]() {
            // Wait for researcher to finish
            {
                std::unique_lock<std::mutex> lock(researcherMutex);
                researcherDone.wait(lock, [&researcherFinished]() { return researcherFinished; });
            }

            roleplayAnswer = m_roleplayer.respond(researchAnswer, question, m_history);
            if (roleplayAnswer.isEmpty()) {
                roleplayerFailed = true;
            }
        });
    }

    // Wait for both threads to complete
    researcherThread.join();
    if (m_enableRoleplay) {
        roleplayerThread.join();
    }

    // Check for errors
    if (researcherFailed) {
        qWarning() << "RAGBot::processQuestion(): researcher returned empty answer";
        return;
    }
    if (m_enableRoleplay && roleplayerFailed) {
        qWarning() << "RAGBot::processQuestion(): roleplayer returned empty answer";
        return;
    }

    m_roleplayDb.logConversation(m_embedder.lastQueryEmbedding(), question, researchAnswer, roleplayAnswer);

    m_history.append({question, researchAnswer, roleplayAnswer});
    if (m_history.size() > MaxHistoryTurns)
        m_history.removeFirst();
}
