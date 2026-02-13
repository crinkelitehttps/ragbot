#ifndef PARSER_H
#define PARSER_H

#include <QString>
#include <QJsonValue>


class Parser
{
public:
    virtual ~Parser() = default;
    virtual const QString extractText(const QByteArray &value) = 0;

protected:
    Parser() {}
};

#endif // PARSER_H
