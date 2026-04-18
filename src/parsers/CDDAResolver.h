#ifndef CDDARESOLVER_H
#define CDDARESOLVER_H

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>

// Resolves Cataclysm: Dark Days Ahead copy-from inheritance.
// Objects in CDDA JSON use "copy-from" to inherit fields from a named base object.
// CDDAResolver builds a registry of all known objects and merges inheritance chains
// so that each resolved object contains its complete effective data.
class CDDAResolver
{
public:
    using Registry = QHash<QString, QJsonObject>;

    // Scan all *.json files under dataDir and return a registry of all objects
    // keyed by their "id" or "abstract" field.
    static auto buildRegistry(const QString& dataDir) -> Registry;
    static auto buildRegistry(const QStringList& dataDirs) -> Registry;

    // Return obj with all copy-from fields merged in from the registry.
    // Child fields take precedence over parent fields.
    // Returns the object unchanged (minus copy-from) if the parent is not found.
    static auto resolve(const QJsonObject& obj, const Registry& registry, int depth = 0)
        -> QJsonObject;

private:
    CDDAResolver() = delete;

    static auto mergeObjects(const QJsonObject& base, const QJsonObject& overlay)
        -> QJsonObject;

    static constexpr int MaxDepth { 16 };
};

#endif // CDDARESOLVER_H
