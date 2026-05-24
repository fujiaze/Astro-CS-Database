#include "sdet_log.h"
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <mutex>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

static std::mutex g_sdet_log_mutex;
static FILE* g_sdet_log_file = nullptr;
static int g_sdet_log_level = -1;

static int sdet_get_log_level() {
    if (g_sdet_log_level >= 0) return g_sdet_log_level;
    const char* env = std::getenv("STAR_DETECTOR_LOG_LEVEL");
    if (env) {
        int v = std::atoi(env);
        g_sdet_log_level = (v >= 0 && v <= 3) ? v : SDET_LOG_INFO;
    } else {
        g_sdet_log_level = SDET_LOG_INFO;
    }
    return g_sdet_log_level;
}

static void sdet_ensure_log_file() {
    if (g_sdet_log_file) return;
    {
        const char* dir = "lib\\star_detector\\logs";
        CreateDirectoryA(dir, nullptr);
    }
    g_sdet_log_file = std::fopen("lib\\star_detector\\logs\\star_detector.log", "a");
}

static const char* sdet_level_name(int level) {
    switch (level) {
        case SDET_LOG_INFO:  return "INFO";
        case SDET_LOG_DEBUG: return "DEBUG";
        case SDET_LOG_WARN:  return "WARN";
        case SDET_LOG_ERROR: return "ERROR";
        default:             return "UNKNOWN";
    }
}

void sdet_log(int level, const char* module, const char* fmt, ...) {
    if (level < sdet_get_log_level()) return;

    std::lock_guard<std::mutex> lock(g_sdet_log_mutex);

    std::time_t now = std::time(nullptr);
    std::tm tm_buf;
    localtime_s(&tm_buf, &now);
    char time_str[32];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_buf);

    char msg[2048];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    char line[2304];
    std::snprintf(line, sizeof(line), "[%s][%s][%s] %s\n",
                  time_str, sdet_level_name(level), module ? module : "", msg);

    std::fprintf(stderr, "%s", line);
    std::fflush(stderr);

    sdet_ensure_log_file();
    if (g_sdet_log_file) {
        std::fprintf(g_sdet_log_file, "%s", line);
        std::fflush(g_sdet_log_file);
    }
}
