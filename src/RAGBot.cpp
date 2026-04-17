#include <QDebug>
#include <QTextStream>
#include "RAGBot.h"

//--------------------------------------------------------------------------------
RAGBot::RAGBot(Embedder& embedder, Researcher& researcher, Roleplayer& roleplayer,
               RoleplayDatabase& roleplayDb)
    : m_embedder(embedder)
    , m_researcher(researcher)
    , m_roleplayer(roleplayer)
    , m_roleplayDb(roleplayDb)
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

    // Stage 2: synthesise a factual answer from the retrieved context.
    const QString researchAnswer = m_researcher.research(question, results);
    if (researchAnswer.isEmpty()) {
        qWarning() << "RAGBot::processQuestion(): researcher returned empty answer";
        return;
    }

    // Stage 3: deliver the answer in-character.
    const QString roleplayAnswer = m_roleplayer.respond(researchAnswer, question);

    m_roleplayDb.logConversation(m_embedder.lastQueryEmbedding(), question, researchAnswer, roleplayAnswer);
}
