#ifndef CDDARESOLVER_H
#define CDDARESOLVER_H

#include "../compat/Json.h"
#include "../compat/Types.h"

// Resolves Cataclysm: Dark Days Ahead copy-from inheritance.
class CDDAResolver
{
public:
    using Registry = rb::Map<rb::String, rb::Json>;

    static auto buildRegistry(const rb::String& dataDir) -> Registry;
    static auto buildRegistry(const rb::Vector<rb::String>& dataDirs) -> Registry;
    static auto buildRegistryFromFiles(const rb::Vector<rb::String>& filePaths) -> Registry;

    static auto resolve(const rb::Json& obj, const Registry& registry, int depth = 0)
        -> rb::Json;

private:
    CDDAResolver() = delete;

    static auto mergeObjects(const rb::Json& base, const rb::Json& overlay) -> rb::Json;

    static constexpr int MaxDepth { 16 };
};

#endif // CDDARESOLVER_H
