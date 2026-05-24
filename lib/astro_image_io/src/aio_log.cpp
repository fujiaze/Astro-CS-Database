#include "aio_log.h"
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <mutex>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

static std::mutex g_aio_log_mutex;
static FILE* g_aio_log_file = nullptr;
static int g_aio_log_level = -1;

static int aio_get_log_level() {
    if (g_aio_log_level >= 0) return g_aio_log_level;
    const char* env = std::getenv("AIO_LOG_LEVEL");
    if (env) {
        int v = std::atoi(env);
        g_aio_log_level = (v >= 0 && v <= 3) ? v : AIO_LOG_INFO;
    } else {
        g_aio_log_level = AIO_LOG_INFO;
    }
    return g_aio_log_level;
}

static void aio_ensure_log_file() {
    if (g_aio_log_file) return;
    {
        const char* dir = "lib\\astro_image_io\\logs";
        CreateDirectoryA(dir, nullptr);
    }
    g_aio_log_file = std::fopen("lib\\astro_image_io\\logs\\astro_image_io.log", "a");
}

static const char* aio_level_name(int level) {
    switch (level) {
        case AIO_LOG_INFO:  return "INFO";
        case AIO_LOG_DEBUG: return "DEBUG";
        case AIO_LOG_WARN:  return "WARN";
        case AIO_LOG_ERROR: return "ERROR";
        default:            return "UNKNOWN";
    }
}

void aio_log(int level, const char* module, const char* fmt, ...) {
    if (level < aio_get_log_level()) return;

    std::lock_guard<std::mutex> lock(g_aio_log_mutex);

    std::time_t now = std::time(nullptr);
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &now);
#else
    localtime_r(&now, &tm_buf);
#endif
    char time_str[32];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_buf);

    char msg[2048];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    char line[2304];
    std::snprintf(line, sizeof(line), "[%s][%s][%s] %s\n",
                  time_str, aio_level_name(level), module ? module : "", msg);

    std::fprintf(stderr, "%s", line);
    std::fflush(stderr);

    aio_ensure_log_file();
    if (g_aio_log_file) {
        std::fprintf(g_aio_log_file, "%s", line);
        std::fflush(g_aio_log_file);
    }
}
