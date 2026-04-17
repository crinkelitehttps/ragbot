#include "CDDAResolver.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDebug>


//--------------------------------------------------------------------------------
auto CDDAResolver::buildRegistry(const QString& dataDir) -> Registry
{
    Registry registry;

    QDirIterator it(dataDir, {"*.json"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFile file(it.next());
        if (!file.open(QIODevice::ReadOnly)) continue;

        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();

        auto registerObject = [&](const QJsonObject& obj) {
            // Concrete objects use "id"; abstract base templates use "abstract".
            const QString id = obj.value("id").toString(
                                   obj.value("abstract").toString());
            if (!id.isEmpty()) registry.insert(id, obj);
        };

        if (doc.isArray()) {
            for (const QJsonValue& v : doc.array())
                if (v.isObject()) registerObject(v.toObject());
        } else if (doc.isObject()) {
            registerObject(doc.object());
        }
    }

    qDebug() << "CDDAResolver::buildRegistry(): registered" << registry.size() << "objects";
    return registry;
}


//--------------------------------------------------------------------------------
auto CDDAResolver::resolve(const QJsonObject& obj, const Registry& registry, int depth)
    -> QJsonObject
{
    if (depth > MaxDepth) {
        qWarning() << "CDDAResolver::resolve(): copy-from chain exceeds depth limit";
        QJsonObject r = obj;
        r.remove("copy-from");
        return r;
    }

    const QString parentId = obj.value("copy-from").toString();
    if (parentId.isEmpty() || !registry.contains(parentId)) {
        QJsonObject r = obj;
        r.remove("copy-from");
        return r;
    }

    const QJsonObject resolvedParent = resolve(registry.value(parentId), registry, depth + 1);
    QJsonObject result = mergeObjects(resolvedParent, obj);
    result.remove("copy-from");
    return result;
}


//--------------------------------------------------------------------------------
auto CDDAResolver::mergeObjects(const QJsonObject& base, const QJsonObject& overlay)
    -> QJsonObject
{
    QJsonObject result = base;
    for (auto it = overlay.begin(); it != overlay.end(); ++it) {
        if (it.key() == "copy-from") continue;

        if (it.value().isObject()
                && result.contains(it.key())
                && result.value(it.key()).isObject()) {
            result[it.key()] = mergeObjects(result.value(it.key()).toObject(),
                                            it.value().toObject());
        } else {
            result[it.key()] = it.value();
        }
    }
    return result;
}
