#ifndef BROWSER_LOGGER_H
#define BROWSER_LOGGER_H

#include <cstdio>
#include <cstdarg>
#include <ctime>

namespace browser_log {

inline void log(const char* level, const char* fmt, ...) {
    time_t now = time(nullptr);
    char tbuf[32];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(stderr, "[%s][%s] ", tbuf, level);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    fflush(stderr);
}

}  // namespace browser_log

#define LOG_INFO(fmt, ...)  browser_log::log("INFO",  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  browser_log::log("WARN",  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) browser_log::log("ERROR", fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) browser_log::log("DEBUG", fmt, ##__VA_ARGS__)

#endif
