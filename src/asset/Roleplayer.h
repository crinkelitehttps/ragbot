#ifndef ROLEPLAYER_H
#define ROLEPLAYER_H

#include <QJsonObject>
#include <QString>
#include "../generation/Generator.h"

class Roleplayer
{
public:
    explicit Roleplayer(const QJsonObject& config);
    ~Roleplayer() { delete m_generator; }

    // Delivers researchAnswer in-character as a response to question.
    auto respond(const QString& researchAnswer, const QString& question) -> QString;

    [[nodiscard]] auto characterName() const -> const QString& { return m_characterName; }

private:
    Generator* m_generator {};
    QString    m_characterName;
    QString    m_characterBackground;
};

#endif // ROLEPLAYER_H
