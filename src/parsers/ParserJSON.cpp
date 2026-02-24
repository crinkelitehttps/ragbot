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
void ParserJSON::extractTextRecursive(const QJsonValue &value, QStringList &texts)
{
    if (value.isObject()) {
        const QJsonObject obj = value.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            const QString &key = it.key();
            const QJsonValue &val = it.value();

            if (val.isString()) {
                const QString str = val.toString();
                if (str.length() > 1 && (str.startsWith('{') || str.startsWith('['))) {
                    QJsonDocument nested = QJsonDocument::fromJson(str.toUtf8());
                    if (!nested.isNull()) {
                        extractTextRecursive(
                            nested.isObject() ? QJsonValue(nested.object())
                                              : QJsonValue(nested.array()),
                            texts);
                        continue;
                    }
                }
                texts.append(QStringLiteral("%1: %2").arg(key, str));
            } else if (val.isDouble()) {
                texts.append(QStringLiteral("%1: %2").arg(key, QString::number(val.toDouble())));
            } else if (val.isBool()) {
                texts.append(QStringLiteral("%1: %2").arg(key, val.toBool() ? "true" : "false"));
            } else {
                extractTextRecursive(val, texts);
            }
        }
    } else if (value.isArray()) {
        for (const QJsonValue &item : value.toArray())
            extractTextRecursive(item, texts);
    } else if (value.isString()) {
        texts.append(value.toString());
    }
}
