#include <QJsonObject>
#include <QJsonArray>
#include "ParserJSON.h"

ParserJSON::ParserJSON() 
{
}

//--------------------------------------------------------------------------------
const QString ParserJSON::extractText(const QByteArray &value) 
{
    QStringList texts;
    extractTextRecursive(QJsonValue::fromVariant(value), texts);
    qDebug().noquote() << "ParserJSON::extractTextFromJson" << texts.join("\n");
    return texts.join(" ");
};


//--------------------------------------------------------------------------------
void ParserJSON::extractTextRecursive(
        const QJsonValue &value,
        QStringList &texts
    )
{
    if (value.isObject()) {
        QJsonObject obj = value.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            QJsonValue val = it.value();
            
            if (val.isString()) {
                texts.append(val.toString());
            } else if (val.isDouble()) {
                texts.append(QString::number(val.toDouble()));
            } else if (val.isBool()) {
                texts.append(val.toBool() ? "true" : "false");
            } else if (val.isObject()) {
                extractTextRecursive(val, texts);
            } else if (val.isArray()) {
                extractTextRecursive(val, texts);
            }
        }
    } else if (value.isArray()) {
        for (const auto &item : value.toArray()) {
            extractTextRecursive(item, texts);
        }
    }
};


