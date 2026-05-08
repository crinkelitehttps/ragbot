#include "Strings.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <string>

namespace rb {

// ============================================================================
// Predicates / case
// ============================================================================
auto starts_with(const String& s, const String& prefix) -> bool
{
    return s.size() >= prefix.size()
        && s.compare(0, prefix.size(), prefix) == 0;
}

auto ends_with(const String& s, const String& suffix) -> bool
{
    return s.size() >= suffix.size()
        && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

auto contains(const String& s, const String& needle) -> bool
{
    return s.find(needle) != std::string::npos;
}

auto to_lower(const String& s) -> String
{
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return out;
}

// ============================================================================
// Whitespace
// ============================================================================
auto trim(const String& s) -> String
{
    auto is_ws = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
    };
    auto begin = s.begin();
    while (begin != s.end() && is_ws(*begin)) ++begin;
    auto end = s.end();
    while (end != begin && is_ws(static_cast<unsigned char>(*(end - 1)))) --end;
    return std::string(begin, end);
}

auto squash_whitespace(const String& s) -> String
{
    std::string out;
    out.reserve(s.size());
    bool in_ws = false;
    bool started = false;
    auto is_ws = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
    };
    for (unsigned char c : s) {
        if (is_ws(c)) {
            in_ws = true;
        } else {
            if (in_ws && started) out.push_back(' ');
            out.push_back(static_cast<char>(c));
            started = true;
            in_ws = false;
        }
    }
    return out;
}

// ============================================================================
// Splitting
// ============================================================================
auto split(const String& s, char sep, bool keepEmpty) -> Vector<String>
{
    Vector<String> out;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == sep) {
            std::string piece = s.substr(start, i - start);
            if (keepEmpty || !piece.empty()) out.push_back(std::move(piece));
            start = i + 1;
        }
    }
    return out;
}

// ============================================================================
// Numeric parse
// ============================================================================
auto parse_int(const String& s, int default_value) -> int
{
    try {
        size_t end = 0;
        const int v = std::stoi(s, &end);
        return v;
    } catch (...) {
        return default_value;
    }
}

auto parse_double(const String& s, double default_value) -> double
{
    try {
        return std::stod(s);
    } catch (...) {
        return default_value;
    }
}

// ============================================================================
// "{}"-style format
// ============================================================================
namespace detail {

auto to_arg(int v) -> std::string                 { return std::to_string(v); }
auto to_arg(unsigned v) -> std::string            { return std::to_string(v); }
auto to_arg(long v) -> std::string                { return std::to_string(v); }
auto to_arg(unsigned long v) -> std::string       { return std::to_string(v); }
auto to_arg(long long v) -> std::string           { return std::to_string(v); }
auto to_arg(unsigned long long v) -> std::string  { return std::to_string(v); }
auto to_arg(double v) -> std::string {
    std::ostringstream ss; ss << v; return ss.str();
}
auto to_arg(float v) -> std::string {
    std::ostringstream ss; ss << v; return ss.str();
}

auto format_apply(const std::string& fmt, const std::vector<std::string>& args)
    -> std::string
{
    std::string out;
    out.reserve(fmt.size() + 16 * args.size());
    size_t arg_idx = 0;
    for (size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] == '{' && i + 1 < fmt.size() && fmt[i + 1] == '}') {
            if (arg_idx < args.size()) {
                out += args[arg_idx++];
            }
            ++i;
        } else if (fmt[i] == '{' && i + 1 < fmt.size() && fmt[i + 1] == '{') {
            out.push_back('{');
            ++i;
        } else if (fmt[i] == '}' && i + 1 < fmt.size() && fmt[i + 1] == '}') {
            out.push_back('}');
            ++i;
        } else {
            out.push_back(fmt[i]);
        }
    }
    return out;
}

}  // namespace detail

auto format_fixed(double value, int precision) -> String
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", precision, value);
    return std::string(buf);
}

auto replace_placeholders(const String& tmpl, const Vector<String>& values) -> String
{
    std::string out;
    out.reserve(tmpl.size() + 64);
    for (size_t i = 0; i < tmpl.size(); ++i) {
        if (tmpl[i] == '%' && i + 1 < tmpl.size() && std::isdigit(static_cast<unsigned char>(tmpl[i + 1]))) {
            size_t j = i + 1;
            while (j < tmpl.size() && std::isdigit(static_cast<unsigned char>(tmpl[j]))) ++j;
            const int idx = std::stoi(tmpl.substr(i + 1, j - (i + 1)));
            if (idx >= 1 && idx <= static_cast<int>(values.size())) {
                out += values[idx - 1];
            } else {
                out.append(tmpl, i, j - i);
            }
            i = j - 1;
        } else {
            out.push_back(tmpl[i]);
        }
    }
    return out;
}

}  // namespace rb
