#ifndef ROLEPLAYER_H
#define ROLEPLAYER_H

#include <memory>
#include <QJsonObject>
#include <QString>
#include "../ConversationTurn.h"
#include "../generation/TextGenerator.h"

class Roleplayer
{
public:
    explicit Roleplayer(const QJsonObject& config);

    auto respond(
        const QString& researchAnswer,
        const QString& question,
        const QVector<ConversationTurn>& history,
        const TextGenerator::TokenSink& tokenSink = {}
    ) -> QString;

    [[nodiscard]] auto characterName() const -> const QString& { return m_characterName; }

private:
    std::unique_ptr<TextGenerator> m_generator;
    QString                        m_characterName;
    QString                        m_characterBackground;
    QString                        m_assetsDir;
};

#endif // ROLEPLAYER_H
