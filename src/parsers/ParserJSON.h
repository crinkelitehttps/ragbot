#ifndef PARSERJSON_H
#define PARSERJSON_H

#include <QString>
#include <QJsonValue>

#include "Parser.h"

class ParserJSON : public Parser
{
public:
    ParserJSON();

    const QStringList toChunks(const QByteArray& data) override;

private:
    void extractTextRecursive(
        const QJsonValue& value,
        QStringList& texts,
        const QString& prefix = QString()
    );

    QString stringifyObject(const QJsonObject& obj);
};
#endif // PARSERJSON_H
