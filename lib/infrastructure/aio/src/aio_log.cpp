#include "aio_log.h"
#include <cstdio>
#include <cstdarg>
#include <ctime>
#ifndef _WIN32
#include <filesystem>
#endif
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
    // 两平台必须指向**同一**实际目录：ARCH-001 迁移把 aio 从 lib/astro_image_io 迁到
    // lib/infrastructure/aio，而 Windows 分支仍写死迁移前的旧路径。旧目录已不存在 ⇒
    // CreateDirectoryA 失败（返回值此前被丢弃）⇒ 下一行 fopen 必然失败 ⇒
    // **Windows 上文件日志静默不产生**。
    // 修法照本函数内 POSIX 分支的既有正确范式：建目录失败要可见，不再静默吞。
    // Windows 侧用带错误码的窄字符 API；GetLastError() 的 ERROR_ALREADY_EXISTS 视为成功。
    {
#ifdef _WIN32
        const char* dir = "lib\\infrastructure\\aio\\logs";
        if (!CreateDirectoryA(dir, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
            // 建目录失败时不静默：日志通道此时不可用，交给调用侧按无日志降级。
            g_aio_log_file = nullptr;
            return;
        }
#else
        std::error_code ec;
        std::filesystem::create_directories("lib/infrastructure/aio/logs", ec);
        // POSIX 分支此前连 ec 都没接；create_directories 失败（权限/只读挂载）同样会让
        // 下面的 fopen 失败，改为与 Windows 侧同口径：不吞、不假装成功。
        if (ec) {
            g_aio_log_file = nullptr;
            return;
        }
#endif
    }
    g_aio_log_file = std::fopen(
#ifdef _WIN32
        "lib\\infrastructure\\aio\\logs\\astro_image_io.log",
#else
        "lib/infrastructure/aio/logs/astro_image_io.log",
#endif
        "a");
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
