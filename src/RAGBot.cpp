#include <QDebug>
#include <QTextStream>
#include "RAGBot.h"
#include "RAGBotSession.h"


//--------------------------------------------------------------------------------
RAGBot::RAGBot(RAGBotSession& session)
    : m_session(session)
{
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
        m_session.ask(question);
    }
}
