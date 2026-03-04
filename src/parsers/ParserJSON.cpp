#include "ParserJSON.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

ParserJSON::ParserJSON() 
{

}


//--------------------------------------------------------------------------------
const QString ParserJSON::extractText(const QByteArray &value)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(value, &error);
    if (doc.isNull()) {
        qWarning() << "ParserJSON: failed to parse JSON:" << error.errorString();
        return {};
    }

    qDebug() << "ParserJSON: " << value;
    QStringList texts;
    if (doc.isObject())
        extractTextRecursive(QJsonValue(doc.object()), texts);
    else if (doc.isArray())
        extractTextRecursive(QJsonValue(doc.array()), texts);

    return texts.join(" ");
}


//--------------------------------------------------------------------------------
void ParserJSON::extractTextRecursive(
        const QJsonValue &value,
        QStringList &texts,
        const QString &prefix
    )
{
    if (value.isObject()) {
        const QJsonObject obj = value.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            QString key = it.key();
            
            // 1. Skip technical noise that doesn't help Roleplay
            if (key == "//" || key == "type" || key == "copy-from") continue;

            const QJsonValue &val = it.value();
            // Create a breadcrumb trail (e.g., "armor_data coverage")
            QString fullKey = prefix.isEmpty() ? key : QString("%1 %2").arg(prefix, key);

            if (val.isObject() || val.isArray()) {
                extractTextRecursive(val, texts, fullKey);
            } else {
                QString valStr;
                if (val.isString()) valStr = val.toString();
                else if (val.isDouble()) valStr = QString::number(val.toDouble());
                else if (val.isBool()) valStr = val.toBool() ? "true" : "false";

                // 2. Format as a descriptive sentence fragment
                // Example: "bash damage is 15" instead of "bash: 15"
                texts.append(QString("%1 is %2").arg(fullKey.replace('_', ' '), valStr));
            }
        }
    } else if (value.isArray()) {
        const QJsonArray arr = value.toArray();
        QStringList arrayItems;
        for (const QJsonValue &item : arr) {
            if (item.isString()) {
                arrayItems.append(item.toString());
            } else {
                extractTextRecursive(item, texts, prefix);
            }
        }
        // 3. Handle arrays of strings (like flags or materials) cleanly

#if 1
        if (!arrayItems.isEmpty()) {
            texts.append(QString("%1 includes: %2")
                    .arg(QString(prefix).replace('_', ' '), arrayItems.join(", ")));
        }
#endif
    }
}

