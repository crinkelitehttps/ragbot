#ifndef RAGBOT_COMPAT_TYPES_H
#define RAGBOT_COMPAT_TYPES_H

#ifdef RAGBOT_USE_QT
#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringView>
#include <QVector>
#else
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#endif

namespace rb {

#ifdef RAGBOT_USE_QT
using String     = QString;
using StringView = QStringView;
using Bytes      = QByteArray;
template <class T>          using Vector = QVector<T>;
template <class K, class V> using Map    = QHash<K, V>;
#else
using String     = std::string;
using StringView = std::string_view;
using Bytes      = std::vector<unsigned char>;
template <class T>          using Vector = std::vector<T>;
template <class K, class V> using Map    = std::unordered_map<K, V>;
#endif

}  // namespace rb

// ---------------------------------------------------------------------------
// Portable map helpers — hide QHash vs std::unordered_map iterator differences
// ---------------------------------------------------------------------------
namespace rb {

template<class K, class V>
inline bool map_contains(const Map<K,V>& m, const K& key)
{
#ifdef RAGBOT_USE_QT
    return m.contains(key);
#else
    return m.count(key) > 0;
#endif
}

template<class K, class V>
inline const V& map_get(const Map<K,V>& m, const K& key)
{
#ifdef RAGBOT_USE_QT
    return *m.find(key);
#else
    return m.at(key);
#endif
}

template<class K, class V, class F>
inline void map_for_each(const Map<K,V>& m, F&& fn)
{
#ifdef RAGBOT_USE_QT
    for (auto it = m.cbegin(); it != m.cend(); ++it)
        fn(it.key(), it.value());
#else
    for (const auto& kv : m)
        fn(kv.first, kv.second);
#endif
}

}  // namespace rb

#endif
