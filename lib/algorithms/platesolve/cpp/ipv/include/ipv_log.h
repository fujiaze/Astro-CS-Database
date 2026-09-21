#ifndef IPV_LOG_H
#define IPV_LOG_H

// ============================================================================
// ipv_log.h - IPV 统一日志接口
//
// 每个模块独立日志文件
// 日志目录结构:
// lib/algorithms/platesolve/logs/ipv/<frame>/
// ├── phase_0_star_selector.log
// ├── phase_polygon_matcher.log
// ├── phase_prosac.log
// └── wcs_final.json (最终 WCS)
// ============================================================================

#include <string>
#include <mutex>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <sstream>

// CLEAN-403 (ASTROCS_DESIGN §10「aio 是文件级唯一 I/O 边界」): 日志落盘经 aio
// 唯一实现 (aio_atomic::append_open/append_write/append_flush); 本头不再持有
// std::ofstream 通道。
#include "aio_atomic_file.h"

namespace ipv {

class Logger {
public:
    enum Level { INFO, WARN, ERROR, DEBUG };

    Logger() : enabled_(false) {}
    ~Logger() { close(); }

    // 初始化日志文件
    void init(const std::string& path) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (sink_) {
            aio_atomic::append_close(sink_);
            sink_ = nullptr;
        }
        sink_ = aio_atomic::append_open(path, nullptr);
        if (sink_) {
            // UTF-8 BOM
            const char bom[] = {(char)0xEF, (char)0xBB, (char)0xBF};
            (void)aio_atomic::append_write(sink_, bom, 3);
            (void)aio_atomic::append_write_str(
                sink_, "=== IPV Plate Solve Log ===\n");
            (void)aio_atomic::append_flush(sink_);
            enabled_ = true;
        }
    }

    void close() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (sink_) {
            (void)aio_atomic::append_write_str(sink_, "=== Log End ===\n");
            aio_atomic::append_close(sink_);
            sink_ = nullptr;
        }
        enabled_ = false;
    }

    void log(Level lvl, const std::string& msg) {
        std::lock_guard<std::mutex> lock(mtx_);
        std::string level_str;
        switch (lvl) {
            case INFO:  level_str = "INFO";  break;
            case WARN:  level_str = "WARN";  break;
            case ERROR: level_str = "ERROR"; break;
            case DEBUG: level_str = "DEBUG"; break;
        }
        // 时间戳
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &t);
#else
        localtime_r(&t, &tm_buf);
#endif
        char time_str[32];
        std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_buf);

        std::string line = std::string("[") + time_str + "][" + level_str + "] " + msg + "\n";
        if (enabled_ && sink_) {
            (void)aio_atomic::append_write_str(sink_, line);
            (void)aio_atomic::append_flush(sink_);
        }
        // 同时输出到 stderr
        std::fprintf(stderr, "%s", line.c_str());
    }

    void info(const std::string& msg)  { log(INFO, msg); }
    void warn(const std::string& msg)  { log(WARN, msg); }
    void error(const std::string& msg) { log(ERROR, msg); }
    void debug(const std::string& msg) { log(DEBUG, msg); }

    // 便利方法: 格式化日志
    template<typename... Args>
    void infof(const char* fmt, Args... args) {
        char buf[1024];
        std::snprintf(buf, sizeof(buf), fmt, args...);
        info(std::string(buf));
    }

    template<typename... Args>
    void warnf(const char* fmt, Args... args) {
        char buf[1024];
        std::snprintf(buf, sizeof(buf), fmt, args...);
        warn(std::string(buf));
    }

    template<typename... Args>
    void errorf(const char* fmt, Args... args) {
        char buf[1024];
        std::snprintf(buf, sizeof(buf), fmt, args...);
        error(std::string(buf));
    }

    template<typename... Args>
    void debugf(const char* fmt, Args... args) {
        char buf[1024];
        std::snprintf(buf, sizeof(buf), fmt, args...);
        debug(std::string(buf));
    }

private:
    aio_atomic::AppendSink* sink_ = nullptr;
    std::mutex    mtx_;
    bool          enabled_;
};

} // namespace ipv

#endif // IPV_LOG_H
