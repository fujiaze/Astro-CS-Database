// ============================================================================
// logger.cpp - 集成日志系统实现
// 功能: 单例 Logger 的具体实现
//   - 单例模式 (Meyers' Singleton)
//   - 线程安全 (std::mutex 保护文件写入)
//   - 跨天自动切换日志文件
//   - 日志格式: [YYYY-MM-DD HH:MM:SS][LEVEL][module] message
// ============================================================================

#include "logger.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

// ============================================================================
// 单例模式 (Meyers' Singleton, C++11 起线程安全)
// ============================================================================
Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

// ============================================================================
// 构造 / 析构
// ============================================================================
Logger::Logger() {
    // 默认状态: 未初始化, INFO 级别, stderr 输出开启
}

Logger::~Logger() {
    // 析构时关闭日志文件
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

// ============================================================================
// init - 初始化日志系统
// ============================================================================
void Logger::init(const std::string& log_dir, LogLevel level) {
    // 加锁, 防止并发初始化
    std::lock_guard<std::mutex> lock(mutex_);

    // 设置日志目录和级别
    log_dir_ = log_dir;
    level_.store(level);

    // 创建日志目录 (若不存在)
    if (!log_dir_.empty()) {
        std::error_code ec;
        fs::create_directories(log_dir_, ec);
        // ec 非零不一定是错误 (目录可能已存在), 忽略
    }

    // 关闭旧文件, 下次写日志时自动打开新文件
    if (log_file_.is_open()) {
        log_file_.close();
    }
    current_date_.clear();
    log_file_path_.clear();

    // 标记为已初始化
    initialized_.store(true);

    // 输出初始化日志 (注意: 此时不要再调 log(), 防止递归锁)
    std::string ts = get_timestamp();
    std::cerr << "[" << ts << "][INFO][logger] 日志系统初始化完成"
              << " (log_dir=" << log_dir_
              << ", level=" << level_to_string(level) << ")" << std::endl;
}

// ============================================================================
// set_level / get_level - 原子操作
// ============================================================================
void Logger::set_level(LogLevel level) {
    level_.store(level);
}

LogLevel Logger::get_level() const {
    return level_.load();
}

// ============================================================================
// set_log_dir - 更新日志目录
// ============================================================================
void Logger::set_log_dir(const std::string& dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    log_dir_ = dir;
    // 关闭当前文件, 下次写日志时重新打开
    if (log_file_.is_open()) {
        log_file_.close();
    }
    current_date_.clear();
    log_file_path_.clear();

    // 创建日志目录 (若不存在)
    if (!log_dir_.empty()) {
        std::error_code ec;
        fs::create_directories(log_dir_, ec);
    }
}

// ============================================================================
// get_log_file_path - 获取当前日志文件路径
// ============================================================================
std::string Logger::get_log_file_path() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return log_file_path_;
}

// ============================================================================
// log - 主日志输出接口
// ============================================================================
void Logger::log(LogLevel level, const std::string& module, const std::string& message) {
    // 级别过滤 (小于当前级别则跳过)
    if (static_cast<int>(level) < static_cast<int>(level_.load())) {
        return;
    }

    // 格式化日志行
    std::string line = format_line(level, module, message);

    // 加锁, 保证文件写入线程安全
    std::lock_guard<std::mutex> lock(mutex_);

    // 输出到 stderr (用户可见)
    if (stderr_output_.load()) {
        std::cerr << line;
        std::cerr.flush();
    }

    // 输出到日志文件 (若已初始化)
    if (initialized_.load()) {
        ensure_log_file();
        if (log_file_.is_open()) {
            log_file_ << line;
            log_file_.flush();
        }
    }
}

// ============================================================================
// 便捷方法
// ============================================================================
void Logger::debug(const std::string& module, const std::string& message) {
    log(LogLevel::DEBUG, module, message);
}

void Logger::info(const std::string& module, const std::string& message) {
    log(LogLevel::INFO, module, message);
}

