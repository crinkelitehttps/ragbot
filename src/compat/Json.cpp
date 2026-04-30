#include "Json.h"
#include "Strings.h"

#include <utility>

#ifdef RAGBOT_USE_QT
#include <QJsonParseError>
#endif

namespace rb {

// ============================================================================
// Construction
// ============================================================================
Json::Json() : m_valid(false) {}

auto Json::object() -> Json
{
    Json j;
#ifdef RAGBOT_USE_QT
    j.m_value = QJsonValue(QJsonObject{});
#else
    j.m_value = nlohmann::json::object();
#endif
    j.m_valid = true;
    return j;
}

auto Json::array() -> Json
{
    Json j;
#ifdef RAGBOT_USE_QT
    j.m_value = QJsonValue(QJsonArray{});
#else
    j.m_value = nlohmann::json::array();
#endif
    j.m_valid = true;
    return j;
}

auto Json::fromString(const String& s) -> Json
{
    Json j;
#ifdef RAGBOT_USE_QT
    j.m_value = QJsonValue(s);
#else
    j.m_value = s;
#endif
    j.m_valid = true;
    return j;
}

auto Json::fromBool(bool v) -> Json
{
    Json j;
#ifdef RAGBOT_USE_QT
    j.m_value = QJsonValue(v);
#else
    j.m_value = v;
#endif
    j.m_valid = true;
    return j;
}

auto Json::fromInt(long long v) -> Json
{
    Json j;
#ifdef RAGBOT_USE_QT
    j.m_value = QJsonValue(static_cast<qint64>(v));
#else
    j.m_value = v;
#endif
    j.m_valid = true;
    return j;
}

auto Json::fromDouble(double v) -> Json
{
    Json j;
#ifdef RAGBOT_USE_QT
    j.m_value = QJsonValue(v);
#else
    j.m_value = v;
#endif
    j.m_valid = true;
    return j;
}

auto Json::parse(const Bytes& data) -> Json
{
#ifdef RAGBOT_USE_QT
    QJsonParseError err {};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || doc.isNull()) return {};
    Json j;
    if (doc.isObject()) j.m_value = QJsonValue(doc.object());
    else if (doc.isArray()) j.m_value = QJsonValue(doc.array());
    else return {};
    j.m_valid = true;
    return j;
#else
    auto parsed = nlohmann::json::parse(data.begin(), data.end(), nullptr, false);
    if (parsed.is_discarded()) return {};
    Json j;
    j.m_value = std::move(parsed);
    j.m_valid = true;
    return j;
#endif
}

auto Json::parse(StringView text) -> Json
{
#ifdef RAGBOT_USE_QT
    return parse(text.toUtf8());
#else
    Bytes bytes(text.begin(), text.end());
    return parse(bytes);
#endif
}

#ifdef RAGBOT_USE_QT
auto Json::parse(std::string_view utf8) -> Json
{
    return parse(QByteArray(utf8.data(), static_cast<int>(utf8.size())));
}
#endif

// ============================================================================
// Introspection
// ============================================================================
auto Json::isValid() const -> bool { return m_valid; }

auto Json::isNull() const -> bool
{
#ifdef RAGBOT_USE_QT
    return !m_valid || m_value.isNull() || m_value.isUndefined();
#else
    return !m_valid || m_value.is_null();
#endif
}

auto Json::isObject() const -> bool
{
#ifdef RAGBOT_USE_QT
    return m_valid && m_value.isObject();
#else
    return m_valid && m_value.is_object();
#endif
}

auto Json::isArray() const -> bool
{
#ifdef RAGBOT_USE_QT
    return m_valid && m_value.isArray();
#else
    return m_valid && m_value.is_array();
#endif
}

auto Json::isString() const -> bool
{
#ifdef RAGBOT_USE_QT
    return m_valid && m_value.isString();
#else
    return m_valid && m_value.is_string();
#endif
}

auto Json::isBool() const -> bool
{
#ifdef RAGBOT_USE_QT
    return m_valid && m_value.isBool();
#else
    return m_valid && m_value.is_boolean();
#endif
}

auto Json::isNumber() const -> bool
{
#ifdef RAGBOT_USE_QT
    return m_valid && m_value.isDouble();
#else
    return m_valid && m_value.is_number();
#endif
}

// ============================================================================
// Scalar extraction
// ============================================================================
auto Json::toString(StringView default_value) const -> String
{
    if (!m_valid) return from_std(to_std(default_value));
#ifdef RAGBOT_USE_QT
    return m_value.toString(default_value.toString());
#else
    if (m_value.is_string()) return m_value.get<std::string>();
    return std::string(default_value);
#endif
}

auto Json::toBool(bool default_value) const -> bool
{
    if (!m_valid) return default_value;
#ifdef RAGBOT_USE_QT
    return m_value.toBool(default_value);
#else
    if (m_value.is_boolean()) return m_value.get<bool>();
    return default_value;
#endif
}

auto Json::toInt(int default_value) const -> int
{
    if (!m_valid) return default_value;
#ifdef RAGBOT_USE_QT
    return m_value.toInt(default_value);
#else
    if (m_value.is_number_integer()) return m_value.get<int>();
    if (m_value.is_number_float())   return static_cast<int>(m_value.get<double>());
    return default_value;
#endif
}

