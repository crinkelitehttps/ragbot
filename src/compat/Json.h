#ifndef RAGBOT_COMPAT_JSON_H
#define RAGBOT_COMPAT_JSON_H

#include "Types.h"
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <nlohmann/json.hpp>

namespace rb {

// Lean JSON façade wrapping nlohmann::json.
// Access patterns: read config, build HTTP request bodies, walk CDDA registry objects.
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

    // -------- introspection --------
    [[nodiscard]] auto isValid() const -> bool;
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
    [[nodiscard]] auto value(const String& key) const -> Json;
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

    explicit Json(nlohmann::json v) : m_value(std::move(v)), m_valid(true) {}
    [[nodiscard]] auto stdValue() const -> const nlohmann::json& { return m_value; }
    [[nodiscard]] auto stdValue() -> nlohmann::json& { return m_value; }

private:
    nlohmann::json m_value;
    bool m_valid { false };
};

}  // namespace rb

#endif
