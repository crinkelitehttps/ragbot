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
        QString question = tin.readLine().trimmed();
        
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
auto  RAGBot::processQuestion(const QString &question) -> void
{
    qDebug() << "RAGBot::processQuestion(): " << question;

    ///m_researcher.research(question, m_embedder.queryResults(question));
    // m_embedder.generationEmbed("placeholder");


#if DEBUG_DISABLED
    const auto truncateTo = [](const QString &text, int maxChars) {
        if (maxChars <= 0 || text.size() <= maxChars) {
            return text;
        }
        return text.left(maxChars);
    };


    auto results = m_embedder.textResults(question, 10);

    if (results.isEmpty()) {
        qDebug() << "RAGBot::processQuestion(): No relevant documents found";
        return;
    }

    auto output = m_researcher.research(question, results);
    
    qDebug() << QString("RAGBot::processQuestion(): Found %1 relevant documents").arg(results.size());
    
    // Build context from top results
    QString context;
    const int maxContextChars = 8000;
    for (int i = 0; i < results.size(); i++) {
        QString next = QString("RAGBot::processQuestion(): Document %1 (similarity: %2):\n%3\n\n")
            .arg(i + 1)
            .arg(results[i].similarity, 0, 'f', 3)
            .arg(results[i].content);
        if (context.size() + next.size() > maxContextChars) {
            context += truncateTo(next, maxContextChars - context.size());
            break;
        }
        context += next;
    }
    
    // Stage 1: Research with remote LLM
    qDebug() << "RAGBot::processQuestion(): Research Query";
    
    QString researchPrompt = QString(
        "You are a helpful assistant analyzing Cataclysm: Dark Days Ahead game data. "
        "Answer the question based on the provided context documents. "
        "Be factual, concise, and cite specific game mechanics, items, or data when relevant.\n\n"
        "Context:\n%1\n\n"
        "Question: %2"
    ).arg(context, question);
    
    QString researchAnswer;
    qDebug() << context;
    qDebug() << question;

    ConfigGenerator researchGenConfig = m_researchConfig.generatorConfig;
    researchGenConfig.isImmediate = true;
    Generator *researchGen = buildGenerator(researchGenConfig);
    if (researchGen) {
        qInfo() << "RAGBot::processQuestion(): Research generator ready";
        QTextStream(stdout) << "\nBot (Research): " << Qt::flush;
        researchAnswer = researchGen->generateText(
            m_researchConfig.instruction,
            researchPrompt,
            false
        );
        QTextStream(stdout) << "\n" << Qt::flush;
        delete researchGen;
    } else {
        qWarning() << "RAGBot::processQuestion(): No research generator; skipping";
    }
    
    if (researchAnswer.isEmpty()) {
        qWarning() << "No response from research LLM";
        return;
    }
    
    QString roleplayAnswer;
    
    // Stage 2: Roleplay response
    if (m_roleplayConfig.generatorConfig.isValid()) {
        qDebug() << "RAGBot::processQuestion(): Roleplay Response";

        QFile roleplayPromptFile("roleplayPrompt.txt");
        QString rp;
        
        if (roleplayPromptFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            rp = QString::fromUtf8(roleplayPromptFile.readAll());
            roleplayPromptFile.close();
        } else {
            qWarning() << "RAGBot::processQuestion(): Failed to open roleplayPrompt.txt";
        }
        
        QString roleplayPrompt = rp.arg("Survivor", researchAnswer, question);
        qDebug() << roleplayPrompt;
        
        ConfigGenerator roleplayGenConfig = m_roleplayConfig.generatorConfig;
        Generator *roleplayGen = buildGenerator(roleplayGenConfig);
        if (roleplayGen) {
            QTextStream(stdout) << "\n" << m_roleplayConfig.characterName << ": " << Qt::flush;
            roleplayAnswer = roleplayGen->generateText(
                m_roleplayConfig.characterBackground,
                roleplayPrompt,
                true
            );
            QTextStream(stdout) << "\n" << Qt::flush;
            delete roleplayGen;
        } else {
            qWarning() << "RAGBot::processQuestion(): No roleplay generator; skipping";
        }
        
        if (roleplayAnswer.isEmpty()) {
            qWarning() << "RAGBot::processQuestio(): No response from roleplay LLM";
        }
    }
    
    // Log conversation to database
    if (!m_conversation_db.logConversation(queryEmb, question, researchAnswer, roleplayAnswer)) {
        qWarning() << "RAGBot::processQuestion(): Failed to log conversation to database";
    }
#endif
}


