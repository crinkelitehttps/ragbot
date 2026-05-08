#ifndef RAGBOT_COMPAT_STRINGS_H
#define RAGBOT_COMPAT_STRINGS_H

#include "Types.h"
#include <string>
#include <string_view>
#include <utility>

namespace rb {

// In non-Qt mode rb::String is std::string; these conversions are identity ops
// kept for call-site compatibility.
inline auto to_std(StringView s) -> std::string   { return std::string(s); }
inline auto from_std(std::string_view s) -> String { return std::string(s); }

// ---------- predicates / case ----------
auto starts_with(const String& s, const String& prefix) -> bool;
auto ends_with(const String& s, const String& suffix) -> bool;
auto contains(const String& s, const String& needle) -> bool;
auto to_lower(const String& s) -> String;

// ---------- whitespace ----------
auto trim(const String& s) -> String;
auto squash_whitespace(const String& s) -> String;

// ---------- emptiness check ----------
inline auto str_empty(const String& s) -> bool { return s.empty(); }

// ---------- splitting ----------
auto split(const String& s, char sep, bool keepEmpty = true) -> Vector<String>;

// ---------- numeric parse ----------
auto parse_int(const String& s, int default_value = 0) -> int;
auto parse_double(const String& s, double default_value = 0.0) -> double;

// ---------- minimal "{}"-style format ----------
namespace detail {
    auto format_apply(const std::string& fmt, const std::vector<std::string>& args)
        -> std::string;
    inline auto to_arg(const String& s) -> std::string { return s; }
    inline auto to_arg(StringView s) -> std::string { return std::string(s); }
    inline auto to_arg(const char* s) -> std::string { return s ? s : ""; }
    inline auto to_arg(bool v) -> std::string { return v ? "true" : "false"; }
    auto to_arg(int v) -> std::string;
    auto to_arg(unsigned v) -> std::string;
    auto to_arg(long v) -> std::string;
    auto to_arg(unsigned long v) -> std::string;
    auto to_arg(long long v) -> std::string;
    auto to_arg(unsigned long long v) -> std::string;
    auto to_arg(double v) -> std::string;
    auto to_arg(float v) -> std::string;
}

template <class... Args>
auto format(std::string_view fmt, Args&&... args) -> String
{
    std::vector<std::string> packed { detail::to_arg(std::forward<Args>(args))... };
    return detail::format_apply(std::string(fmt), packed);
}

// Fixed-precision double formatting.
auto format_fixed(double value, int precision) -> String;

// Replace %1, %2, %3, ... in a runtime template.
auto replace_placeholders(const String& tmpl, const Vector<String>& values) -> String;

}  // namespace rb

#endif
