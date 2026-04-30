#include "Logging.h"

#ifdef RAGBOT_USE_QT
#include <QDebug>
#else
#include <cstdio>
#endif

namespace rb {

static LogCallback g_log_callback;

#ifdef RAGBOT_USE_QT
static void qt_log_handler(QtMsgType type, const QMessageLogContext& /*ctx*/, const QString& msg)
{
    LogLevel level = LogLevel::Info;
    if (type == QtWarningMsg)                    level = LogLevel::Warn;
    if (type == QtCriticalMsg || type == QtFatalMsg) level = LogLevel::Error;

    const std::string utf8 = msg.toUtf8().toStdString();
    if (g_log_callback) {
        g_log_callback(level, utf8);
        return;
    }
    const char* tag = "INFO";
    if (level == LogLevel::Error)     tag = "ERROR";
    else if (level == LogLevel::Warn) tag = "WARN";
    std::fprintf(stderr, "[%s] %s\n", tag, utf8.c_str());
}
#endif

auto install_log_callback(LogCallback callback) -> void
{
    g_log_callback = std::move(callback);
#ifdef RAGBOT_USE_QT
    if (g_log_callback)
        qInstallMessageHandler(qt_log_handler);
    else
        qInstallMessageHandler(nullptr);
#endif
}

auto log_message(LogLevel level, const String& msg) -> void
{
#ifdef RAGBOT_USE_QT
    // In Qt mode all routing goes through qDebug/qWarning/qCritical so that
    // qt_log_handler intercepts them when a callback is installed.
    switch (level) {
        case LogLevel::Info:  qDebug().noquote()    << msg; break;
        case LogLevel::Warn:  qWarning().noquote()  << msg; break;
        case LogLevel::Error: qCritical().noquote() << msg; break;
    }
#else
    const std::string& std_msg = msg;
    if (g_log_callback) {
        g_log_callback(level, std_msg);
        return;
    }
    const char* tag = level == LogLevel::Error ? "ERROR"
                    : level == LogLevel::Warn  ? "WARN" : "INFO";
    std::fprintf(stderr, "[%s] %s\n", tag, std_msg.c_str());
#endif
}

}  // namespace rb
