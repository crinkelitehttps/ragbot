#include "Logging.h"

#include <cstdio>

namespace rb {

static LogCallback g_log_callback;

auto install_log_callback(LogCallback callback) -> void
{
    g_log_callback = std::move(callback);
}

auto log_message(LogLevel level, const String& msg) -> void
{
    if (g_log_callback) {
        g_log_callback(level, msg);
        return;
    }
    const char* tag = level == LogLevel::Error ? "ERROR"
                    : level == LogLevel::Warn  ? "WARN" : "INFO";
    std::fprintf(stderr, "[%s] %s\n", tag, msg.c_str());
}

}  // namespace rb
