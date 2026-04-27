#include "Researcher.h"
#include "../generation/GeneratorFactory.h"
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>


//--------------------------------------------------------------------------------
Researcher::Researcher(const QJsonObject& config)
    : m_generator(GeneratorFactory::createText(config.value("generator").toObject()))
    , m_instruction(config.value("instruction").toString(
          "You are a helpful assistant. Answer the question using only the provided context."))
{
    if (!m_generator || !m_generator->isValid()) {
        qWarning() << "Researcher: generator failed to initialise — disabled";
        m_generator.reset();
    }
}


//--------------------------------------------------------------------------------
auto Researcher::research(
        const QString& question,
        const QVector<EmbeddingDatabase::SearchResult>& results,
        const QVector<ConversationTurn>& history
) -> QString
{
    if (!m_generator || !m_generator->isValid()) {
        qWarning() << "Researcher::research(): generator not available";
        return {};
    }

    qDebug() << "Researcher::research(): context chunks (" << results.size() << ")";
    QString context;
    for (int i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        const QString fileName = QFileInfo(r.sourceFile).fileName();
        const bool reranked = r.rerankScore >= 0.0f;
        QString debugScore = QString("sim=%1").arg(r.similarity, 0, 'f', 4);
        if (reranked)
            debugScore += QString("  rerank=%1").arg(r.rerankScore, 0, 'f', 4);
        qDebug().noquote() << QString("  [%1] %2  %3  %4 chars")
            .arg(i).arg(fileName, -40).arg(debugScore).arg(r.content.size());
        qDebug().noquote() << "       " + r.content.left(120).replace('\n', ' ');
        const QString scoreLabel = reranked ? "relevance" : "similarity";
        const float   scoreValue = reranked ? r.rerankScore : r.similarity;
        context += QString("[%1, %2: %3]\n%4\n\n")
            .arg(fileName).arg(scoreLabel).arg(scoreValue, 0, 'f', 3).arg(r.content);
    }
    qDebug() << "Researcher::research(): total context" << context.size() << "chars";

    QString prompt;
    if (!history.isEmpty()) {
        prompt += "Prior conversation:\n";
        for (const auto& turn : history)
            prompt += "Q: " + turn.question + "\nA: " + turn.researchAnswer + "\n\n";
    }
    prompt += "Context:\n" + context + "\nQuestion: " + question;

    QTextStream(stdout) << "\nResearcher: " << Qt::flush;
    const QString answer = m_generator->generateText(m_instruction, /*stream=*/true, prompt);
    QTextStream(stdout) << "\n" << Qt::flush;

    return answer;
}
