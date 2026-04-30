#ifndef RAGBOT_COMPAT_STRINGS_H
#define RAGBOT_COMPAT_STRINGS_H

#include "Types.h"
#include <string>
#include <string_view>
#include <utility>

namespace rb {

// ---------- conversions to std::string for things that aren't already ----------
// In non-Qt mode rb::String is already std::string; these are no-ops.
auto to_std(StringView s) -> std::string;
auto from_std(std::string_view s) -> String;

// ---------- predicates / case ----------
auto starts_with(const String& s, const String& prefix) -> bool;
auto ends_with(const String& s, const String& suffix) -> bool;
auto contains(const String& s, const String& needle) -> bool;
auto to_lower(const String& s) -> String;

// ---------- whitespace ----------
auto trim(const String& s) -> String;
auto squash_whitespace(const String& s) -> String;  // QString::simplified analogue

// ---------- emptiness check (portable across Qt/std) ----------
inline auto str_empty(const String& s) -> bool
{
#ifdef RAGBOT_USE_QT
    return s.isEmpty();
#else
    return s.empty();
#endif
}

// ---------- splitting ----------
auto split(const String& s, char sep, bool keepEmpty = true) -> Vector<String>;

// ---------- numeric parse ----------
auto parse_int(const String& s, int default_value = 0) -> int;
auto parse_double(const String& s, double default_value = 0.0) -> double;

// ---------- minimal "{}"-style format ----------
// Supports plain "{}" placeholders only. For fixed-precision doubles use
// format_fixed.
namespace detail {
    auto format_apply(const std::string& fmt, const std::vector<std::string>& args)
        -> std::string;
    inline auto to_arg(const String& s) -> std::string { return to_std(s); }
    inline auto to_arg(StringView s) -> std::string { return to_std(s); }
#ifdef RAGBOT_USE_QT
    // In Qt mode String != std::string, so these overloads are needed.
    inline auto to_arg(const std::string& s) -> std::string { return s; }
    inline auto to_arg(std::string_view s) -> std::string { return std::string(s); }
#endif
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
    return from_std(detail::format_apply(std::string(fmt), packed));
}

// Fixed-precision double formatting (replaces QString::arg(d, 0, 'f', n)).
auto format_fixed(double value, int precision) -> String;

// Replace %1, %2, %3, ... in a runtime template (replaces QString::arg chains
// against a roleplayPrompt.txt-style template).
auto replace_placeholders(const String& tmpl, const Vector<String>& values) -> String;

}  // namespace rb

#endif