void Logger::warn(const std::string& module, const std::string& message) {
    log(LogLevel::WARN, module, message);
}

void Logger::error(const std::string& module, const std::string& message) {
    log(LogLevel::ERROR, module, message);
}

// ============================================================================
// shutdown - 关闭日志系统
// ============================================================================
void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (log_file_.is_open()) {
        log_file_.close();
    }
    initialized_.store(false);
    current_date_.clear();
    log_file_path_.clear();
}

// ============================================================================
// level_to_string - 日志级别转字符串
// ============================================================================
std::string Logger::level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default:              return "UNKNOWN";
    }
}

// ============================================================================
// string_to_level - 字符串转日志级别 (大小写不敏感)
// ============================================================================
LogLevel Logger::string_to_level(const std::string& str) {
    // 转大写比较
    std::string s;
    s.reserve(str.size());
    for (char c : str) {
        s.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }

    if (s == "DEBUG") return LogLevel::DEBUG;
    if (s == "INFO")  return LogLevel::INFO;
    if (s == "WARN" || s == "WARNING") return LogLevel::WARN;
    if (s == "ERROR") return LogLevel::ERROR;

    // 无效字符串默认返回 INFO
    return LogLevel::INFO;
}

// ============================================================================
// set_stderr_output - 控制 stderr 输出
// ============================================================================
void Logger::set_stderr_output(bool enable) {
    stderr_output_.store(enable);
}

// ============================================================================
// get_date_string - 获取当前日期 YYYY-MM-DD
// ============================================================================
std::string Logger::get_date_string() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_local;
#ifdef _WIN32
    localtime_s(&tm_local, &t);
#else
    localtime_r(&t, &tm_local);
#endif
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << (tm_local.tm_year + 1900) << "-"
        << std::setw(2) << (tm_local.tm_mon + 1) << "-"
        << std::setw(2) << tm_local.tm_mday;
    return oss.str();
}

// ============================================================================
// get_timestamp - 获取当前时间 YYYY-MM-DD HH:MM:SS
// ============================================================================
std::string Logger::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_local;
#ifdef _WIN32
    localtime_s(&tm_local, &t);
#else
    localtime_r(&t, &tm_local);
#endif
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << (tm_local.tm_year + 1900) << "-"
        << std::setw(2) << (tm_local.tm_mon + 1) << "-"
        << std::setw(2) << tm_local.tm_mday << " "
        << std::setw(2) << tm_local.tm_hour << ":"
        << std::setw(2) << tm_local.tm_min << ":"
        << std::setw(2) << tm_local.tm_sec;
    return oss.str();
}

// ============================================================================
// ensure_log_file - 确保日志文件已打开 (跨天自动切换)
// 注意: 调用者必须持有 mutex_
// ============================================================================
void Logger::ensure_log_file() {
    // 获取当前日期
    std::string today = get_date_string();

    // 已打开文件且日期未变化: 无需操作
    if (log_file_.is_open() && today == current_date_) {
        return;
    }

    // 关闭旧文件
    if (log_file_.is_open()) {
        log_file_.close();
    }

    // 构造新文件路径: log_dir_/orchestrator_YYYY-MM-DD.log
    if (log_dir_.empty()) {
        return;  // 无日志目录, 无法创建文件
    }

    std::string filename = "orchestrator_" + today + ".log";
    fs::path full_path = fs::path(log_dir_) / filename;
    log_file_path_ = full_path.string();
    current_date_ = today;

    // 以 append 模式打开 (保留同一天的历史日志)
    log_file_.open(log_file_path_, std::ios::app);
    // 打开失败时静默忽略, stderr 仍可输出
}

// ============================================================================
// format_line - 格式化日志行
// ============================================================================
std::string Logger::format_line(LogLevel level, const std::string& module, const std::string& message) {
    std::ostringstream oss;
    oss << "[" << get_timestamp() << "]"
        << "[" << level_to_string(level) << "]"
        << "[" << module << "] "
        << message << "\n";
    return oss.str();
}
