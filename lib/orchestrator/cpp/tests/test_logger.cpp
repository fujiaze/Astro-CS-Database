// ============================================================================
// test_logger.cpp - Logger 单元测试
// 功能: 验证日志系统的所有功能
//
// 测试内容 (10 项):
//   1. 日志级别设置和获取
//   2. DEBUG 级别输出 (默认 INFO 时不输出 DEBUG)
//   3. INFO/WARN/ERROR 级别输出
//   4. 日志文件创建
//   5. 日志格式正确 (时间戳/级别/模块/消息)
//   6. level_to_string/string_to_level 转换
//   7. stderr 输出开关
//   8. 多线程安全 (10 个线程各输出 100 条日志, 验证无丢失)
//   9. 日志文件路径 (orchestrator_YYYY-MM-DD.log)
//  10. shutdown 后不再写文件
//
// 编译:
//   g++ -O2 -std=c++17 -Wall -fopenmp -o tests/test_logger.exe
//       tests/test_logger.cpp src/logger.cpp -Iinclude -static
//
// 运行 (任意目录):
//   lib\orchestrator\cpp\tests\test_logger.exe
// ============================================================================

#include "logger.h"

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdio>
#include <chrono>
#include <thread>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
// windows.h 在 logger.h 之后包含, 会重新定义 ERROR 宏
// 这里再次取消, 保证后续 LogLevel::ERROR 可用
#ifdef ERROR
#undef ERROR
#endif
#endif

namespace fs = std::filesystem;

// ============================================================================
// 测试辅助宏 (与 test_checkpoint.cpp / test_dll_loader.cpp 风格一致)
// ============================================================================
static int g_pass_count = 0;
static int g_fail_count = 0;

#define TEST_CHECK(cond, msg)                                                  \
    do {                                                                       \
        if (cond) {                                                            \
            std::cerr << "  [PASS] " << (msg) << std::endl;                    \
            ++g_pass_count;                                                    \
        } else {                                                               \
            std::cerr << "  [FAIL] " << (msg)                                  \
                      << " (line " << __LINE__ << ")" << std::endl;            \
            ++g_fail_count;                                                    \
        }                                                                      \
    } while (0)

#define TEST_SECTION(name)                                                     \
    do {                                                                       \
        std::cerr << "\n========================================================"   \
                  << "\n[测试] " << (name)                                     \
                  << "\n========================================================"   \
                  << std::endl;                                                \
    } while (0)

