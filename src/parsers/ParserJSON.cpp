#include "ParserJSON.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>


//--------------------------------------------------------------------------------
auto ParserJSON::objectToChunk(const QJsonObject& obj) -> Chunk
{
    return {
        stringifyObject(obj),
        QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))
    };
}


//--------------------------------------------------------------------------------
auto ParserJSON::stringifyObject(const QJsonObject& obj) -> QString
{
    QStringList parts;

    if (obj.contains("id"))
        parts << "id is " + obj["id"].toString();

    if (obj.contains("name")) {
        const QString name = obj["name"].isObject()
            ? obj["name"].toObject()["str"].toString()
            : obj["name"].toString();
        if (!name.isEmpty()) parts << "name is " + name;
    }

    QStringList details;
    extractTextRecursive(QJsonValue(obj), details);
    for (const QString& d : details) {
        if (!d.startsWith("id is") && !d.startsWith("name is"))
            parts << d;
    }

    return parts.join(' ');
}


//--------------------------------------------------------------------------------
auto ParserJSON::extractTextRecursive(
        const QJsonValue& value,
        QStringList& texts,
        const QString& prefix
) -> void
{
    if (value.isObject()) {
        const QJsonObject obj = value.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            const QString& key = it.key();
            if (key == "//" || key == "type") continue;

            const QString fullKey = prefix.isEmpty() ? key
                                  : prefix + ' ' + key;
            if (it.value().isObject() || it.value().isArray()) {
                extractTextRecursive(it.value(), texts, fullKey);
            } else {
                texts << QString(fullKey).replace('_', ' ') + " is "
                         + it.value().toVariant().toString();
            }
        }
    } else if (value.isArray()) {
        QStringList items;
        for (const QJsonValue& item : value.toArray()) {
            if (item.isString()) items << item.toString();
            else extractTextRecursive(item, texts, prefix);
        }
        if (!items.isEmpty())
            texts << QString(prefix).replace('_', ' ') + " includes: " + items.join(", ");
    }
}
