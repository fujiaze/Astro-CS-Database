#include "sdet_log.h"
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <mutex>
#include <string>

// CLEAN-403 (ASTROCS_DESIGN §10「aio 是文件级唯一 I/O 边界」): 日志落盘经 aio
// 唯一实现 (aio_atomic::make_dirs + append_open/append_write), 本 TU 不自持
// std::filesystem / FILE* 通道。
#include "aio_atomic_file.h"

static std::mutex g_sdet_log_mutex;
static aio_atomic::AppendSink* g_sdet_log_file = nullptr;
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
    (void)aio_atomic::make_dirs(
#ifdef _WIN32
        "lib\\star_detector\\logs"
#else
        "lib/algorithms/star_detection/logs"
#endif
    );
    g_sdet_log_file = aio_atomic::append_open(
#ifdef _WIN32
        "lib\\star_detector\\logs\\star_detector.log",
#else
        "lib/algorithms/star_detection/logs/star_detector.log",
#endif
        nullptr);
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
                  time_str, sdet_level_name(level), module ? module : "", msg);

    std::fprintf(stderr, "%s", line);
    std::fflush(stderr);

    sdet_ensure_log_file();
    if (g_sdet_log_file) {
        (void)aio_atomic::append_write_str(g_sdet_log_file, line);
    }
}