// ============================================================================
// 辅助: 创建临时目录 (基于时间戳 + PID)
// ============================================================================
static std::string make_temp_dir(const std::string& prefix = "logger_test_") {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    std::ostringstream oss;
    oss << prefix << ns << "_" << std::this_thread::get_id();
    std::string dir = oss.str();
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

// ============================================================================
// 辅助: 读取文件全部内容
// ============================================================================
static std::string read_file_all(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) return "";
    std::stringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

// ============================================================================
// 辅助: 统计字符串中子串出现次数
// ============================================================================
static int count_substring(const std::string& text, const std::string& sub) {
    if (sub.empty()) return 0;
    int count = 0;
    size_t pos = 0;
    while ((pos = text.find(sub, pos)) != std::string::npos) {
        ++count;
        pos += sub.size();
    }
    return count;
}

// ============================================================================
// 测试 1: 日志级别设置和获取
// ============================================================================
static void test_level_set_get() {
    TEST_SECTION("1. 日志级别设置和获取");

    Logger& log = Logger::instance();
    log.set_level(LogLevel::WARN);
    TEST_CHECK(log.get_level() == LogLevel::WARN, "set_level(WARN) -> get_level()");

    log.set_level(LogLevel::ERROR);
    TEST_CHECK(log.get_level() == LogLevel::ERROR, "set_level(ERROR) -> get_level()");

    log.set_level(LogLevel::DEBUG);
    TEST_CHECK(log.get_level() == LogLevel::DEBUG, "set_level(DEBUG) -> get_level()");

    log.set_level(LogLevel::INFO);
    TEST_CHECK(log.get_level() == LogLevel::INFO, "set_level(INFO) -> get_level()");
}

// ============================================================================
// 测试 2: DEBUG 级别在 INFO 设置下不输出
// ============================================================================
static void test_debug_filtered() {
    TEST_SECTION("2. DEBUG 级别在 INFO 设置下不输出");

    std::string tmp_dir = make_temp_dir();
    Logger& log = Logger::instance();

    // 初始化为 INFO 级别, 关闭 stderr 输出避免干扰
    log.init(tmp_dir, LogLevel::INFO);
    log.set_stderr_output(false);

    // 输出 DEBUG 和 INFO 日志
    log.debug("test", "DEBUG_LEVEL_MESSAGE_SHOULD_NOT_APPEAR");
    log.info("test", "INFO_LEVEL_MESSAGE_SHOULD_APPEAR");

    // 获取日志文件路径并读取内容
    std::string file_path = log.get_log_file_path();
    TEST_CHECK(!file_path.empty(), "日志文件路径非空");

    std::string content = read_file_all(file_path);
    TEST_CHECK(content.find("DEBUG_LEVEL_MESSAGE_SHOULD_NOT_APPEAR") == std::string::npos,
               "DEBUG 级别在 INFO 设置下不写入文件");
    TEST_CHECK(content.find("INFO_LEVEL_MESSAGE_SHOULD_APPEAR") != std::string::npos,
               "INFO 级别日志写入文件");

    // 清理
    log.shutdown();
    log.set_stderr_output(true);
    std::error_code ec;
    fs::remove_all(tmp_dir, ec);
}

// ============================================================================
// 测试 3: INFO/WARN/ERROR 级别输出
// ============================================================================
static void test_info_warn_error_output() {
    TEST_SECTION("3. INFO/WARN/ERROR 级别输出");

    std::string tmp_dir = make_temp_dir();
    Logger& log = Logger::instance();

    log.init(tmp_dir, LogLevel::INFO);
    log.set_stderr_output(false);

    log.info("mod1", "INFO_MSG_ABC");
    log.warn("mod2", "WARN_MSG_DEF");
    log.error("mod3", "ERROR_MSG_GHI");

    std::string file_path = log.get_log_file_path();
    std::string content = read_file_all(file_path);

    TEST_CHECK(content.find("INFO_MSG_ABC") != std::string::npos, "INFO 日志写入文件");
    TEST_CHECK(content.find("WARN_MSG_DEF") != std::string::npos, "WARN 日志写入文件");
    TEST_CHECK(content.find("ERROR_MSG_GHI") != std::string::npos, "ERROR 日志写入文件");

    // 验证级别标记
    TEST_CHECK(content.find("[INFO]") != std::string::npos, "INFO 日志含 [INFO] 标记");
    TEST_CHECK(content.find("[WARN]") != std::string::npos, "WARN 日志含 [WARN] 标记");
    TEST_CHECK(content.find("[ERROR]") != std::string::npos, "ERROR 日志含 [ERROR] 标记");

    // 验证模块名
    TEST_CHECK(content.find("[mod1]") != std::string::npos, "INFO 日志含 [mod1] 模块名");
    TEST_CHECK(content.find("[mod2]") != std::string::npos, "WARN 日志含 [mod2] 模块名");
    TEST_CHECK(content.find("[mod3]") != std::string::npos, "ERROR 日志含 [mod3] 模块名");

    log.shutdown();
    log.set_stderr_output(true);
    std::error_code ec;
    fs::remove_all(tmp_dir, ec);
}

// ============================================================================
// 测试 4: 日志文件创建
// ============================================================================
static void test_log_file_creation() {
    TEST_SECTION("4. 日志文件创建");

    std::string tmp_dir = make_temp_dir();
    Logger& log = Logger::instance();

    log.init(tmp_dir, LogLevel::INFO);
    log.set_stderr_output(false);

    // 在输出日志前, 文件路径可能尚未确定 (懒打开)
    // 输出第一条日志后, 文件应被创建
    log.info("test", "trigger_file_creation");

    std::string file_path = log.get_log_file_path();
    TEST_CHECK(!file_path.empty(), "日志文件路径已设置");
    TEST_CHECK(fs::exists(file_path), "日志文件存在");

    log.shutdown();
    log.set_stderr_output(true);
    std::error_code ec;
    fs::remove_all(tmp_dir, ec);
}

// ============================================================================
// 测试 5: 日志格式正确 (时间戳/级别/模块/消息)
// ============================================================================
static void test_log_format() {
    TEST_SECTION("5. 日志格式正确");

    std::string tmp_dir = make_temp_dir();
    Logger& log = Logger::instance();

    log.init(tmp_dir, LogLevel::DEBUG);
    log.set_stderr_output(false);

    log.info("modfmt", "MSG_BODY_XYZ");

    std::string file_path = log.get_log_file_path();
    std::string content = read_file_all(file_path);

    // 格式: [YYYY-MM-DD HH:MM:SS][LEVEL][module] message
    // 验证各字段存在
    // 时间戳前缀: 4 位数字年份
    TEST_CHECK(content.size() >= 4 && std::isdigit(static_cast<unsigned char>(content[1])),
               "日志行起始为 [YYYY 形式");

    // 验证 [INFO][modfmt] MSG_BODY_XYZ 的拼接顺序
    size_t pos_info = content.find("[INFO]");
    size_t pos_mod = content.find("[modfmt]");
    size_t pos_msg = content.find("MSG_BODY_XYZ");
    TEST_CHECK(pos_info != std::string::npos && pos_mod != std::string::npos
               && pos_msg != std::string::npos,
               "INFO/模块/消息 字段全部存在");
    TEST_CHECK(pos_info < pos_mod && pos_mod < pos_msg,
               "字段顺序: [INFO] < [modfmt] < MSG_BODY_XYZ");

    // 验证时间戳包含 4 位年份 + "-"+ 2 位月份 + "-" + 2 位日期
    bool ts_ok = false;
    if (content.size() >= 11) {
        // 格式: [YYYY-MM-DD
        if (content[0] == '[' && std::isdigit(static_cast<unsigned char>(content[1]))
            && std::isdigit(static_cast<unsigned char>(content[2]))
            && std::isdigit(static_cast<unsigned char>(content[3]))
            && std::isdigit(static_cast<unsigned char>(content[4]))
            && content[5] == '-' && content[8] == '-' && content[11] == ' ') {
            ts_ok = true;
        }
    }
    TEST_CHECK(ts_ok, "时间戳格式为 [YYYY-MM-DD ...");

    log.shutdown();
    log.set_stderr_output(true);
    std::error_code ec;
    fs::remove_all(tmp_dir, ec);
}

// ============================================================================
// 测试 6: level_to_string / string_to_level 转换
// ============================================================================
static void test_level_string_convert() {
    TEST_SECTION("6. level_to_string / string_to_level 转换");

    // level_to_string
    TEST_CHECK(Logger::level_to_string(LogLevel::DEBUG) == "DEBUG", "DEBUG -> \"DEBUG\"");
    TEST_CHECK(Logger::level_to_string(LogLevel::INFO) == "INFO", "INFO -> \"INFO\"");
    TEST_CHECK(Logger::level_to_string(LogLevel::WARN) == "WARN", "WARN -> \"WARN\"");
    TEST_CHECK(Logger::level_to_string(LogLevel::ERROR) == "ERROR", "ERROR -> \"ERROR\"");

    // string_to_level (大写)
    TEST_CHECK(Logger::string_to_level("DEBUG") == LogLevel::DEBUG, "\"DEBUG\" -> DEBUG");
    TEST_CHECK(Logger::string_to_level("INFO") == LogLevel::INFO, "\"INFO\" -> INFO");
    TEST_CHECK(Logger::string_to_level("WARN") == LogLevel::WARN, "\"WARN\" -> WARN");
    TEST_CHECK(Logger::string_to_level("ERROR") == LogLevel::ERROR, "\"ERROR\" -> ERROR");

    // string_to_level (小写, 大小写不敏感)
    TEST_CHECK(Logger::string_to_level("debug") == LogLevel::DEBUG, "\"debug\" -> DEBUG");
    TEST_CHECK(Logger::string_to_level("info") == LogLevel::INFO, "\"info\" -> INFO");
    TEST_CHECK(Logger::string_to_level("warn") == LogLevel::WARN, "\"warn\" -> WARN");
    TEST_CHECK(Logger::string_to_level("error") == LogLevel::ERROR, "\"error\" -> ERROR");

    // string_to_level (混合大小写)
    TEST_CHECK(Logger::string_to_level("Info") == LogLevel::INFO, "\"Info\" -> INFO");
    TEST_CHECK(Logger::string_to_level("WaRn") == LogLevel::WARN, "\"WaRn\" -> WARN");

    // string_to_level (无效字符串默认 INFO)
    TEST_CHECK(Logger::string_to_level("INVALID") == LogLevel::INFO, "\"INVALID\" -> INFO (默认)");
    TEST_CHECK(Logger::string_to_level("") == LogLevel::INFO, "\"\" -> INFO (默认)");

    // WARNING 别名
    TEST_CHECK(Logger::string_to_level("WARNING") == LogLevel::WARN, "\"WARNING\" -> WARN (别名)");
    TEST_CHECK(Logger::string_to_level("warning") == LogLevel::WARN, "\"warning\" -> WARN (别名)");
}

// ============================================================================
// 测试 7: stderr 输出开关
// ============================================================================
static void test_stderr_switch() {
    TEST_SECTION("7. stderr 输出开关");

    std::string tmp_dir = make_temp_dir();
    Logger& log = Logger::instance();

    log.init(tmp_dir, LogLevel::INFO);
    log.set_stderr_output(false);

    // 输出后, 日志文件应该有内容, stderr 不应被打印 (此处仅验证文件仍写入)
    log.info("test", "STDERR_OFF_TEST");
    std::string file_path = log.get_log_file_path();
    std::string content1 = read_file_all(file_path);
    TEST_CHECK(content1.find("STDERR_OFF_TEST") != std::string::npos,
               "stderr 关闭时, 日志仍写入文件");

    // 重新开启 stderr 输出
    log.set_stderr_output(true);
    log.info("test", "STDERR_ON_TEST");
    std::string content2 = read_file_all(file_path);
    TEST_CHECK(content2.find("STDERR_ON_TEST") != std::string::npos,
               "stderr 开启时, 日志写入文件");

    log.shutdown();
    std::error_code ec;
    fs::remove_all(tmp_dir, ec);
}

// ============================================================================
// 测试 8: 多线程安全 (10 个线程各输出 100 条日志, 验证无丢失)
// ============================================================================
static void test_multithread_safety() {
    TEST_SECTION("8. 多线程安全 (10 线程 × 100 条 = 1000 条)");

    std::string tmp_dir = make_temp_dir();
    Logger& log = Logger::instance();

    log.init(tmp_dir, LogLevel::INFO);
    log.set_stderr_output(false);  // 关闭 stderr, 避免大量输出干扰

    const int n_threads = 10;
    const int n_per_thread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> counter{0};

    auto worker = [&log, &counter, n_per_thread](int tid) {
        for (int i = 0; i < n_per_thread; ++i) {
            std::ostringstream oss;
            oss << "T" << tid << "_MSG_" << i;
            log.info("mt", oss.str());
            ++counter;
        }
    };

    for (int t = 0; t < n_threads; ++t) {
        threads.emplace_back(worker, t);
    }
    for (auto& th : threads) {
        th.join();
    }

    TEST_CHECK(counter.load() == n_threads * n_per_thread,
               "所有线程日志调用次数正确");

    // 验证日志文件中消息条数
    std::string file_path = log.get_log_file_path();
    std::string content = read_file_all(file_path);

    // 统计 [mt] 出现次数 (每条日志含一个 [mt])
    int mt_count = count_substring(content, "[mt]");
    TEST_CHECK(mt_count == n_threads * n_per_thread,
               "日志文件中消息条数无丢失");

    // 验证每个线程的每条消息都存在
    bool all_present = true;
    for (int t = 0; t < n_threads && all_present; ++t) {
        for (int i = 0; i < n_per_thread; ++i) {
            std::ostringstream oss;
            oss << "T" << t << "_MSG_" << i;
            if (content.find(oss.str()) == std::string::npos) {
                all_present = false;
                break;
            }
        }
    }
    TEST_CHECK(all_present, "1000 条日志全部存在于文件中");

    log.shutdown();
    log.set_stderr_output(true);
    std::error_code ec;
    fs::remove_all(tmp_dir, ec);
}

// ============================================================================
// 测试 9: 日志文件路径 (orchestrator_YYYY-MM-DD.log)
// ============================================================================
static void test_log_file_path_format() {
    TEST_SECTION("9. 日志文件路径 (orchestrator_YYYY-MM-DD.log)");

    std::string tmp_dir = make_temp_dir();
    Logger& log = Logger::instance();

    log.init(tmp_dir, LogLevel::INFO);
    log.set_stderr_output(false);
    log.info("test", "trigger_path_check");

    std::string file_path = log.get_log_file_path();

    // 路径应包含 tmp_dir 前缀
    TEST_CHECK(file_path.find(tmp_dir) != std::string::npos,
               "日志文件路径包含日志目录");

    // 文件名应为 orchestrator_YYYY-MM-DD.log
    fs::path p(file_path);
    std::string filename = p.filename().string();
    TEST_CHECK(filename.find("orchestrator_") == 0,
               "文件名以 orchestrator_ 开头");
    TEST_CHECK(filename.find(".log") == filename.size() - 4,
               "文件名以 .log 结尾");

    // 验证日期部分 (orchestrator_YYYY-MM-DD.log)
    // 期望格式: orchestrator_ 后跟 10 字符 (YYYY-MM-DD), 再跟 .log
    bool date_format_ok = false;
    if (filename.size() >= 22) {  // orchestrator_(13) + YYYY-MM-DD(10) + .log(4) = 27
        std::string date_part = filename.substr(13, 10);  // YYYY-MM-DD
        if (date_part.size() == 10
            && std::isdigit(static_cast<unsigned char>(date_part[0]))
            && std::isdigit(static_cast<unsigned char>(date_part[1]))
            && std::isdigit(static_cast<unsigned char>(date_part[2]))
            && std::isdigit(static_cast<unsigned char>(date_part[3]))
            && date_part[4] == '-'
            && std::isdigit(static_cast<unsigned char>(date_part[5]))
            && std::isdigit(static_cast<unsigned char>(date_part[6]))
            && date_part[7] == '-'
            && std::isdigit(static_cast<unsigned char>(date_part[8]))
            && std::isdigit(static_cast<unsigned char>(date_part[9]))) {
            date_format_ok = true;
        }
    }
    TEST_CHECK(date_format_ok, "文件名含日期 YYYY-MM-DD");

    log.shutdown();
    log.set_stderr_output(true);
    std::error_code ec;
    fs::remove_all(tmp_dir, ec);
}

// ============================================================================
// 测试 10: shutdown 后不再写文件
// ============================================================================
static void test_shutdown_no_write() {
    TEST_SECTION("10. shutdown 后不再写文件");

    std::string tmp_dir = make_temp_dir();
    Logger& log = Logger::instance();

    log.init(tmp_dir, LogLevel::INFO);
    log.set_stderr_output(false);
    log.info("test", "BEFORE_SHUTDOWN");

    std::string file_path = log.get_log_file_path();
    std::string content1 = read_file_all(file_path);

    // 关闭日志
    log.shutdown();

    // 再次输出日志 (应被丢弃)
    log.info("test", "AFTER_SHUTDOWN_SHOULD_NOT_APPEAR");
    log.error("test", "AFTER_SHUTDOWN_ERROR");

    std::string content2 = read_file_all(file_path);

    TEST_CHECK(content1.find("BEFORE_SHUTDOWN") != std::string::npos,
               "shutdown 前日志已写入");
    TEST_CHECK(content2.find("AFTER_SHUTDOWN_SHOULD_NOT_APPEAR") == std::string::npos,
               "shutdown 后 INFO 不再写入文件");
    TEST_CHECK(content2.find("AFTER_SHUTDOWN_ERROR") == std::string::npos,
               "shutdown 后 ERROR 不再写入文件");
    TEST_CHECK(content1 == content2, "shutdown 后文件内容未变化");

    log.set_stderr_output(true);
    std::error_code ec;
    fs::remove_all(tmp_dir, ec);
}

// ============================================================================
// main - 运行全部测试
// ============================================================================
int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    std::cerr << "========================================================" << std::endl;
    std::cerr << "Logger 单元测试" << std::endl;
    std::cerr << "========================================================" << std::endl;

    test_level_set_get();
    test_debug_filtered();
    test_info_warn_error_output();
    test_log_file_creation();
    test_log_format();
    test_level_string_convert();
    test_stderr_switch();
    test_multithread_safety();
    test_log_file_path_format();
    test_shutdown_no_write();

    // 最终状态确保还原
    Logger::instance().set_stderr_output(true);

    std::cerr << "\n========================================================" << std::endl;
    std::cerr << "测试汇总: " << g_pass_count << " 通过, "
              << g_fail_count << " 失败" << std::endl;
    std::cerr << "========================================================" << std::endl;

    return (g_fail_count == 0) ? 0 : 1;
}
