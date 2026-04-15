#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include "RAGBot.h"

//--------------------------------------------------------------------------------
RAGBot::RAGBot(const Embedder& embedder, const Researcher& researcher, const Roleplayer& roleplayer)
    : m_embedder(embedder)
    , m_researcher(researcher)
    , m_roleplayer(roleplayer)
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

    QCoreApplication::quit();
}


//--------------------------------------------------------------------------------
auto RAGBot::processQuestion(const QString& question) -> void
{
    qDebug() << "RAGBot::processQuestion():" << question;

    // TODO(task 2): embed question via m_embedder, search DB, pipe through researcher → roleplayer
    // Stage 1 — embed + retrieve: m_embedder.search(question) → QVector<SearchResult>
    // Stage 2 — research:         m_researcher.research(question, results) → QString
    // Stage 3 — roleplay:         m_roleplayer.respond(researchAnswer, question) → QString
    // Stage 4 — log:              m_conversationDb.logConversation(...)

    QTextStream(stdout) << "[pipeline not yet wired]\n" << Qt::flush;
}


