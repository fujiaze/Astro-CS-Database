#ifndef IPV_LOG_H
#define IPV_LOG_H

// ============================================================================
// ipv_log.h - IPV 统一日志接口
//
// 每个模块独立日志文件
// 日志目录结构:
//   lib/plate_solve/logs/ipv/<frame>/
//   ├── phase_0_star_selector.log
//   ├── phase_polygon_matcher.log
//   ├── phase_prosac.log
//   └── wcs_final.json             (最终 WCS)
// ============================================================================

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <sstream>

namespace ipv {

class Logger {
public:
    enum Level { INFO, WARN, ERROR, DEBUG };

    Logger() : enabled_(false) {}
    ~Logger() { close(); }

    // 初始化日志文件
    void init(const std::string& path) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (ofs_.is_open()) {
            ofs_.close();
        }
        ofs_.open(path, std::ios::binary);
        if (ofs_.is_open()) {
            // UTF-8 BOM
            const char bom[] = {(char)0xEF, (char)0xBB, (char)0xBF};
            ofs_.write(bom, 3);
            ofs_ << "=== IPV Plate Solve Log ===" << std::endl;
            enabled_ = true;
        }
    }

    void close() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (ofs_.is_open()) {
            ofs_ << "=== Log End ===" << std::endl;
            ofs_.close();
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
        if (enabled_ && ofs_.is_open()) {
            ofs_ << line;
            ofs_.flush();
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
    std::ofstream ofs_;
    std::mutex    mtx_;
    bool          enabled_;
};

} // namespace ipv

#endif // IPV_LOG_H
