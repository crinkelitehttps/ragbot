#ifndef RAGBOT_COMPAT_JSON_H
#define RAGBOT_COMPAT_JSON_H

#include "Types.h"
#include <cstddef>
#include <cstdint>
#include <string_view>

#ifdef RAGBOT_USE_QT
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#else
#include <nlohmann/json.hpp>
#endif

namespace rb {

// Lean JSON façade. Wraps either QJsonValue (Qt) or nlohmann::json. Designed
// for the access patterns this codebase actually uses: read config, build
// HTTP request bodies, walk CDDA registry objects.
class Json
{
public:
    // -------- construction --------
    Json();                                // null
    static auto object() -> Json;
    static auto array() -> Json;
    static auto fromString(const String& s) -> Json;
    static auto fromBool(bool v) -> Json;
    static auto fromInt(long long v) -> Json;
    static auto fromDouble(double v) -> Json;

    // Parse. Returns null Json on error; check isValid().
    static auto parse(const Bytes& data) -> Json;
    static auto parse(StringView text) -> Json;
#ifdef RAGBOT_USE_QT
    // In Qt mode StringView is QStringView, so a std::string_view overload is
    // needed for call sites that pass raw UTF-8 chunks (e.g. SSE framer).
    static auto parse(std::string_view utf8) -> Json;
#endif

    // -------- introspection --------
    [[nodiscard]] auto isValid() const -> bool;   // false if parse failed or null-uninitialised
    [[nodiscard]] auto isNull() const -> bool;
    [[nodiscard]] auto isObject() const -> bool;
    [[nodiscard]] auto isArray() const -> bool;
    [[nodiscard]] auto isString() const -> bool;
    [[nodiscard]] auto isBool() const -> bool;
    [[nodiscard]] auto isNumber() const -> bool;

    // -------- scalar extraction --------
    [[nodiscard]] auto toString(StringView default_value = {}) const -> String;
    [[nodiscard]] auto toBool(bool default_value = false) const -> bool;
    [[nodiscard]] auto toInt(int default_value = 0) const -> int;
    [[nodiscard]] auto toLongLong(long long default_value = 0) const -> long long;
    [[nodiscard]] auto toDouble(double default_value = 0.0) const -> double;

    // -------- object access --------
    [[nodiscard]] auto contains(const String& key) const -> bool;
    [[nodiscard]] auto value(const String& key) const -> Json;   // null Json if missing
    [[nodiscard]] auto stringValue(const String& key, const String& default_value = {}) const -> String;
    [[nodiscard]] auto boolValue(const String& key, bool default_value = false) const -> bool;
    [[nodiscard]] auto intValue(const String& key, int default_value = 0) const -> int;
    [[nodiscard]] auto doubleValue(const String& key, double default_value = 0.0) const -> double;
    [[nodiscard]] auto keys() const -> Vector<String>;
    auto remove(const String& key) -> void;

    // -------- array access --------
    [[nodiscard]] auto size() const -> std::size_t;
    [[nodiscard]] auto at(std::size_t index) const -> Json;
    [[nodiscard]] auto items() const -> Vector<Json>;

    // -------- mutation (for building request bodies) --------
    auto set(const String& key, const Json& value) -> void;
    auto setString(const String& key, const String& value) -> void;
    auto setBool(const String& key, bool value) -> void;
    auto setInt(const String& key, long long value) -> void;
    auto setDouble(const String& key, double value) -> void;
    auto append(const Json& value) -> void;

    // -------- serialization --------
    [[nodiscard]] auto dump(bool compact = true) const -> Bytes;
    [[nodiscard]] auto dumpString(bool compact = true) const -> String;

#ifdef RAGBOT_USE_QT
    // Escape hatches while migrating Phase 2 — Qt-mode-only.
    explicit Json(const QJsonValue& v) : m_value(v), m_valid(!v.isUndefined()) {}
    explicit Json(const QJsonObject& o) : m_value(o), m_valid(true) {}
    explicit Json(const QJsonArray& a)  : m_value(a), m_valid(true) {}
    [[nodiscard]] auto qtValue() const -> const QJsonValue& { return m_value; }
    [[nodiscard]] auto toQJsonObject() const -> QJsonObject { return m_value.toObject(); }
    [[nodiscard]] auto toQJsonArray() const -> QJsonArray  { return m_value.toArray(); }
#else
    explicit Json(nlohmann::json v) : m_value(std::move(v)), m_valid(true) {}
    [[nodiscard]] auto stdValue() const -> const nlohmann::json& { return m_value; }
    [[nodiscard]] auto stdValue() -> nlohmann::json& { return m_value; }
#endif

private:
#ifdef RAGBOT_USE_QT
    QJsonValue m_value;
#else
    nlohmann::json m_value;
#endif
    bool m_valid { false };
};

}  // namespace rb

#endif
