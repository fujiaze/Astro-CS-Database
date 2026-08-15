// ============================================================================
// logger.h - 集成日志系统
// 功能: 统一管理编排器和各模块的日志输出
// - 单例模式 (Meyers' Singleton)
// - 线程安全 (std::mutex 保护文件写入)
// - 跨天自动切换日志文件
// - 默认日志级别 INFO
// - 默认日志路径: lib/orchestrator/logs/orchestrator_YYYY-MM-DD.log
// - 日志格式: [YYYY-MM-DD HH:MM:SS][LEVEL][module] message
// 用途: 编排器 C++ CLI 各模块的统一日志接口
// ============================================================================

#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <atomic>

// Windows.h 定义了 ERROR 宏 (0L), 与 LogLevel::ERROR 冲突
// 在定义 LogLevel 枚举前取消该宏定义, 保证枚举值名称可用
// (本项目中不直接使用 Windows ERROR 宏, 取消是安全的)
#ifdef _WIN32
#ifdef ERROR
#undef ERROR
#endif
#endif

// 日志级别 (值越小, 优先级越低)
enum class LogLevel {
    DEBUG = 0,
    INFO  = 1,
    WARN  = 2,
    ERROR = 3
};

// ============================================================================
// Logger 日志器 (单例)
// ============================================================================
class Logger {
public:
    // 单例模式 (Meyers' Singleton)
    static Logger& instance();

    // 初始化日志系统
    // log_dir: 日志目录 (相对或绝对路径)
    // level: 日志级别 (默认 INFO)
    void init(const std::string& log_dir, LogLevel level = LogLevel::INFO);

    // 设置/获取日志级别 (原子操作)
    void set_level(LogLevel level);
    LogLevel get_level() const;

    // 设置日志目录 (更新后关闭当前文件, 下次写日志时重新打开)
    void set_log_dir(const std::string& dir);

    // 获取当前日志文件路径
    std::string get_log_file_path() const;

    // 日志输出 (主接口)
    // level < level_ 时跳过
    void log(LogLevel level, const std::string& module, const std::string& message);

    // 便捷方法
    void debug(const std::string& module, const std::string& message);
    void info(const std::string& module, const std::string& message);
    void warn(const std::string& module, const std::string& message);
    void error(const std::string& module, const std::string& message);

    // 关闭日志 (关闭日志文件, 设置 initialized_ = false)
    void shutdown();

    // 日志级别转字符串 ("DEBUG"/"INFO"/"WARN"/"ERROR")
    static std::string level_to_string(LogLevel level);

    // 字符串转日志级别 (大小写不敏感, 无效返回 INFO)
    static LogLevel string_to_level(const std::string& str);

    // 是否输出到 stderr (默认 true)
    void set_stderr_output(bool enable);

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string log_dir_;        // 日志目录
    std::string log_file_path_;  // 当前日志文件路径
    std::string current_date_;   // 当前文件对应日期 (YYYY-MM-DD), 用于跨天切换
    std::ofstream log_file_;     // 日志文件流
    mutable std::mutex mutex_;   // 保护文件写入
    std::atomic<LogLevel> level_{LogLevel::INFO};        // 当前日志级别
    std::atomic<bool> stderr_output_{true};              // 是否输出到 stderr
    std::atomic<bool> initialized_{false};               // 是否已初始化

    // 获取当前日期字符串 (YYYY-MM-DD)
    std::string get_date_string();

    // 获取当前时间字符串 (YYYY-MM-DD HH:MM:SS)
    std::string get_timestamp();

    // 确保日志文件已打开 (跨天自动切换)
    void ensure_log_file();

    // 格式化日志行: "[timestamp][LEVEL][module] message\n"
    std::string format_line(LogLevel level, const std::string& module, const std::string& message);
};

// ============================================================================
// 便捷宏 (建议在各模块中使用)
// ============================================================================
#define LOG_DEBUG(module, msg) Logger::instance().debug(module, msg)
#define LOG_INFO(module, msg)  Logger::instance().info(module, msg)
#define LOG_WARN(module, msg)  Logger::instance().warn(module, msg)
#define LOG_ERROR(module, msg) Logger::instance().error(module, msg)
