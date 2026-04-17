#include "Roleplayer.h"
#include "../generation/GeneratorFactory.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>


//--------------------------------------------------------------------------------
Roleplayer::Roleplayer(const QJsonObject& config)
    : m_generator(GeneratorFactory::createText(config.value("generator").toObject()))
    , m_characterName(config.value("characterName").toString("Survivor"))
    , m_characterBackground(config.value("characterBackground").toString())
{}


//--------------------------------------------------------------------------------
auto Roleplayer::respond(
        const QString& researchAnswer,
        const QString& question,
        const QVector<ConversationTurn>& history
) -> QString
{
    if (!m_generator || !m_generator->isValid()) {
        qWarning() << "Roleplayer::respond(): generator not available";
        return {};
    }

    QString promptTemplate;
    QFile f("inputs/roleplayPrompt.txt");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        promptTemplate = QString::fromUtf8(f.readAll());
    } else {
        promptTemplate =
            "You are %1.\n\n"
            "Here is some factual information to help you answer:\n%2\n\n"
            "Someone asks you: \"%3\"\n";
    }

    QString prompt;
    if (!history.isEmpty()) {
        prompt += "Prior conversation:\n";
        for (const auto& turn : history)
            prompt += "User: " + turn.question + "\n" + m_characterName + ": " + turn.roleplayAnswer + "\n\n";
    }
    prompt += promptTemplate.arg(m_characterName, researchAnswer, question);

    QTextStream(stdout) << "\n" << m_characterName << ": " << Qt::flush;
    const QString answer = m_generator->generateText(m_characterBackground, /*stream=*/true, prompt);
    QTextStream(stdout) << "\n" << Qt::flush;

    return answer;
}
