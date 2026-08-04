#include "fsic/log.h"

#include <cstdarg>
#include <cstdio>

namespace fsic {

static LogLevel g_level = LogLevel::Info;

void log_set_level(LogLevel level) { g_level = level; }

void log_msg(LogLevel level, const char* fmt, ...) {
    if (static_cast<int>(level) > static_cast<int>(g_level)) return;
    const char* prefix = nullptr;
    switch (level) {
        case LogLevel::Error: prefix = "[ERROR] "; break;
        case LogLevel::Warn:  prefix = "[WARN] ";  break;
        case LogLevel::Info:  prefix = "[INFO] ";  break;
        case LogLevel::Debug: prefix = "[DEBUG] "; break;
    }
    if (prefix) std::fputs(prefix, stderr);
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
    std::fputc('\n', stderr);
}

} // namespace fsic
