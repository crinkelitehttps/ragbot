#ifndef RAGBOT_COMPAT_TYPES_H
#define RAGBOT_COMPAT_TYPES_H

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace rb {

using String     = std::string;
using StringView = std::string_view;
using Bytes      = std::vector<unsigned char>;
template <class T>          using Vector = std::vector<T>;
template <class K, class V> using Map    = std::unordered_map<K, V>;

}  // namespace rb

namespace rb {

template<class K, class V>
inline bool map_contains(const Map<K,V>& m, const K& key)
{
    return m.count(key) > 0;
}

template<class K, class V>
inline const V& map_get(const Map<K,V>& m, const K& key)
{
    return m.at(key);
}

template<class K, class V, class F>
inline void map_for_each(const Map<K,V>& m, F&& fn)
{
    for (const auto& kv : m)
        fn(kv.first, kv.second);
}

}  // namespace rb

#endif
