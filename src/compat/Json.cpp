#include "Json.h"
#include "Strings.h"

#include <utility>

namespace rb {

// ============================================================================
// Construction
// ============================================================================
Json::Json() : m_valid(false) {}

auto Json::object() -> Json
{
    Json j;
    j.m_value = nlohmann::json::object();
    j.m_valid = true;
    return j;
}

auto Json::array() -> Json
{
    Json j;
    j.m_value = nlohmann::json::array();
    j.m_valid = true;
    return j;
}

auto Json::fromString(const String& s) -> Json
{
    Json j;
    j.m_value = s;
    j.m_valid = true;
    return j;
}

auto Json::fromBool(bool v) -> Json
{
    Json j;
    j.m_value = v;
    j.m_valid = true;
    return j;
}

auto Json::fromInt(long long v) -> Json
{
    Json j;
    j.m_value = v;
    j.m_valid = true;
    return j;
}

auto Json::fromDouble(double v) -> Json
{
    Json j;
    j.m_value = v;
    j.m_valid = true;
    return j;
}

auto Json::parse(const Bytes& data) -> Json
{
    auto parsed = nlohmann::json::parse(data.begin(), data.end(), nullptr, false);
    if (parsed.is_discarded()) return {};
    Json j;
    j.m_value = std::move(parsed);
    j.m_valid = true;
    return j;
}

auto Json::parse(StringView text) -> Json
{
    Bytes bytes(text.begin(), text.end());
    return parse(bytes);
}

// ============================================================================
// Introspection
// ============================================================================
auto Json::isValid() const -> bool { return m_valid; }

auto Json::isNull() const -> bool
{
    return !m_valid || m_value.is_null();
}

auto Json::isObject() const -> bool
{
    return m_valid && m_value.is_object();
}

auto Json::isArray() const -> bool
{
    return m_valid && m_value.is_array();
}

auto Json::isString() const -> bool
{
    return m_valid && m_value.is_string();
}

auto Json::isBool() const -> bool
{
    return m_valid && m_value.is_boolean();
}

auto Json::isNumber() const -> bool
{
    return m_valid && m_value.is_number();
}

// ============================================================================
// Scalar extraction
// ============================================================================
auto Json::toString(StringView default_value) const -> String
{
    if (!m_valid) return std::string(default_value);
    if (m_value.is_string()) return m_value.get<std::string>();
    return std::string(default_value);
}

auto Json::toBool(bool default_value) const -> bool
{
    if (!m_valid) return default_value;
    if (m_value.is_boolean()) return m_value.get<bool>();
    return default_value;
}

auto Json::toInt(int default_value) const -> int
{
    if (!m_valid) return default_value;
    if (m_value.is_number_integer()) return m_value.get<int>();
    if (m_value.is_number_float())   return static_cast<int>(m_value.get<double>());
    return default_value;
}

auto Json::toLongLong(long long default_value) const -> long long
{
    if (!m_valid) return default_value;
    if (m_value.is_number_integer()) return m_value.get<long long>();
    if (m_value.is_number_float())   return static_cast<long long>(m_value.get<double>());
    return default_value;
}

auto Json::toDouble(double default_value) const -> double
{
    if (!m_valid) return default_value;
    if (m_value.is_number()) return m_value.get<double>();
    return default_value;
}

// ============================================================================
// Object access
// ============================================================================
auto Json::contains(const String& key) const -> bool
{
    if (!isObject()) return false;
    return m_value.contains(key);
}

auto Json::value(const String& key) const -> Json
{
    if (!isObject()) return {};
    auto it = m_value.find(key);
    if (it == m_value.end()) return {};
    Json j;
    j.m_value = *it;
    j.m_valid = true;
    return j;
}

auto Json::stringValue(const String& key, const String& default_value) const -> String
{
    return value(key).toString(default_value);
}

auto Json::boolValue(const String& key, bool default_value) const -> bool
{
    return value(key).toBool(default_value);
}

auto Json::intValue(const String& key, int default_value) const -> int
{
    return value(key).toInt(default_value);
}

auto Json::doubleValue(const String& key, double default_value) const -> double
{
    return value(key).toDouble(default_value);
}

auto Json::remove(const String& key) -> void
{
    if (!isObject()) return;
    m_value.erase(key);
}

auto Json::keys() const -> Vector<String>
{
    Vector<String> out;
    if (!isObject()) return out;
    out.reserve(m_value.size());
    for (auto it = m_value.begin(); it != m_value.end(); ++it)
        out.push_back(it.key());
    return out;
}

// ============================================================================
// Array access
// ============================================================================
auto Json::size() const -> std::size_t
{
    if (!m_valid) return 0;
    if (m_value.is_array() || m_value.is_object()) return m_value.size();
    return 0;
}

auto Json::at(std::size_t index) const -> Json
{
    if (!isArray()) return {};
    if (index >= m_value.size()) return {};
    Json j;
    j.m_value = m_value.at(index);
    j.m_valid = true;
    return j;
}

auto Json::items() const -> Vector<Json>
{
    Vector<Json> out;
    if (!isArray()) return out;
    out.reserve(m_value.size());
    for (const auto& v : m_value) {
        Json j;
        j.m_value = v;
        j.m_valid = true;
        out.push_back(j);
    }
    return out;
}

// ============================================================================
// Mutation
// ============================================================================
auto Json::set(const String& key, const Json& v) -> void
{
    if (!isObject()) *this = object();
    m_value[key] = v.m_value;
}

auto Json::setString(const String& key, const String& value) -> void
{
    set(key, Json::fromString(value));
}

auto Json::setBool(const String& key, bool value) -> void
{
    set(key, Json::fromBool(value));
}

auto Json::setInt(const String& key, long long value) -> void
{
    set(key, Json::fromInt(value));
}

auto Json::setDouble(const String& key, double value) -> void
{
    set(key, Json::fromDouble(value));
}

auto Json::append(const Json& v) -> void
{
    if (!isArray()) *this = array();
    m_value.push_back(v.m_value);
}

// ============================================================================
// Serialization
// ============================================================================
auto Json::dump(bool compact) const -> Bytes
{
    if (!m_valid) return {};
    const std::string s = compact ? m_value.dump() : m_value.dump(2);
    return Bytes(s.begin(), s.end());
}

auto Json::dumpString(bool compact) const -> String
{
    if (!m_valid) return {};
    return compact ? m_value.dump() : m_value.dump(2);
}

}  // namespace rb