auto Json::toLongLong(long long default_value) const -> long long
{
    if (!m_valid) return default_value;
#ifdef RAGBOT_USE_QT
    return static_cast<long long>(m_value.toDouble(static_cast<double>(default_value)));
#else
    if (m_value.is_number_integer()) return m_value.get<long long>();
    if (m_value.is_number_float())   return static_cast<long long>(m_value.get<double>());
    return default_value;
#endif
}

auto Json::toDouble(double default_value) const -> double
{
    if (!m_valid) return default_value;
#ifdef RAGBOT_USE_QT
    return m_value.toDouble(default_value);
#else
    if (m_value.is_number()) return m_value.get<double>();
    return default_value;
#endif
}

// ============================================================================
// Object access
// ============================================================================
auto Json::contains(const String& key) const -> bool
{
    if (!isObject()) return false;
#ifdef RAGBOT_USE_QT
    return m_value.toObject().contains(key);
#else
    return m_value.contains(key);
#endif
}

auto Json::value(const String& key) const -> Json
{
    if (!isObject()) return {};
#ifdef RAGBOT_USE_QT
    const QJsonValue v = m_value.toObject().value(key);
    if (v.isUndefined()) return {};
    Json j;
    j.m_value = v;
    j.m_valid = true;
    return j;
#else
    auto it = m_value.find(key);
    if (it == m_value.end()) return {};
    Json j;
    j.m_value = *it;
    j.m_valid = true;
    return j;
#endif
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
#ifdef RAGBOT_USE_QT
    QJsonObject obj = m_value.toObject();
    obj.remove(key);
    m_value = QJsonValue(obj);
#else
    m_value.erase(key);
#endif
}

auto Json::keys() const -> Vector<String>
{
    Vector<String> out;
    if (!isObject()) return out;
#ifdef RAGBOT_USE_QT
    const QJsonObject obj = m_value.toObject();
    const QStringList qkeys = obj.keys();
    out.reserve(qkeys.size());
    for (const auto& k : qkeys) out.push_back(k);
#else
    out.reserve(m_value.size());
    for (auto it = m_value.begin(); it != m_value.end(); ++it) {
        out.push_back(it.key());
    }
#endif
    return out;
}

// ============================================================================
// Array access
// ============================================================================
auto Json::size() const -> std::size_t
{
    if (!m_valid) return 0;
#ifdef RAGBOT_USE_QT
    if (m_value.isArray())  return static_cast<std::size_t>(m_value.toArray().size());
    if (m_value.isObject()) return static_cast<std::size_t>(m_value.toObject().size());
    return 0;
#else
    if (m_value.is_array() || m_value.is_object()) return m_value.size();
    return 0;
#endif
}

auto Json::at(std::size_t index) const -> Json
{
    if (!isArray()) return {};
#ifdef RAGBOT_USE_QT
    const QJsonArray arr = m_value.toArray();
    if (index >= static_cast<std::size_t>(arr.size())) return {};
    Json j;
    j.m_value = arr.at(static_cast<int>(index));
    j.m_valid = true;
    return j;
#else
    if (index >= m_value.size()) return {};
    Json j;
    j.m_value = m_value.at(index);
    j.m_valid = true;
    return j;
#endif
}

auto Json::items() const -> Vector<Json>
{
    Vector<Json> out;
    if (!isArray()) return out;
#ifdef RAGBOT_USE_QT
    const QJsonArray arr = m_value.toArray();
    out.reserve(arr.size());
    for (const auto& v : arr) {
        Json j;
        j.m_value = v;
        j.m_valid = true;
        out.push_back(j);
    }
#else
    out.reserve(m_value.size());
    for (const auto& v : m_value) {
        Json j;
        j.m_value = v;
        j.m_valid = true;
        out.push_back(j);
    }
#endif
    return out;
}

// ============================================================================
// Mutation
// ============================================================================
auto Json::set(const String& key, const Json& v) -> void
{
    if (!isObject()) *this = object();
#ifdef RAGBOT_USE_QT
    QJsonObject obj = m_value.toObject();
    obj.insert(key, v.m_value);
    m_value = QJsonValue(obj);
#else
    m_value[key] = v.m_value;
#endif
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
#ifdef RAGBOT_USE_QT
    QJsonArray arr = m_value.toArray();
    arr.append(v.m_value);
    m_value = QJsonValue(arr);
#else
    m_value.push_back(v.m_value);
#endif
}

// ============================================================================
// Serialization
// ============================================================================
auto Json::dump(bool compact) const -> Bytes
{
    if (!m_valid) return {};
#ifdef RAGBOT_USE_QT
    QJsonDocument doc;
    if (m_value.isObject())      doc = QJsonDocument(m_value.toObject());
    else if (m_value.isArray())  doc = QJsonDocument(m_value.toArray());
    else                         return {};
    return doc.toJson(compact ? QJsonDocument::Compact : QJsonDocument::Indented);
#else
    const std::string s = compact ? m_value.dump() : m_value.dump(2);
    return Bytes(s.begin(), s.end());
#endif
}

auto Json::dumpString(bool compact) const -> String
{
    const Bytes b = dump(compact);
#ifdef RAGBOT_USE_QT
    return QString::fromUtf8(b);
#else
    return std::string(b.begin(), b.end());
#endif
}

}  // namespace rb
