// ============================================================================
// cli_command.cpp - JSONL 事件输出 + SIGINT 信号处理 + SHA-256 实现
//
// Phase1 JSON 入口重构后, 本文件仅保留:
//   1. P04-004 SIGINT 信号处理 (p04004_register/unregister_signal_handler)
//   2. sha256_impl::sha256 (纯 C++17 SHA-256, 供 json_config.cpp 调用)
//   3. CliCommand::output_jsonl_event_ex (JSONL 事件输出, 供 main.cpp 调用)
//
// 已删除:
//   - 所有 cmd_* 命令实现 (cmd_run/cmd_run_batch/cmd_stage1/cmd_stage2/...)
//   - json_merge namespace (手写 JSON 解析器)
//   - BUILTIN_DEFAULT_CONFIG / compute_effective_config (配置优先级逻辑)
//   - merge_stage1_overrides_into_config / print_stage1_diagnostics
//   - output_json_result / output_json_batch / output_jsonl_event (旧版)
//   - CliCommand::execute / CliCommand::print_usage
//   - stage_str / generate_job_id
// ============================================================================

#include "cli_command.h"
#include "logger.h"

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
// ============================================================================
namespace sha256_impl {

constexpr uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

std::string sha256(const std::string& input) {
    uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};

    // 预处理: 补位 + 长度
    std::string msg = input;
    uint64_t bit_len = (uint64_t)msg.size() * 8;
    msg.push_back((char)0x80);
    while (msg.size() % 64 != 56) msg.push_back((char)0x00);
    for (int i = 7; i >= 0; --i) msg.push_back((char)((bit_len >> (i * 8)) & 0xFF));

    // 处理每个 512-bit 块
    for (size_t offset = 0; offset < msg.size(); offset += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = ((uint32_t)(uint8_t)msg[offset + i*4] << 24) |
                   ((uint32_t)(uint8_t)msg[offset + i*4 + 1] << 16) |
                   ((uint32_t)(uint8_t)msg[offset + i*4 + 2] << 8) |
                   ((uint32_t)(uint8_t)msg[offset + i*4 + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i-15], 7) ^ rotr(w[i-15], 18) ^ (w[i-15] >> 3);
            uint32_t s1 = rotr(w[i-2], 17) ^ rotr(w[i-2], 19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }

        uint32_t a=h[0], b=h[1], c=h[2], d=h[3], e=h[4], f=h[5], g=h[6], hh=h[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t temp1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = S0 + maj;
            hh=g; g=f; f=e; e=d+temp1; d=c; c=b; b=a; a=temp1+temp2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }

    // 输出十六进制
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 8; ++i) oss << std::setw(8) << h[i];
    return oss.str();
}

} // namespace sha256_impl

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
