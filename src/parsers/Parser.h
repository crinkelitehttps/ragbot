#ifndef PARSER_H
#define PARSER_H

#include <QString>
#include <QJsonValue>


class Parser
{
public:
    virtual ~Parser() = default;
    virtual auto toChunks(const QVariant& dataVariant) -> QStringList = 0;

protected:
    Parser() {}
};

#endif // PARSER_H
