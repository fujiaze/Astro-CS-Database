#include "dpsf_log.h"
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <mutex>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

static std::mutex g_dpsf_log_mutex;
static FILE* g_dpsf_log_file = nullptr;
static int g_dpsf_log_level = -1;

static int dpsf_get_log_level() {
    if (g_dpsf_log_level >= 0) return g_dpsf_log_level;
    const char* env = std::getenv("DYNAMIC_PSF_LOG_LEVEL");
    if (env) {
        int v = std::atoi(env);
        g_dpsf_log_level = (v >= 0 && v <= 3) ? v : LOG_INFO;
    } else {
        g_dpsf_log_level = LOG_INFO;
    }
    return g_dpsf_log_level;
}

static void dpsf_ensure_log_file() {
    if (g_dpsf_log_file) return;
    {
        const char* dir = "lib\\dynamic_psf\\logs";
        CreateDirectoryA(dir, nullptr);
    }
    g_dpsf_log_file = std::fopen("lib\\dynamic_psf\\logs\\dynamic_psf.log", "a");
}

static const char* dpsf_level_name(int level) {
    switch (level) {
        case LOG_INFO:  return "INFO";
        case LOG_DEBUG: return "DEBUG";
        case LOG_WARN:  return "WARN";
        case LOG_ERROR: return "ERROR";
        default:        return "UNKNOWN";
    }
}

void dpsf_log(int level, const char* module, const char* fmt, ...) {
    if (level < dpsf_get_log_level()) return;

    std::lock_guard<std::mutex> lock(g_dpsf_log_mutex);

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
                  time_str, dpsf_level_name(level), module ? module : "", msg);

    std::fprintf(stderr, "%s", line);
    std::fflush(stderr);

    dpsf_ensure_log_file();
    if (g_dpsf_log_file) {
        std::fprintf(g_dpsf_log_file, "%s", line);
        std::fflush(g_dpsf_log_file);
    }
}
