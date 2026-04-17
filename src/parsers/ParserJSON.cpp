#include "ParserJSON.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>


auto ParserJSON::toChunks(const QVariant& dataVariant) -> QVector<Chunk>
{
    QJsonDocument doc;
    QJsonParseError error;

    if (dataVariant.type() == QVariant::String) {
        doc = QJsonDocument::fromJson(dataVariant.toString().toUtf8(), &error);
    } else if (dataVariant.type() == QVariant::ByteArray) {
        doc = QJsonDocument::fromJson(dataVariant.toByteArray(), &error);
    } else {
        doc = dataVariant.toJsonDocument();
    }

    if (doc.isNull()) {
        qWarning() << "ParserJSON::toChunks(): failed to parse JSON:" << error.errorString();
        return {};
    }

    QVector<Chunk> chunks;

    auto addObject = [&](const QJsonObject& obj) {
        Chunk chunk;
        chunk.embedText = stringifyObject(obj);
        chunk.content   = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        chunks.append(chunk);
    };

    if (doc.isArray()) {
        for (const QJsonValue& val : doc.array()) {
            if (val.isObject()) addObject(val.toObject());
        }
    } else if (doc.isObject()) {
        addObject(doc.object());
    }

    return chunks;
}


//--------------------------------------------------------------------------------
auto ParserJSON::stringifyObject(const QJsonObject& obj) -> QString
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
            if (key == "//" || key == "type" || key == "copy-from") continue;

            const QJsonValue& val = it.value();
            const QString fullKey = prefix.isEmpty() ? key : QString("%1 %2").arg(prefix, key);

            if (val.isObject() || val.isArray()) {
                extractTextRecursive(val, texts, fullKey);
            } else {
                texts.append(QString("%1 is %2")
                    .arg(QString(fullKey).replace('_', ' '), val.toVariant().toString()));
            }
        }
    } else if (value.isArray()) {
        const QJsonArray arr = value.toArray();
        QStringList arrayItems;
        for (const QJsonValue& item : arr) {
            if (item.isString()) arrayItems.append(item.toString());
            else extractTextRecursive(item, texts, prefix);
        }
        if (!arrayItems.isEmpty()) {
            texts.append(QString("%1 includes: %2")
                .arg(QString(prefix).replace('_', ' '), arrayItems.join(", ")));
        }
    }
}
