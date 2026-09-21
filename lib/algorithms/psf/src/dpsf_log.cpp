#include "dpsf_log.h"
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <mutex>
#include <string>

// CLEAN-403 (ASTROCS_DESIGN §10「aio 是文件级唯一 I/O 边界」): 日志落盘经 aio
// 唯一实现 (aio_atomic::make_dirs + append_open/append_write), 本 TU 不自持
// std::filesystem / FILE* 通道。
#include "aio_atomic_file.h"

static std::mutex g_dpsf_log_mutex;
static aio_atomic::AppendSink* g_dpsf_log_file = nullptr;
static int g_dpsf_log_level = -1;

static int dpsf_get_log_level() {
    if (g_dpsf_log_level >= 0) return g_dpsf_log_level;
    const char* env = std::getenv("DYNAMIC_PSF_LOG_LEVEL");
    if (env) {
        int v = std::atoi(env);
        g_dpsf_log_level = (v >= 0 && v <= 3) ? v : LOG_INFO;
    } else {
        // 默认只输出WARN+ERROR，避免DEBUG日志导致性能问题
        g_dpsf_log_level = LOG_WARN;
    }
    return g_dpsf_log_level;
}

static void dpsf_ensure_log_file() {
    if (g_dpsf_log_file) return;
    (void)aio_atomic::make_dirs(
#ifdef _WIN32
        "lib\\dynamic_psf\\logs"
#else
        "lib/algorithms/psf/logs"
#endif
    );
    g_dpsf_log_file = aio_atomic::append_open(
#ifdef _WIN32
        "lib\\dynamic_psf\\logs\\dynamic_psf.log",
#else
        "lib/algorithms/psf/logs/dynamic_psf.log",
#endif
        nullptr);
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
                  time_str, dpsf_level_name(level), module ? module : "", msg);

    // 仅输出到stderr，不再强制fflush（性能优化：避免每条日志刷盘）
    std::fprintf(stderr, "%s", line);

    // WARN及以上级别才写文件（DEBUG/INFO不写文件，减少I/O）
    if (level >= LOG_WARN) {
        dpsf_ensure_log_file();
        if (g_dpsf_log_file) {
            (void)aio_atomic::append_write_str(g_dpsf_log_file, line);
            // 不再每条fflush，由操作系统缓冲
        }
    }
}
