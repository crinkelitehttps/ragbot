#ifndef PARSERJSON_H
#define PARSERJSON_H

#include <QString>
#include <QJsonValue>

#include "Parser.h"

class ParserJSON : public Parser
{
public:
    ParserJSON();

    const QString extractText(const QByteArray& value) override;
private:
    void extractTextRecursive(
        const QJsonValue& value,
        QStringList& texts,
        const QString& prefix = QString()
    );

};
#endif // PARSERJSON_H
