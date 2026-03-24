#ifndef PARSER_H
#define PARSER_H

#include <QString>
#include <QJsonValue>


class Parser
{
public:
    virtual ~Parser() = default;
    virtual auto bytesChunks(const QByteArray& value) -> QByteArrayList = 0;

protected:
    Parser() {}
};

#endif // PARSER_H
