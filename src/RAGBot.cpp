#include "RAGBot.h"
#include <QTextStream>
#include <QDebug>
#include <QCoreApplication>
#include <QFile>
#include "Embedder.h"
// llama_silenced.h
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"

#include "common.h"

#pragma GCC diagnostic pop


RAGBot::RAGBot(
        ConfigEmbed &embedConfig,
        ConfigResearch &researchConfig,
        ConfigRoleplay &roleplayConfig
    )
    : m_embedder(embedConfig)
    , m_researcher(researchConfig)
    , m_roleplayer(roleplayConfig)
{
    qDebug() << "RAGBot::RAGBot()";
}


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


void RAGBot::processQuestion(const QString &question)
{
#if 1
    qDebug() << "RAGBot::processQuestion(): " << question;
    
    QVector<float> queryEmb = generateEmbedding(question);
    if (queryEmb.isEmpty()) {
        qWarning() << "RAGBot::processQuestion(): Failed to generate query embedding";
        return;
    }
    
    auto results = m_db->search(queryEmb, 10);
    
    if (results.isEmpty()) {
        qDebug() << "RAGBot::processQuestio(): No relevant documents found";
        return;
    }
    
    qDebug() << QString("RAGBot::processQuestion(): Found %1 relevant documents").arg(results.size());
    
    // Build context from top results
    QString context;
    for (int i = 0; i < results.size(); i++) {
        context += QString("RAGBot::processQuestion(): Document %1 (similarity: %2):\n%3\n\n")
            .arg(i + 1)
            .arg(results[i].similarity, 0, 'f', 3)
            .arg(results[i].content);
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
    qDebug() << researchAnswer;
    
    if (m_llmConfig.enabled) {
        qInfo() << "m_llmConfig.enabled";
        QTextStream(stdout) << "\nBot (Research): " << Qt::flush;
        researchAnswer = m_remoteLLM->chat("", researchPrompt, false);
        QTextStream(stdout) << "\n" << Qt::flush;
    } else {
        qDebug() << "Would call local research model here";
        researchAnswer = "[Research answer would appear here with local model]";
    }
    
    if (researchAnswer.isEmpty()) {
        qWarning() << "No response from research LLM";
        return;
    }
    
    QString roleplayAnswer;
    
    // Stage 2: Roleplay response
    if (m_rpConfig.enabled && m_roleplayLLM) {
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
        
        QTextStream(stdout) << "\n" << m_rpConfig.characterName << ": " << Qt::flush;
        roleplayAnswer = m_roleplayLLM->chat(
            m_rpConfig.characterBackground,
            roleplayPrompt,
            true
        );

        QTextStream(stdout) << "\n" << Qt::flush;
        
        if (roleplayAnswer.isEmpty()) {
            qWarning() << "RAGBot::processQuestio(): No response from roleplay LLM";
        }
    }
    
    // Log conversation to database
    if (!m_convDb->logConversation(queryEmb, question, researchAnswer, roleplayAnswer)) {
        qWarning() << "RAGBot::processQuestion(): Failed to log conversation to database";
    }
#endif
}

void RAGBot::cleanup()
{
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
