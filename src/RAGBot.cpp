#include <QTextStream>
#include <QDebug>
#include <QCoreApplication>
#include <QFile>

#include "RAGBot.h"
#include "generation/GeneratorImmediate.h"
#include "generation/GeneratorIP.h"

// llama_silenced.h

//--------------------------------------------------------------------------------
RAGBot::RAGBot(
        ConfigEmbed embedConfig,
        ConfigResearch researchConfig,
        ConfigRoleplay roleplayConfig
    )
    : m_embedConfig(embedConfig)
    , m_researchConfig(researchConfig)
    , m_roleplayConfig(roleplayConfig)
    , m_embedder(embedConfig)
    , m_researcher(researchConfig)
    , m_roleplayer(roleplayConfig)
{
    qDebug() << "RAGBot::RAGBot()";
}


//--------------------------------------------------------------------------------
void RAGBot::startChatLoop()
{
    QTextStream in(stdin);
    
    while (true) {
        QTextStream(stdout) << "\nYou: " << Qt::flush;
        QString question = in.readLine().trimmed();
        
        if (question.isEmpty()) continue;
        if (question.toLower() == "quit" || question.toLower() == "exit") {
            qDebug() << "Goodbye!";
            break;
        }
        processQuestion(question);
    }
    
    cleanup();
    QCoreApplication::quit();
}


//--------------------------------------------------------------------------------
void RAGBot::processQuestion(const QString &question)
{
    qDebug() << "RAGBot::processQuestion(): " << question;

    const auto buildGenerator = [](ConfigGenerator &config) -> Generator* {
        auto driverLoaded = [](Generator* generator) {
            if (generator && generator->isValid()) {
                return generator;
            }
            delete generator;
            return static_cast<Generator*>(nullptr);
        };
        if (config.isImmediate) {
            if (auto* generator = driverLoaded(new GeneratorImmediate(config))) {
                qDebug() << "RAGBot::processQuestion() [ GeneratorImmediate ]";
                return generator;
            }
        }
        if (auto* generator = driverLoaded(new GeneratorIP(config))) {
            qDebug() << "RAGBot::processQuestion() [ GeneratorIP ]";
            return generator;
        }
        return static_cast<Generator*>(nullptr);
    };

    const auto truncateTo = [](const QString &text, int maxChars) {
        if (maxChars <= 0 || text.size() <= maxChars) {
            return text;
        }
        return text.left(maxChars);
    };

    ConfigGenerator embedGenConfig = m_embedConfig.generatorConfig;
    Generator *embedGen = buildGenerator(embedGenConfig);
    if (!embedGen) {
        qWarning() << "RAGBot::processQuestion(): No valid embedding generator";
        return;
    }

    QVector<float> queryEmb = embedGen->generate(question);
    delete embedGen;

    if (queryEmb.isEmpty()) {
        qWarning() << "RAGBot::processQuestion(): Failed to generate query embedding";
        return;
    }
    
    auto results = m_embed_db.search(queryEmb, 10);
    
    if (results.isEmpty()) {
        qDebug() << "RAGBot::processQuestio(): No relevant documents found";
        return;
    }
    
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
}


//--------------------------------------------------------------------------------
void RAGBot::cleanup()
{
    qDebug() << "RAGBot::cleanup()";
#if 0
    if (m_embedCtx) {
        llama_free(m_embedCtx);
        m_embedCtx = nullptr;
    }
    if (m_embedModel) {
        llama_model_free(m_embedModel);
        m_embedModel = nullptr;
    }
    llama_backend_free();
#endif
}
