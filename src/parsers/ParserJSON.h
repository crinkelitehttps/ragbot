#ifndef PARSERJSON_H
#define PARSERJSON_H

#include <QString>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>

#include "Parser.h"

class ParserJSON : public Parser
{
public:
    ParserJSON() = default;

    auto toChunks(const QVariant& dataVariant) -> QVector<Chunk> override;

    // Provide the full id→object map so copy-from chains can be resolved.
    void setObjectRegistry(const QHash<QString, QJsonObject>& registry) override
    {
        m_registry = registry;
    }

private:
    // Recursively merges copy-from inheritance. Returns the fully resolved object.
    // depth guards against circular references.
    auto resolveObject(const QJsonObject& obj, int depth = 0) const -> QJsonObject;

    // Shallow-merge src fields into dst; nested objects are merged recursively,
    // arrays and scalars in src overwrite dst.
    static auto mergeObjects(const QJsonObject& base, const QJsonObject& overlay) -> QJsonObject;

    auto stringifyObject(const QJsonObject& obj) -> QString;

    auto extractTextRecursive(
        const QJsonValue& value,
        QStringList& texts,
        const QString& prefix = QString()
    ) -> void;

    QHash<QString, QJsonObject> m_registry;
};

#endif // PARSERJSON_H
