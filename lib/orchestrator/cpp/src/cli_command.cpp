// ============================================================================
// cli_command.cpp - JSONL 事件输出 + SIGINT 信号处理 + SHA-256 实现
//
// Phase1 JSON 入口重构后, 本文件仅保留:
// 1. P04-004 SIGINT 信号处理 (p04004_register/unregister_signal_handler)
// 2. sha256_impl::sha256 (纯 C++17 SHA-256, 供 json_config.cpp 调用)
// 3. CliCommand::output_jsonl_event_ex (JSONL 事件输出, 供 main.cpp 调用)
//
// 已删除:
// - 所有 cmd_* 命令实现 (cmd_run/cmd_run_batch/cmd_stage1/cmd_stage2/...)
// - json_merge namespace (手写 JSON 解析器)
// - BUILTIN_DEFAULT_CONFIG / compute_effective_config (配置优先级逻辑)
// - merge_stage1_overrides_into_config / print_stage1_diagnostics
// - output_json_result / output_json_batch / output_jsonl_event (旧版)
// - CliCommand::execute / CliCommand::print_usage
// - stage_str / generate_job_id
// ============================================================================

#include "cli_command.h"
#include "logger.h"
#include "crypto/sha256.h"

#include <iostream>
#include <sstream>
#include <string>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <csignal>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#endif

// ============================================================================
// P04-004: 全局取消信号支持
// 通过全局指针在 SIGINT/Ctrl+C 信号处理器中调用 orch->request_cancel()
// 注意: 信号处理器中只能调用 async-signal-safe 函数, atomic store 是安全的
// ============================================================================
static std::atomic<Orchestrator*> g_active_orchestrator{nullptr};
static std::atomic<bool> g_cancel_on_signal_enabled{false};

// P04-004: SIGINT 信号处理器 (Ctrl+C)
// 设置 cancel_token_, 让正在执行的 stage 在下一个检查点停止
static void p04004_sigint_handler(int sig) {
    (void)sig;
    Orchestrator* orch = g_active_orchestrator.load(std::memory_order_acquire);
    if (orch != nullptr && g_cancel_on_signal_enabled.load(std::memory_order_acquire)) {
        orch->request_cancel();
    }
}

// P04-004: 注册信号处理器 (在 stage1/stage2 执行前调用)
void p04004_register_signal_handler(Orchestrator* orch, bool enable_cancel_on_signal) {
    g_active_orchestrator.store(orch, std::memory_order_release);
    g_cancel_on_signal_enabled.store(enable_cancel_on_signal, std::memory_order_release);
    if (enable_cancel_on_signal) {
        std::signal(SIGINT, p04004_sigint_handler);
        LOG_INFO("cli", "P04-004: --cancel-on-signal 已启用, Ctrl+C 将触发取消");
    }
}

// P04-004: 注销信号处理器 (在命令完成后调用)
void p04004_unregister_signal_handler() {
    g_active_orchestrator.store(nullptr, std::memory_order_release);
    g_cancel_on_signal_enabled.store(false, std::memory_order_release);
    // 恢复默认 SIGINT 处理 (避免影响后续命令)
    std::signal(SIGINT, SIG_DFL);
}

// ============================================================================
// P04-001: SHA-256 纯 C++17 实现 (无外部依赖)
// 用于计算 config 的 hash, 保证可追溯性
// V18R2 (CODE-002): SHA-256 归一化到 lib/common/crypto（单一实现）。
// 旧 sha256_impl 已删除；json_config 改用 astrocs::crypto::sha256_hex。
// ============================================================================

// ============================================================================
// 辅助函数 (供 output_jsonl_event_ex 使用)
// ============================================================================

// JSON 字符串转义
static std::string json_escape(const std::string& s) {
    std::string r;
    r.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n";  break;
            case '\r': r += "\\r";  break;
            case '\t': r += "\\t";  break;
            default:
                if ((unsigned char)c < 0x20) {
                    // 控制字符: \uXXXX
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                    r += buf;
                } else {
                    r += c;
                }
                break;
        }
    }
    return r;
}

// 获取当前 UTC 时间 ISO 8601 字符串
static std::string get_utc_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc;
#ifdef _WIN32
    gmtime_s(&tm_utc, &t);
#else
    gmtime_r(&t, &tm_utc);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

// ============================================================================
// P04-002: output_jsonl_event_ex - 扩展 JSONL 事件输出
// 含数字 exit_code + duration_ms + status + 额外字段
// 用于 stage_start/stage_end/result/error/warning/progress 事件
// ============================================================================
void CliCommand::output_jsonl_event_ex(const std::string& event_type,
                                       const std::string& job_id,
                                       const std::string& stage,
                                       double progress,
                                       const std::string& message,
                                       const std::string& result_json,
                                       const std::string& error_json,
                                       int exit_code,
                                       double duration_ms,
                                       const std::string& status,
                                       const std::string& extra_json) {
    std::cout << "{";
    std::cout << "\"schema_version\":1,";
    std::cout << "\"type\":\"" << json_escape(event_type) << "\",";
    std::cout << "\"job_id\":\"" << json_escape(job_id) << "\",";
    std::cout << "\"timestamp\":\"" << get_utc_timestamp() << "\"";
    if (!stage.empty()) {
        std::cout << ",\"stage\":\"" << json_escape(stage) << "\"";
    }
    if (duration_ms >= 0.0) {
        std::cout << ",\"duration_ms\":" << duration_ms;
    }
    if (!status.empty()) {
        std::cout << ",\"status\":\"" << json_escape(status) << "\"";
    }
    if (progress >= 0.0) {
        std::cout << ",\"progress\":" << progress;
    }
    if (!message.empty()) {
        std::cout << ",\"message\":\"" << json_escape(message) << "\"";
    }
    if (!result_json.empty()) {
        std::cout << ",\"result\":" << result_json;
    }
    if (!error_json.empty()) {
        std::cout << ",\"error\":" << error_json;
    }
    if (exit_code >= 0) {
        std::cout << ",\"exit_code\":" << exit_code;
    }
    if (!extra_json.empty()) {
        // extra_json 应以 "," 开头, 直接追加
        std::cout << extra_json;
    }
    std::cout << "}" << std::endl;
}
