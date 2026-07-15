#ifndef BROWSER_LOGGER_H
#define BROWSER_LOGGER_H

// 日志系统: 内存缓冲 + 程序结束时一次性写出 (避免每次 fopen/fclose 的 I/O 阻塞)
// LOG_DEBUG 默认关闭 (编译时宏 BROWSER_LOG_DEBUG 控制, 定义后启用 DEBUG 日志)

#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <cstdlib>
#include <string>
#include <vector>
#include <mutex>

namespace browser_log {

// 内存日志缓冲 (线程安全)
inline std::vector<std::string>& log_buffer() {
    static std::vector<std::string> buffer;
    return buffer;
}

inline std::mutex& log_mutex() {
    static std::mutex mtx;
    return mtx;
}

// 缓冲区上限 (防止内存无限增长)
inline constexpr size_t MAX_LOG_BUFFER = 50000;

// 写入日志到内存缓冲
inline void log(const char* level, const char* fmt, ...) {
    time_t now = time(nullptr);
    char tbuf[32];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", localtime(&now));

    // 格式化消息
    char msg[1024];
    int header_len = snprintf(msg, sizeof(msg), "[%s][%s] ", tbuf, level);
    if (header_len < 0) return;

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg + header_len, sizeof(msg) - header_len, fmt, args);
    va_end(args);

    // 输出到 stderr (若有控制台, 无文件 I/O 开销)
    fprintf(stderr, "%s\n", msg);

    // 写入内存缓冲 (线程安全, 有上限保护)
    {
        std::lock_guard<std::mutex> lock(log_mutex());
        auto& buf = log_buffer();
        if (buf.size() < MAX_LOG_BUFFER) {
            buf.push_back(msg);
        }
    }
}

// 程序结束时将内存缓冲写入文件 (一次性 I/O, 避免运行时 fopen/fclose 开销)
inline void flush_to_file(const char* path) {
    std::lock_guard<std::mutex> lock(log_mutex());
    auto& buf = log_buffer();
    FILE* fp = std::fopen(path, "w");
    if (!fp) return;
    for (const auto& line : buf) {
        fprintf(fp, "%s\n", line.c_str());
    }
    std::fclose(fp);
    buf.clear();
}

}  // namespace browser_log

#define LOG_INFO(fmt, ...)  browser_log::log("INFO",  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  browser_log::log("WARN",  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) browser_log::log("ERROR", fmt, ##__VA_ARGS__)

// LOG_DEBUG 默认关闭 (编译时控制, 不定义 BROWSER_LOG_DEBUG 则为空操作)
#ifdef BROWSER_LOG_DEBUG
    #define LOG_DEBUG(fmt, ...) browser_log::log("DEBUG", fmt, ##__VA_ARGS__)
#else
    #define LOG_DEBUG(fmt, ...) ((void)0)
#endif

#endif
