// ============================================================================
// cli_command.cpp - JSONL 事件输出 + SIGINT 信号处理 [+ SHA-256 deprecated shim]
//
// Phase1 JSON 入口重构后, 本文件仅保留:
// 1. SIGINT 信号处理 (p04004_register/unregister_signal_handler)
// 2. sha256_impl::sha256 — 已归一到 lib/common/crypto (B4-02)，此处仅为
// test_orchestrator_cli 兼容的 deprecated 转发 shim（新代码用 astrocs::crypto）
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
// 全局取消信号支持
// 通过全局指针在 SIGINT/Ctrl+C 信号处理器中调用 orch->request_cancel
// async-signal-safe 论证 (Bug 狩猎 R5 P1-2): handler 全路径只做 atomic
// load/store —— request_cancel() 已改为纯 cancel_token_ 原子 store (无锁、
// 无 logging、无内存分配); 之前在 handler 路径内经 request_cancel 调用
// LOG_WARN 会重入 Logger 非递归 mutex (中断主线程持锁写日志时死锁/UB),
// 该日志已移至主循环消费取消标志的分支 (check_stage_continue 等)。
// ============================================================================
static std::atomic<Orchestrator*> g_active_orchestrator{nullptr};
static std::atomic<bool> g_cancel_on_signal_enabled{false};

// SIGINT 信号处理器 (Ctrl+C)
// 设置 cancel_token_, 让正在执行的 stage 在下一个检查点停止
static void p04004_sigint_handler(int sig) {
    (void)sig;  // async-signal-safe: 仅下方两个 atomic load + 一个 atomic store
    Orchestrator* orch = g_active_orchestrator.load(std::memory_order_acquire);
    if (orch != nullptr && g_cancel_on_signal_enabled.load(std::memory_order_acquire)) {
        orch->request_cancel();
    }
}

// 注册信号处理器 (在 stage1/stage2 执行前调用)
void p04004_register_signal_handler(Orchestrator* orch, bool enable_cancel_on_signal) {
    g_active_orchestrator.store(orch, std::memory_order_release);
    g_cancel_on_signal_enabled.store(enable_cancel_on_signal, std::memory_order_release);
    if (enable_cancel_on_signal) {
        std::signal(SIGINT, p04004_sigint_handler);
        LOG_INFO("cli", "P04-004: --cancel-on-signal 已启用, Ctrl+C 将触发取消");
    }
}

// 注销信号处理器 (在命令完成后调用)
void p04004_unregister_signal_handler() {
    g_active_orchestrator.store(nullptr, std::memory_order_release);
    g_cancel_on_signal_enabled.store(false, std::memory_order_release);
    // 恢复默认 SIGINT 处理 (避免影响后续命令)
    std::signal(SIGINT, SIG_DFL);
}

// ============================================================================
// SHA-256 deprecated shim (B4-02 去重，DATA-FRAME-ID-001)
// 权威实现：lib/common/crypto/sha256.h/.cpp（astrocs::crypto::sha256_hex）。
// 旧独立实现已删除；此处仅保留 sha256_impl::sha256 转发以兼容历史测试。
// 新代码一律使用 astrocs::crypto::sha256_hex / Sha256。
// ============================================================================
namespace sha256_impl {
std::string sha256(const std::string& input) {
    return astrocs::crypto::sha256_hex(input.data(), input.size());
}
}

// ============================================================================
// 辅助函数 (供 output_jsonl_event_ex 使用)
// ============================================================================

// JSON 字符串转义 (P1-3 权威实现, 经 CliCommand::json_escape_string 对外暴露)
// 覆盖: " \\ \n \r \t 与全部 <0x20 控制字符 (\uXXXX);
// 0x7F (DEL) 与 UTF-8 多字节序列按 RFC 8259 原样保留 (输出仍为合法 JSON)。
// Windows 反斜杠路径 ('C:\\data\\a.fits')、引号与错误消息文本必须经此转义,
// 否则手工字符串拼接产出非法 JSON。
std::string CliCommand::json_escape_string(const std::string& s) {
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
// output_jsonl_event_ex - 扩展 JSONL 事件输出
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
    std::cout << "\"type\":\"" << CliCommand::json_escape_string(event_type) << "\",";
    std::cout << "\"job_id\":\"" << CliCommand::json_escape_string(job_id) << "\",";
    std::cout << "\"timestamp\":\"" << get_utc_timestamp() << "\"";
    if (!stage.empty()) {
        std::cout << ",\"stage\":\"" << CliCommand::json_escape_string(stage) << "\"";
    }
    if (duration_ms >= 0.0) {
        std::cout << ",\"duration_ms\":" << duration_ms;
    }
    if (!status.empty()) {
        std::cout << ",\"status\":\"" << CliCommand::json_escape_string(status) << "\"";
    }
    if (progress >= 0.0) {
        std::cout << ",\"progress\":" << progress;
    }
    if (!message.empty()) {
        std::cout << ",\"message\":\"" << CliCommand::json_escape_string(message) << "\"";
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
