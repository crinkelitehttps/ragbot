#include "ParserJSON.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

ParserJSON::ParserJSON() {}


//--------------------------------------------------------------------------------
const QStringList ParserJSON::toChunks(const QByteArray &data)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (doc.isNull()) {
        qWarning() << "ParserJSON::toChunks(): failed to parse JSON:" << error.errorString();
        return {};
    }

    QStringList chunks;

    if (doc.isArray()) {
        QJsonArray arr = doc.array();
        for (const QJsonValue &val : arr) {
            if (val.isObject()) {
                chunks.append(stringifyObject(val.toObject()));
            }
        }
    } else if (doc.isObject()) {
        chunks.append(stringifyObject(doc.object()));
    }

    return chunks;
}

//--------------------------------------------------------------------------------
QString ParserJSON::stringifyObject(const QJsonObject& obj) 
{
    QStringList parts;
    
    if (obj.contains("id")) parts << QString("id is %1").arg(obj["id"].toString());
    if (obj.contains("name")) {
        if (obj["name"].isObject()) {
            parts << QString("name is %1").arg(obj["name"].toObject()["str"].toString());
        } else {
            parts << QString("name is %1").arg(obj["name"].toString());
        }
    }
    
    QStringList details;
    extractTextRecursive(QJsonValue(obj), details);
    
    for (const QString& detail : details) {
        if (!detail.startsWith("id is") && !detail.startsWith("name is")) {
            parts << detail;
        }
    }

    return parts.join(" ");
}

#if 1
//--------------------------------------------------------------------------------
void ParserJSON::extractTextRecursive(const QJsonValue &value, QStringList &texts, const QString &prefix)
{
    if (value.isObject()) {
        const QJsonObject obj = value.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            QString key = it.key();
            if (key == "//" || key == "type" || key == "copy-from") continue;

            const QJsonValue &val = it.value();
            QString fullKey = prefix.isEmpty() ? key : QString("%1 %2").arg(prefix, key);

            if (val.isObject() || val.isArray()) {
                extractTextRecursive(val, texts, fullKey);
            } else {
                QString valStr = val.toVariant().toString();
                texts.append(QString("%1 is %2").arg(fullKey.replace('_', ' '), valStr));
            }
        }
    } else if (value.isArray()) {
        const QJsonArray arr = value.toArray();
        QStringList arrayItems;
        for (const QJsonValue &item : arr) {
            if (item.isString()) arrayItems.append(item.toString());
            else extractTextRecursive(item, texts, prefix);
        }
        if (!arrayItems.isEmpty()) {
            texts.append(QString("%1 includes: %2")
                    .arg(QString(prefix).replace('_', ' '), arrayItems.join(", ")));
        }
    }
}
#endif
