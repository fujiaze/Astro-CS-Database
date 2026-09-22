#ifndef IPV_LOG_H
#define IPV_LOG_H

// ============================================================================
// ipv_log.h - IPV 统一日志接口（ipv 模块**公共头**）
//
// 每个模块独立日志文件
// 日志目录结构:
// lib/algorithms/platesolve/logs/ipv/<frame>/
// ├── phase_0_star_selector.log
// ├── phase_polygon_matcher.log
// ├── phase_prosac.log
// └── wcs_final.json (最终 WCS)
//
// 分层纪律（ASTROCS_DESIGN §8.4「lib/include/ 公共头」、§8.5「版本化公开头」）:
//   本头是公共 include 面（ipv/include/）的一员——ipv_solver.h / ipv_select.h /
//   ipv_sip.h / ipv_distortion.h / ipv_robust_refine.h 均包含它，且 Logger*
//   出现在这些头的公共签名里。公共头**只允许依赖公共 include 面**
//   （lib/**/include ∪ lib/**/cpp），不得 include 任何模块 src/ 下的内部头；
//   机器判据 = DOC-004/AST-API（eng/tools/check_ast_api.py，clang 独立解析面）
//   + UT-BACKEND 的 test_public_header_layering.py（公共头 → src/ 依赖直接判红）。
//
// 日志落盘的**机制原语**属基建层（ASTROCS_DESIGN §10「aio 是文件级唯一 I/O
// 边界」; CLEAN-403）。故此处只声明 LogSink 接缝（前向声明的不透明句柄 + 四个
// 自由函数），把「经 aio 落盘」的唯一实现留在 TU 侧:
//   ipv/src/ipv_log_sink.h —— 由使用 Logger 的生产 TU 包含
//   （那些 TU 才有 lib/infrastructure/aio/src 的 include 面）。
// 同款先例: lib/algorithms/star_detection/src/sdet_log.{h,cpp} 与
// lib/algorithms/psf/src/dpsf_log.{h,cpp} 亦把 aio 机制原语只放在 src 侧。
// ============================================================================

#include <cstddef>
#include <string>
#include <mutex>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <sstream>

namespace ipv {

// ---- 日志落点接缝（声明）-------------------------------------------------
// 语义与 aio 机制原语一一对应（见 ipv/src/ipv_log_sink.h）:
//   log_sink_open  失败返回 nullptr（与 aio_atomic::append_open 同语义）;
//   log_sink_write 追加写 n 字节;
//   log_sink_flush 落盘;
//   log_sink_close 关闭并释放句柄（可传 nullptr）。
struct LogSink;
LogSink* log_sink_open(const std::string& path);
void log_sink_write(LogSink* sink, const char* data, std::size_t n);
void log_sink_flush(LogSink* sink);
void log_sink_close(LogSink* sink);

class Logger {
public:
    enum Level { INFO, WARN, ERROR, DEBUG };

    Logger() : enabled_(false) {}
    ~Logger() { close(); }

    // 初始化日志文件
    void init(const std::string& path) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (sink_) {
            log_sink_close(sink_);
            sink_ = nullptr;
        }
        sink_ = log_sink_open(path);
        if (sink_) {
            // UTF-8 BOM
            const char bom[] = {(char)0xEF, (char)0xBB, (char)0xBF};
            log_sink_write(sink_, bom, 3);
            const char kHeader[] = "=== IPV Plate Solve Log ===\n";
            log_sink_write(sink_, kHeader, sizeof(kHeader) - 1);
            log_sink_flush(sink_);
            enabled_ = true;
        }
    }

    void close() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (sink_) {
            const char kFooter[] = "=== Log End ===\n";
            log_sink_write(sink_, kFooter, sizeof(kFooter) - 1);
            log_sink_close(sink_);
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
            log_sink_write(sink_, line.data(), line.size());
            log_sink_flush(sink_);
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
    LogSink*      sink_ = nullptr;
    std::mutex    mtx_;
    bool          enabled_;
};

} // namespace ipv

#endif // IPV_LOG_H
