#ifndef PARSER_H
#define PARSER_H

#include <QString>
#include <QJsonValue>


class Parser
{
public:
    virtual ~Parser() = default;
    virtual QString extractTextFromJson(const QJsonValue &value) = 0;
    virtual void extractTextRecursive(const QJsonValue &value, QStringList &texts) = 0;

protected:
    Parser() {}
};

#endif // PARSER_H
