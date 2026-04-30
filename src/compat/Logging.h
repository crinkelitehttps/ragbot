#ifndef RAGBOT_COMPAT_LOGGING_H
#define RAGBOT_COMPAT_LOGGING_H

#include "Strings.h"
#include "Types.h"
#include <functional>
#include <string>

namespace rb {

enum class LogLevel { Info, Warn, Error };

using LogCallback = std::function<void(LogLevel level, const std::string& msg)>;

// Install a global log callback. Empty function = restore default (stderr).
// Not thread-safe; call before spawning any ragbot session.
auto install_log_callback(LogCallback callback) -> void;

auto log_message(LogLevel level, const String& msg) -> void;

inline auto log_info(const String& msg)  -> void { log_message(LogLevel::Info,  msg); }
inline auto log_warn(const String& msg)  -> void { log_message(LogLevel::Warn,  msg); }
inline auto log_error(const String& msg) -> void { log_message(LogLevel::Error, msg); }

}  // namespace rb

// Macros that take rb::format-style arguments: RAGBOT_LOG_INFO("got {} hits", n)
#define RAGBOT_LOG_INFO(...)  ::rb::log_info(::rb::format(__VA_ARGS__))
#define RAGBOT_LOG_WARN(...)  ::rb::log_warn(::rb::format(__VA_ARGS__))
#define RAGBOT_LOG_ERROR(...) ::rb::log_error(::rb::format(__VA_ARGS__))

#endif
