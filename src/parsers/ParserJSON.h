#ifndef PARSERJSON_H
#define PARSERJSON_H

#include <QString>
#include <QJsonValue>

#include "Parser.h"

class ParserJSON : public Parser
{
public:
    ParserJSON() = default; 

    auto toChunks(const QVariant& dataVariant) -> QStringList override;

private:
    auto extractTextRecursive(
        const QJsonValue& value,
        QStringList& texts,
        const QString& prefix = QString()
    ) -> void;

    auto stringifyObject(const QJsonObject& obj) -> QString ;
};
#endif // PARSERJSON_H
