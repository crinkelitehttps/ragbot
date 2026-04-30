#include "CDDAResolver.h"
#include "../compat/Logging.h"
#include "../compat/Io.h"
#include <cmath>


static void registerObject(CDDAResolver::Registry& registry, const rb::Json& obj)
{
    rb::String id = obj.stringValue("id");
    if (rb::str_empty(id)) id = obj.stringValue("abstract");
    if (!rb::str_empty(id)) registry[id] = obj;
}

static CDDAResolver::Registry buildFromPaths(const rb::Vector<rb::String>& paths)
{
    CDDAResolver::Registry registry;
    for (const rb::String& path : paths) {
        bool ok = false;
        const rb::Bytes data = rb::read_file_bytes(path, &ok);
        if (!ok) continue;

        const rb::Json doc = rb::Json::parse(data);
        if (doc.isArray()) {
            for (const auto& v : doc.items())
                if (v.isObject()) registerObject(registry, v);
        } else if (doc.isObject()) {
            registerObject(registry, doc);
        }
    }
    return registry;
}


auto CDDAResolver::buildRegistry(const rb::String& dataDir) -> Registry
{
    const auto paths = rb::iter_files_recursive(dataDir, rb::from_std(".json"));
    Registry registry = buildFromPaths(paths);
    RAGBOT_LOG_INFO("CDDAResolver::buildRegistry(): registered {} objects",
                    static_cast<int>(registry.size()));
    return registry;
}


auto CDDAResolver::buildRegistry(const rb::Vector<rb::String>& dataDirs) -> Registry
{
    Registry registry;
    for (const rb::String& dir : dataDirs) {
        const Registry partial = buildRegistry(dir);
        rb::map_for_each(partial, [&](const rb::String& key, const rb::Json& val) {
            registry[key] = val;
        });
    }
    return registry;
}


auto CDDAResolver::buildRegistryFromFiles(const rb::Vector<rb::String>& filePaths) -> Registry
{
    Registry registry = buildFromPaths(filePaths);
    RAGBOT_LOG_INFO("CDDAResolver::buildRegistryFromFiles(): registered {} objects",
                    static_cast<int>(registry.size()));
    return registry;
}


auto CDDAResolver::resolve(const rb::Json& obj, const Registry& registry, int depth) -> rb::Json
{
    if (depth > MaxDepth) {
        RAGBOT_LOG_WARN("CDDAResolver::resolve(): copy-from chain exceeds depth limit");
        rb::Json r = obj;
        r.remove("copy-from");
        return r;
    }

    const rb::String parentId = obj.stringValue("copy-from");
    if (rb::str_empty(parentId) || !rb::map_contains(registry, parentId)) {
        rb::Json r = obj;
        r.remove("copy-from");
        return r;
    }

    const rb::Json resolvedParent = resolve(rb::map_get(registry, parentId), registry, depth + 1);
    rb::Json result = mergeObjects(resolvedParent, obj);
    result.remove("copy-from");
    return result;
}


auto CDDAResolver::mergeObjects(const rb::Json& base, const rb::Json& overlay) -> rb::Json
{
    rb::Json result = base;

    for (const rb::String& key : overlay.keys()) {
        if (key == rb::from_std("copy-from") ||
            key == rb::from_std("relative")  ||
            key == rb::from_std("proportional")) continue;

        const rb::Json val = overlay.value(key);
        if (val.isObject() && result.contains(key) && result.value(key).isObject()) {
            result.set(key, mergeObjects(result.value(key), val));
        } else {
            result.set(key, val);
        }
    }

    // "relative": { field: delta } — add delta to the already-merged value
    const rb::Json rel = overlay.value("relative");
    if (rel.isValid() && rel.isObject()) {
        for (const rb::String& key : rel.keys()) {
            const rb::Json cur   = result.value(key);
            const rb::Json delta = rel.value(key);
            if (cur.isNumber() && delta.isNumber())
                result.setDouble(key, cur.toDouble() + delta.toDouble());
        }
    }

    // "proportional": { field: factor } — multiply the already-merged value by factor
    const rb::Json prop = overlay.value("proportional");
    if (prop.isValid() && prop.isObject()) {
        for (const rb::String& key : prop.keys()) {
            const rb::Json cur    = result.value(key);
            const rb::Json factor = prop.value(key);
            if (cur.isNumber() && factor.isNumber())
                result.setDouble(key, std::round(cur.toDouble() * factor.toDouble()));
        }
    }

    return result;
}
