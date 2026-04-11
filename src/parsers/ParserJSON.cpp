#include "ParserJSON.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>


auto ParserJSON::toChunks(const QVariant &dataVariant) -> QStringList
{
    QJsonDocument dataDocument;
    QJsonParseError error;
    
    // Handle different input types
    if (dataVariant.type() == QVariant::String) {
        // Parse JSON from string
        dataDocument = QJsonDocument::fromJson(dataVariant.toString().toUtf8(), &error);
        if (dataDocument.isNull()) {
            qWarning() << "ParserJSON::toChunks(): failed to parse JSON string:" << error.errorString();
            return {};
        }
    } else if (dataVariant.type() == QVariant::ByteArray) {
        // Parse JSON from byte array
        dataDocument = QJsonDocument::fromJson(dataVariant.toByteArray(), &error);
        if (dataDocument.isNull()) {
            qWarning() << "ParserJSON::toChunks(): failed to parse JSON bytes:" << error.errorString();
            return {};
        }
    } else {
        // Try to convert directly (for QJsonDocument, QJsonObject, QJsonArray)
        dataDocument = dataVariant.toJsonDocument();
        if (dataDocument.isNull()) {
            qWarning() << "ParserJSON::toChunks(): unsupported variant type or invalid JSON";
            return {};
        }
    }
    
    QStringList chunks;
    if (dataDocument.isArray()) {
        QJsonArray arr = dataDocument.array();
        for (const QJsonValueRef &val : arr) {
            if (val.isObject()) {
                chunks.append(stringifyObject(val.toObject()));
            }
        }
    } else if (dataDocument.isObject()) {
        chunks.append(stringifyObject(dataDocument.object()));
    }
    return chunks;
}

#if DEBUG_DISABLE
//--------------------------------------------------------------------------------
auto ParserJSON::toChunks(const QVariant &dataVariant) -> QStringList
{
    QJsonParseError error;

    const auto dataObj = dataVariant.toJsonObject();

    if (dataObj.isEmpty()) {
        qWarning() << "ParserJSON::toChunks(): not an object";
    };

    const auto dataDocument = dataVariant.toJsonDocument();
    if (dataDocument.isNull()) {
        qWarning() << "ParserJSON::toChunks(): failed to parse JSON:" << error.errorString();
        return {};
    };

    QStringList chunks;

    if (dataDocument.isArray()) {
        QJsonArray arr = dataDocument.array();
        for (const QJsonValueRef &val : arr) {
            if (val.isObject()) {
                chunks.append(stringifyObject(val.toObject()));
            }
        }
    } else if (dataDocument.isObject()) {
        chunks.append(stringifyObject(dataDocument.object()));
    }

    return chunks;
}

#endif

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

#ifndef DEBUG_DISABLE
//--------------------------------------------------------------------------------
auto ParserJSON::extractTextRecursive(const QJsonValue &value,
        QStringList &texts,
        const QString &prefix
) -> void
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
