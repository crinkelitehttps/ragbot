#ifndef PARSERJSON_H
#define PARSERJSON_H

#include <QString>
#include <QJsonValue>

#include "Parser.h"

class ParserJSON : public Parser
{
public:
    ParserJSON();

    QString extractTextFromJson(const QJsonValue &value) override;
    void extractTextRecursive(const QJsonValue &value, QStringList &texts) override;

};
#endif // PARSERJSON_H
