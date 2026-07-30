// ============================================================================
// cli_command.cpp - 单次命令执行模式实现
// 功能: 解析命令行参数并执行单次任务, 输出 JSON 结果到 stdout
// ============================================================================

#include "cli_command.h"
#include "logger.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <csignal>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

// ============================================================================
// P04-004: 全局取消信号支持
// 通过全局指针在 SIGINT/Ctrl+C 信号处理器中调用 orch->request_cancel()
// 注意: 信号处理器中只能调用 async-signal-safe 函数, atomic store 是安全的
// ============================================================================
static std::atomic<Orchestrator*> g_active_orchestrator{nullptr};
static std::atomic<bool> g_cancel_on_signal_enabled{false};

// P04-004: SIGINT 信号处理器 (Ctrl+C)
// 设置 cancel_token_, 让正在执行的 stage 在下一个检查点停止
// 注意: 不能用 extern "C" + static 组合 (C++17 禁止在 linkage specification 中使用 static)
static void p04004_sigint_handler(int sig) {
    (void)sig;
    Orchestrator* orch = g_active_orchestrator.load(std::memory_order_acquire);
    if (orch != nullptr && g_cancel_on_signal_enabled.load(std::memory_order_acquire)) {
        orch->request_cancel();
    }
}

// P04-004: 注册信号处理器 (在 stage1/stage2 执行前调用)
// orch: 当前命令使用的 Orchestrator 实例
// enable_cancel_on_signal: 是否启用 --cancel-on-signal
static void p04004_register_signal_handler(Orchestrator* orch, bool enable_cancel_on_signal) {
    g_active_orchestrator.store(orch, std::memory_order_release);
    g_cancel_on_signal_enabled.store(enable_cancel_on_signal, std::memory_order_release);
    if (enable_cancel_on_signal) {
        std::signal(SIGINT, p04004_sigint_handler);
        LOG_INFO("cli", "P04-004: --cancel-on-signal 已启用, Ctrl+C 将触发取消");
    }
}

// P04-004: 注销信号处理器 (在命令完成后调用)
static void p04004_unregister_signal_handler() {
    g_active_orchestrator.store(nullptr, std::memory_order_release);
    g_cancel_on_signal_enabled.store(false, std::memory_order_release);
    // 恢复默认 SIGINT 处理 (避免影响后续命令)
    std::signal(SIGINT, SIG_DFL);
}

// ============================================================================
// P04-001: SHA-256 纯 C++17 实现 (无外部依赖)
// 用于计算 effective_config 的 hash, 保证可追溯性
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
// P04-001: 轻量级 JSON 对象解析与合并 (无外部依赖)
// 仅处理顶层 key-value, value 可为 string/number/bool/null/object/array
// 用于配置优先级合并: default < config < overrides < cli
// ============================================================================
namespace json_merge {

// 跳过空白
inline void skip_ws(const std::string& s, size_t& pos) {
    while (pos < s.size() && (s[pos]==' '||s[pos]=='\t'||s[pos]=='\n'||s[pos]=='\r')) pos++;
}

// 提取一个 JSON 值 (从 pos 开始, 返回值的原始字符串, 含 { } [ ] " 等)
// 用于配置合并时保留 value 的原始格式
std::string extract_value(const std::string& s, size_t& pos) {
    skip_ws(s, pos);
    if (pos >= s.size()) return "";
    char c = s[pos];
    if (c == '"') {
        // 字符串
        size_t start = pos;
        pos++;
        while (pos < s.size() && s[pos] != '"') {
            if (s[pos] == '\\' && pos + 1 < s.size()) pos += 2;
            else pos++;
        }
        if (pos < s.size()) pos++; // 跳过结束 "
        return s.substr(start, pos - start);
    }
    if (c == '{' || c == '[') {
        // 嵌套对象/数组, 配对括号
        char open = c, close = (c == '{') ? '}' : ']';
        int depth = 0;
        size_t start = pos;
        while (pos < s.size()) {
            if (s[pos] == '"') {
                pos++;
                while (pos < s.size() && s[pos] != '"') {
                    if (s[pos] == '\\' && pos + 1 < s.size()) pos += 2;
                    else pos++;
                }
                if (pos < s.size()) pos++;
                continue;
            }
            if (s[pos] == open) depth++;
            else if (s[pos] == close) { depth--; if (depth == 0) { pos++; break; } }
            pos++;
        }
        return s.substr(start, pos - start);
    }
    // 数字/true/false/null
    size_t start = pos;
    while (pos < s.size() && s[pos]!=',' && s[pos]!='}' && s[pos]!=']' &&
           s[pos]!=' ' && s[pos]!='\t' && s[pos]!='\n' && s[pos]!='\r') {
        pos++;
    }
    return s.substr(start, pos - start);
}

// 解析 JSON 对象为有序 key-value 列表 (保留出现顺序)
// value 为原始字符串 (含引号, 用于后续序列化)
struct JsonField {
    std::string key;        // 不含引号
    std::string raw_value;  // 原始值 (字符串含引号, 数字/bool 不含)
};
std::vector<JsonField> parse_object(const std::string& s) {
    std::vector<JsonField> result;
    size_t pos = 0;
    skip_ws(s, pos);
    if (pos >= s.size() || s[pos] != '{') return result;
    pos++; // 跳过 {
    while (pos < s.size()) {
        skip_ws(s, pos);
        if (pos >= s.size() || s[pos] == '}') { pos++; break; }
        if (s[pos] == ',') { pos++; continue; }

        // 解析 key (字符串)
        if (s[pos] != '"') { pos++; continue; }
        size_t key_start = pos + 1;
        pos++;
        std::string key;
        while (pos < s.size() && s[pos] != '"') {
            if (s[pos] == '\\' && pos + 1 < s.size()) { key += s[pos+1]; pos += 2; }
            else { key += s[pos]; pos++; }
        }
        if (pos < s.size()) pos++; // 跳过结束 "
        skip_ws(s, pos);
        if (pos < s.size() && s[pos] == ':') pos++;
        skip_ws(s, pos);

        // 解析 value
        std::string val = extract_value(s, pos);
        result.push_back({key, val});
    }
    return result;
}

// 查找字段 (返回指针, 未找到返回 nullptr)
JsonField* find_field(std::vector<JsonField>& fields, const std::string& key) {
    for (auto& f : fields) {
        if (f.key == key) return &f;
    }
    return nullptr;
}

// 合并: 用 override 的值覆盖 base 的值 (仅顶层 key)
// 新 key 直接追加 (保留 override 顺序)
// 返回合并后的字段列表 (base 顺序 + override 新增 key)
std::vector<JsonField> merge_fields(const std::vector<JsonField>& base,
                                     const std::vector<JsonField>& override) {
    std::vector<JsonField> result = base;
    for (const auto& ov : override) {
        bool found = false;
        for (auto& r : result) {
            if (r.key == ov.key) { r.raw_value = ov.raw_value; found = true; break; }
        }
        if (!found) result.push_back(ov);
    }
    return result;
}

// 序列化为规范 JSON (key 排序, 无多余空白, 紧凑格式)
// 用于计算 SHA-256 hash, 保证相同输入产生相同 hash
std::string serialize_canonical(const std::vector<JsonField>& fields) {
    std::vector<JsonField> sorted = fields;
    std::sort(sorted.begin(), sorted.end(),
              [](const JsonField& a, const JsonField& b) { return a.key < b.key; });

    std::ostringstream oss;
    oss << "{";
    for (size_t i = 0; i < sorted.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << sorted[i].key << "\":" << sorted[i].raw_value;
    }
    oss << "}";
    return oss.str();
}

// 序列化为可读 JSON (保留顺序, 2 空格缩进)
std::string serialize_pretty(const std::vector<JsonField>& fields, int indent = 0) {
    std::ostringstream oss;
    std::string pad(indent * 2, ' ');
    std::string pad_inner((indent + 1) * 2, ' ');
    oss << pad << "{";
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\n" << pad_inner << "\"" << fields[i].key << "\": " << fields[i].raw_value;
    }
    if (!fields.empty()) oss << "\n" << pad;
    oss << "}";
    return oss.str();
}

} // namespace json_merge

// ============================================================================
// 辅助: JSON 字符串转义
// ============================================================================
static std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

// 辅助: 阶段枚举转字符串
static const char* stage_str(PipelineStage s) {
    switch (s) {
        case PipelineStage::CALIBRATE:   return "CALIBRATE";
        case PipelineStage::PLATESOLVE:  return "PLATESOLVE";
        case PipelineStage::PHOTOMETRIC: return "PHOTOMETRIC";
        case PipelineStage::DRIZZLE:     return "DRIZZLE";
        case PipelineStage::STACK:       return "STACK";
        default:                         return "UNKNOWN";
    }
}

// ============================================================================
// execute - 解析命令行参数并执行
// ============================================================================
int CliCommand::execute(int argc, char* argv[]) {
    // orchestrator --help / -h
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string sub = argv[1];
    // 转小写
    std::transform(sub.begin(), sub.end(), sub.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (sub == "--help" || sub == "-h" || sub == "help") {
        print_usage();
        return 0;
    }

    if (sub == "run") {
        // orchestrator run <fits> [--config <json>] [--threads <N>] [--fresh] [--log-level <LEVEL>]
        std::string fits_path;
        std::string config_path;
        int threads = 0;
        bool fresh = false;
        std::string log_level;

        for (int i = 2; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--config" && i + 1 < argc) {
                config_path = argv[++i];
            } else if (a == "--threads" && i + 1 < argc) {
                threads = std::atoi(argv[++i]);
            } else if (a == "--fresh") {
                fresh = true;
            } else if (a == "--log-level" && i + 1 < argc) {
                log_level = argv[++i];
            } else if (a.rfind("--", 0) == 0) {
                LOG_ERROR("cli", "未知参数: " + a);
                print_usage();
                return 1;
            } else if (fits_path.empty()) {
                fits_path = a;
            } else {
                LOG_ERROR("cli", "多余的位置参数: " + a);
                print_usage();
                return 1;
            }
        }

        if (fits_path.empty()) {
            LOG_ERROR("cli", "错误: 缺少 FITS 文件路径");
            print_usage();
            return 1;
        }
        return cmd_run(fits_path, config_path, threads, fresh, log_level);
    }

    if (sub == "run-batch") {
        // orchestrator run-batch <dir> [--config <json>] [--threads <N>] [--fresh] [--log-level <LEVEL>]
        std::string dir_path;
        std::string config_path;
        int threads = 0;
        bool fresh = false;
        std::string log_level;

        for (int i = 2; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--config" && i + 1 < argc) {
                config_path = argv[++i];
            } else if (a == "--threads" && i + 1 < argc) {
                threads = std::atoi(argv[++i]);
            } else if (a == "--fresh") {
                fresh = true;
            } else if (a == "--log-level" && i + 1 < argc) {
                log_level = argv[++i];
            } else if (a.rfind("--", 0) == 0) {
                LOG_ERROR("cli", "未知参数: " + a);
                print_usage();
                return 1;
            } else if (dir_path.empty()) {
                dir_path = a;
            } else {
                LOG_ERROR("cli", "多余的位置参数: " + a);
                print_usage();
                return 1;
            }
        }

        if (dir_path.empty()) {
            LOG_ERROR("cli", "错误: 缺少目录路径");
            print_usage();
            return 1;
        }
        return cmd_run_batch(dir_path, config_path, threads, fresh, log_level);
    }

    if (sub == "status") {
        return cmd_status();
    }

    // spec §2.3.3 stage1: 单帧预处理 (FITS -> .hiss, stage 0-7)
    if (sub == "stage1") {
        // orchestrator stage1 --frame <fits> --output <hiss>
        //     [--gaia-data <dir>] [--calibration-dir <dir>]
        //     [--filter <name>] [--config <json>] [--log-level <LEVEL>]
        //     [--request <file>] (P04-001: request JSON 模式)
        //     [--cancel-on-signal] (P04-004: Ctrl+C 时取消)
        std::string fits_path, output_hiss;
        std::string config_path, log_level;
        std::string request_path;
        // 以下参数当前仅记录日志 (后续 Task 传入 stage1_config)
        std::string gaia_data, calibration_dir, filter_name;
        bool cancel_on_signal = false;  // P04-004

        for (int i = 2; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--frame" && i + 1 < argc) {
                fits_path = argv[++i];
            } else if (a == "--output" && i + 1 < argc) {
                output_hiss = argv[++i];
            } else if (a == "--gaia-data" && i + 1 < argc) {
                gaia_data = argv[++i];
            } else if (a == "--calibration-dir" && i + 1 < argc) {
                calibration_dir = argv[++i];
            } else if (a == "--filter" && i + 1 < argc) {
                filter_name = argv[++i];
            } else if (a == "--config" && i + 1 < argc) {
                config_path = argv[++i];
            } else if (a == "--log-level" && i + 1 < argc) {
                log_level = argv[++i];
            } else if (a == "--request" && i + 1 < argc) {
                request_path = argv[++i];
            } else if (a == "--cancel-on-signal") {
                cancel_on_signal = true;  // P04-004
            } else if (a.rfind("--", 0) == 0) {
                LOG_ERROR("cli", "未知参数: " + a);
                print_usage();
                return 1;
            } else {
                LOG_ERROR("cli", "多余的位置参数: " + a);
                print_usage();
                return 1;
            }
        }

        // P04-001: --request 模式优先 (从 request JSON 读取所有参数)
        if (!request_path.empty()) {
            std::map<std::string, std::string> cli_overrides;
            if (!log_level.empty()) cli_overrides["log_level"] = "\"" + log_level + "\"";
            if (!gaia_data.empty()) cli_overrides["gaia_data_dir"] = "\"" + gaia_data + "\"";
            if (!calibration_dir.empty()) cli_overrides["calibration_dir"] = "\"" + calibration_dir + "\"";
            if (!filter_name.empty()) cli_overrides["frame.filter"] = "\"" + filter_name + "\"";
            if (cancel_on_signal) cli_overrides["cancel_on_signal"] = "true";  // P04-004
            return cmd_request(request_path, cli_overrides);
        }

        if (fits_path.empty()) {
            LOG_ERROR("cli", "错误: stage1 缺少 --frame <fits> 参数");
            print_usage();
            return 1;
        }
        if (output_hiss.empty()) {
            LOG_ERROR("cli", "错误: stage1 缺少 --output <hiss> 参数");
            print_usage();
            return 1;
        }
        // gaia_data/calibration_dir/filter_name 当前仅记录日志 (后续 Task 集成)
        if (!gaia_data.empty())        LOG_INFO("cli", "stage1 gaia-data: " + gaia_data);
        if (!calibration_dir.empty())  LOG_INFO("cli", "stage1 calibration-dir: " + calibration_dir);
        if (!filter_name.empty())      LOG_INFO("cli", "stage1 filter: " + filter_name);

        return cmd_stage1(fits_path, output_hiss, config_path, log_level, cancel_on_signal);
    }

    // spec §2.3.3 stage2: 多帧合并 (.hiss -> .hcsd, stage 8-9)
    if (sub == "stage2") {
        // orchestrator stage2 --frames <hiss_dir> --output <hcsd>
        //     [--config <json>] [--log-level <LEVEL>]
        //     [--request <file>] (P04-001: request JSON 模式)
        //     [--cancel-on-signal] (P04-004: Ctrl+C 时取消)
        std::string hiss_dir, output_hcsd;
        std::string config_path, log_level;
        std::string request_path;
        bool cancel_on_signal = false;  // P04-004

        for (int i = 2; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--frames" && i + 1 < argc) {
                hiss_dir = argv[++i];
            } else if (a == "--output" && i + 1 < argc) {
                output_hcsd = argv[++i];
            } else if (a == "--config" && i + 1 < argc) {
                config_path = argv[++i];
            } else if (a == "--log-level" && i + 1 < argc) {
                log_level = argv[++i];
            } else if (a == "--request" && i + 1 < argc) {
                request_path = argv[++i];
            } else if (a == "--cancel-on-signal") {
                cancel_on_signal = true;  // P04-004
            } else if (a.rfind("--", 0) == 0) {
                LOG_ERROR("cli", "未知参数: " + a);
                print_usage();
                return 1;
            } else {
                LOG_ERROR("cli", "多余的位置参数: " + a);
                print_usage();
                return 1;
            }
        }

        // P04-001: --request 模式优先
        if (!request_path.empty()) {
            std::map<std::string, std::string> cli_overrides;
            if (!log_level.empty()) cli_overrides["log_level"] = "\"" + log_level + "\"";
            if (cancel_on_signal) cli_overrides["cancel_on_signal"] = "true";  // P04-004
            return cmd_request(request_path, cli_overrides);
        }

        if (hiss_dir.empty()) {
            LOG_ERROR("cli", "错误: stage2 缺少 --frames <dir> 参数");
            print_usage();
            return 1;
        }
        if (output_hcsd.empty()) {
            LOG_ERROR("cli", "错误: stage2 缺少 --output <hcsd> 参数");
            print_usage();
            return 1;
        }

        return cmd_stage2(hiss_dir, output_hcsd, config_path, log_level, cancel_on_signal);
    }

    // P04-001: inspect 子命令 (检查配置, 输出 effective_config, 不执行实际任务)
    // P04-003: 扩展支持 --hiss/--hcsd/--frame 检查文件元数据
    if (sub == "inspect") {
        std::string request_path;
        std::string hiss_path;
        std::string hcsd_path;
        std::string frame_path;
        for (int i = 2; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--request" && i + 1 < argc) {
                request_path = argv[++i];
            } else if (a == "--hiss" && i + 1 < argc) {
                hiss_path = argv[++i];
            } else if (a == "--hcsd" && i + 1 < argc) {
                hcsd_path = argv[++i];
            } else if (a == "--frame" && i + 1 < argc) {
                frame_path = argv[++i];
            } else if (a.rfind("--", 0) == 0) {
                LOG_ERROR("cli", "未知参数: " + a);
                print_usage();
                return 1;
            } else {
                LOG_ERROR("cli", "多余的位置参数: " + a);
                print_usage();
                return 1;
            }
        }
        // P04-003: 互斥分发 (优先级: --hiss > --hcsd > --frame > --request)
        if (!hiss_path.empty()) {
            return cmd_inspect_hiss(hiss_path);
        }
        if (!hcsd_path.empty()) {
            return cmd_inspect_hcsd(hcsd_path);
        }
        if (!frame_path.empty()) {
            return cmd_inspect_frame(frame_path);
        }
        if (request_path.empty()) {
            LOG_ERROR("cli", "错误: inspect 缺少 --request/--hiss/--hcsd/--frame 参数");
            print_usage();
            return 7;  // CONFIG_ERROR
        }
        return cmd_inspect(request_path);
    }

    // P04-001: capabilities 子命令 (查询 CLI 支持的能力)
    if (sub == "capabilities") {
        return cmd_capabilities();
    }

    LOG_ERROR("cli", "未知子命令: " + std::string(argv[1]));
    print_usage();
    return 1;
}

// ============================================================================
// cmd_run - 单帧处理
// ============================================================================
int CliCommand::cmd_run(const std::string& fits_path,
                        const std::string& config_path,
                        int threads,
                        bool fresh,
                        const std::string& log_level) {
    Orchestrator orch;

    // 加载配置 (可选)
    if (!config_path.empty()) {
        std::string err;
        if (!orch.load_config(config_path, err)) {
            LOG_ERROR("cli", "配置加载失败: " + err);
            return 7;  // P03-003: 配置错误 (原为 2)
        }
    }

    // 设置线程数 (通过配置, 当前骨架: 不直接设置, 后续 Task 完善)
    if (threads > 0) {
        LOG_INFO("cli", "请求线程数: " + std::to_string(threads) + " (骨架: 配置传递待完善)");
    }

    // Task 3: 设置 fresh_start (忽略检查点重新开始)
    if (fresh) {
        LOG_INFO("cli", "启用 fresh_start: 忽略检查点重新开始");
        orch.set_fresh_start(true);
    }

    // Task 4: 设置日志级别 (覆盖配置中的 log_level)
    if (!log_level.empty()) {
        LogLevel lvl = Logger::string_to_level(log_level);
        Logger::instance().set_level(lvl);
        LOG_INFO("cli", "日志级别设置为: " + log_level);
    }

    TaskResult r = orch.run_single(fits_path);
    output_json_result(r);
    // P03-003: 使用 TaskResult.exit_code 传递细分错误码 (失败时若 exit_code=0 用 1 兜底)
    return r.success ? 0 : (r.exit_code != 0 ? r.exit_code : 1);
}

// ============================================================================
// cmd_run_batch - 批量处理
// ============================================================================
int CliCommand::cmd_run_batch(const std::string& dir_path,
                              const std::string& config_path,
                              int threads,
                              bool fresh,
                              const std::string& log_level) {
    Orchestrator orch;

    if (!config_path.empty()) {
        std::string err;
        if (!orch.load_config(config_path, err)) {
            LOG_ERROR("cli", "配置加载失败: " + err);
            return 7;  // P03-003: 配置错误 (原为 2)
        }
    }

    // 检查目录是否存在 (Task 5: 集成测试要求 nonexistent_dir 返回错误)
    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        LOG_ERROR("cli", "目录不存在或不是目录: " + dir_path);
        std::cout << "{" << std::endl;
        std::cout << "  \"total\": 0," << std::endl;
        std::cout << "  \"success_count\": 0," << std::endl;
        std::cout << "  \"failed_count\": 0," << std::endl;
        std::cout << "  \"results\": []" << std::endl;
        std::cout << "}" << std::endl;
        return 8;  // P03-003: 文件 I/O 错误 (原为 4)
    }

    if (threads > 0) {
        LOG_INFO("cli", "请求线程数: " + std::to_string(threads) + " (骨架: 配置传递待完善)");
    }

    // Task 3: 设置 fresh_start (忽略检查点重新开始)
    if (fresh) {
        LOG_INFO("cli", "启用 fresh_start: 忽略检查点重新开始");
        orch.set_fresh_start(true);
    }

    // Task 4: 设置日志级别 (覆盖配置中的 log_level)
    if (!log_level.empty()) {
        LogLevel lvl = Logger::string_to_level(log_level);
        Logger::instance().set_level(lvl);
        LOG_INFO("cli", "日志级别设置为: " + log_level);
    }

    std::vector<TaskResult> results = orch.run_batch(dir_path);
    output_json_batch(results);

    // P03-003: 任意失败则返回非零 (优先使用 TaskResult.exit_code, 否则用 1)
    for (const auto& r : results) {
        if (!r.success) return r.exit_code != 0 ? r.exit_code : 1;
    }
    return 0;
}

// ============================================================================
// cmd_status - 状态查询 (单次模式下无运行实例, 仅输出提示)
// ============================================================================
int CliCommand::cmd_status() {
    std::cout << "{" << std::endl;
    std::cout << "  \"mode\": \"single-command\"," << std::endl;
    std::cout << "  \"status\": \"no running instance\"" << std::endl;
    std::cout << "}" << std::endl;
    return 0;
}

// ============================================================================
// cmd_stage1 - spec §2.3.3 单帧预处理 (FITS -> .hiss, stage 0-7)
// cancel_on_signal: P04-004 - true 时注册 SIGINT 处理器触发取消
// ============================================================================
int CliCommand::cmd_stage1(const std::string& fits_path,
                           const std::string& output_hiss,
                           const std::string& config_path,
                           const std::string& log_level,
                           bool cancel_on_signal) {
    Orchestrator orch;

    // P04-004: 注册 SIGINT 处理器 (如启用 --cancel-on-signal)
    p04004_register_signal_handler(&orch, cancel_on_signal);

    // 加载配置 (可选, 后续 Task 解析 stage1_config.json 各字段)
    std::string config_json;
    if (!config_path.empty()) {
        std::string err;
        if (!orch.load_config(config_path, err)) {
            LOG_ERROR("cli", "配置加载失败: " + err);
            p04004_unregister_signal_handler();
            return 7;  // P03-003: 配置错误 (原为 2)
        }
        // 读取配置文件原始内容作为 config_json 传给 run_stage1
        std::ifstream ifs(config_path, std::ios::binary);
        if (ifs.is_open()) {
            std::stringstream ss;
            ss << ifs.rdbuf();
            config_json = ss.str();
        }
    }

    // 设置日志级别 (覆盖配置中的 log_level)
    if (!log_level.empty()) {
        LogLevel lvl = Logger::string_to_level(log_level);
        Logger::instance().set_level(lvl);
        LOG_INFO("cli", "日志级别设置为: " + log_level);
    }

    TaskResult r = orch.run_stage1(fits_path, output_hiss, config_json);
    p04004_unregister_signal_handler();
    output_json_result(r);
    // P03-003: 使用 TaskResult.exit_code 传递细分错误码 (失败时若 exit_code=0 用 1 兜底)
    return r.success ? 0 : (r.exit_code != 0 ? r.exit_code : 1);
}

// ============================================================================
// cmd_stage2 - spec §2.3.3 多帧合并 (.hiss -> .hcsd, stage 8-9)
// cancel_on_signal: P04-004 - true 时注册 SIGINT 处理器触发取消
// ============================================================================
int CliCommand::cmd_stage2(const std::string& hiss_dir,
                           const std::string& output_hcsd,
                           const std::string& config_path,
                           const std::string& log_level,
                           bool cancel_on_signal) {
    Orchestrator orch;

    // P04-004: 注册 SIGINT 处理器 (如启用 --cancel-on-signal)
    p04004_register_signal_handler(&orch, cancel_on_signal);

    // 加载配置 (可选, 后续 Task 解析 stage2_config.json 各字段)
    std::string config_json;
    if (!config_path.empty()) {
        std::string err;
        if (!orch.load_config(config_path, err)) {
            LOG_ERROR("cli", "配置加载失败: " + err);
            p04004_unregister_signal_handler();
            return 7;  // P03-003: 配置错误 (原为 2)
        }
        std::ifstream ifs(config_path, std::ios::binary);
        if (ifs.is_open()) {
            std::stringstream ss;
            ss << ifs.rdbuf();
            config_json = ss.str();
        }
    }

    // 设置日志级别
    if (!log_level.empty()) {
        LogLevel lvl = Logger::string_to_level(log_level);
        Logger::instance().set_level(lvl);
        LOG_INFO("cli", "日志级别设置为: " + log_level);
    }

    TaskResult r = orch.run_stage2(hiss_dir, output_hcsd, config_json);
    p04004_unregister_signal_handler();
    output_json_result(r);
    // P03-003: 使用 TaskResult.exit_code 传递细分错误码 (失败时若 exit_code=0 用 1 兜底)
    return r.success ? 0 : (r.exit_code != 0 ? r.exit_code : 1);
}

// ============================================================================
// output_json_result - 输出单帧 JSON 结果
// ============================================================================
void CliCommand::output_json_result(const TaskResult& result) {
    std::cout << "{" << std::endl;
    std::cout << "  \"success\": " << (result.success ? "true" : "false") << "," << std::endl;
    std::cout << "  \"frame_name\": \"" << json_escape(result.frame_name) << "\"," << std::endl;

    // timings 数组
    std::cout << "  \"timings\": [" << std::endl;
    for (size_t i = 0; i < result.timings.size(); ++i) {
        const auto& t = result.timings[i];
        std::cout << "    {\"stage\": \"" << stage_str(t.stage)
                  << "\", \"name\": \"" << json_escape(t.stage_name)
                  << "\", \"duration_sec\": " << t.duration_sec
                  << ", \"success\": " << (t.success ? "true" : "false") << "}";
        if (i + 1 < result.timings.size()) std::cout << ",";
        std::cout << std::endl;
    }
    std::cout << "  ]," << std::endl;

    // wcs_fields 对象
    std::cout << "  \"wcs_fields\": {";
    bool first = true;
    for (const auto& kv : result.wcs_fields) {
        if (!first) std::cout << ", ";
        std::cout << "\"" << json_escape(kv.first) << "\": \""
                  << json_escape(kv.second) << "\"";
        first = false;
    }
    std::cout << "}," << std::endl;

    // photo_stats 对象
    std::cout << "  \"photo_stats\": {";
    first = true;
    for (const auto& kv : result.photo_stats) {
        if (!first) std::cout << ", ";
        std::cout << "\"" << json_escape(kv.first) << "\": \""
                  << json_escape(kv.second) << "\"";
        first = false;
    }
    std::cout << "}," << std::endl;

    std::cout << "  \"output_ahpx_path\": \"" << json_escape(result.output_ahpx_path) << "\"," << std::endl;
    std::cout << "  \"error_msg\": \"" << json_escape(result.error_msg) << "\"" << std::endl;
    std::cout << "}" << std::endl;
}

// ============================================================================
// output_json_batch - 输出批量 JSON 结果
// ============================================================================
void CliCommand::output_json_batch(const std::vector<TaskResult>& results) {
    size_t n_ok = 0;
    for (const auto& r : results) {
        if (r.success) ++n_ok;
    }

    std::cout << "{" << std::endl;
    std::cout << "  \"total\": " << results.size() << "," << std::endl;
    std::cout << "  \"success_count\": " << n_ok << "," << std::endl;
    std::cout << "  \"failed_count\": " << (results.size() - n_ok) << "," << std::endl;
    std::cout << "  \"results\": [" << std::endl;
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        std::cout << "    {\"success\": " << (r.success ? "true" : "false")
                  << ", \"frame_name\": \"" << json_escape(r.frame_name) << "\""
                  << ", \"error_msg\": \"" << json_escape(r.error_msg) << "\"}";
        if (i + 1 < results.size()) std::cout << ",";
        std::cout << std::endl;
    }
    std::cout << "  ]" << std::endl;
    std::cout << "}" << std::endl;
}

// ============================================================================
// P04-001: 内置默认配置 (stage1/stage2 通用)
// 优先级最低, 被 config/overrides/cli 逐层覆盖
// ============================================================================
static const char* BUILTIN_DEFAULT_CONFIG =
    "{"
    "\"gaia_data_dir\":\"GaiaDR3SP\","
    "\"calibration_dir\":\"testdata/T4 calibration files\","
    "\"output_root\":\"output\","
    "\"frame\":{\"filter\":\"\",\"qe_curve\":\"\"},"
    "\"calibration\":{\"require_size_match\":true,\"require_exposure_match\":true,"
    "\"exposure_tolerance_s\":0.5,\"allow_no_calibration\":false,"
    "\"dark_optimization\":0,\"dark_scale_factor\":1.0},"
    "\"platesolve\":{\"focal_length\":0.0,\"pixel_size\":0.0,\"max_stars\":2000},"
    "\"psf\":{\"fit_radius\":8,\"max_iter\":100,\"tolerance\":1.0e-6,\"max_stars\":2000},"
    "\"photometric\":{\"mag_min\":6.0,\"mag_max\":16.0,\"fov_radius_deg\":0.0},"
    "\"drizzle\":{\"nside_strategy\":\"1x_to_2x_drizzle\",\"nside_override\":0,"
    "\"pixfrac\":1.0,\"nested\":true},"
    "\"log_level\":\"INFO\","
    "\"threads\":0"
    "}";

// P04-001: 获取当前 UTC 时间 ISO 8601 字符串
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

// P04-001: 生成 job_id (时间戳 + 随机后缀)
static std::string generate_job_id() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return "job_" + std::to_string(ms);
}

// ============================================================================
// P04-001: compute_effective_config - 计算有效配置
// 配置优先级 (低到高): default < config < overrides < cli
// 返回 EffectiveConfig (含 SHA-256 hash)
// ============================================================================
EffectiveConfig CliCommand::compute_effective_config(
    const std::string& command,
    const std::string& job_id,
    const std::string& config_source,
    const std::string& overrides_json,
    const std::map<std::string, std::string>& cli_overrides,
    const std::string& request_path,
    const std::string& config_path) {

    EffectiveConfig ec;
    ec.command = command;
    ec.job_id = job_id.empty() ? generate_job_id() : job_id;
    ec.created_at = get_utc_timestamp();
    ec.request_path = request_path;
    ec.config_path = config_path;

    // 1. 从内置默认配置开始
    std::vector<json_merge::JsonField> fields =
        json_merge::parse_object(BUILTIN_DEFAULT_CONFIG);

    // 记录 default 来源
    for (const auto& f : fields) {
        ec.sources[f.key] = "default";
    }

    // 2. 合并 config (文件路径或内联 JSON)
    std::string config_json_str;
    std::string actual_config_path = config_path;
    if (!config_source.empty()) {
        // 判断是文件路径还是内联 JSON
        bool is_file = false;
        if (config_source.size() > 0 && config_source[0] != '{') {
            // 可能是文件路径
            if (fs::exists(config_source)) {
                std::ifstream ifs(config_source, std::ios::binary);
                if (ifs.is_open()) {
                    std::stringstream ss;
                    ss << ifs.rdbuf();
                    config_json_str = ss.str();
                    is_file = true;
                    if (actual_config_path.empty()) actual_config_path = config_source;
                }
            }
        }
        if (!is_file) {
            // 内联 JSON (以 { 开头)
            config_json_str = config_source;
        }

        if (!config_json_str.empty()) {
            std::vector<json_merge::JsonField> config_fields =
                json_merge::parse_object(config_json_str);
            for (const auto& cf : config_fields) {
                ec.sources[cf.key] = "config";
            }
            fields = json_merge::merge_fields(fields, config_fields);
        }
    }
    ec.config_path = actual_config_path;

    // 3. 合并 overrides (request.overrides JSON 对象)
    if (!overrides_json.empty()) {
        std::vector<json_merge::JsonField> override_fields =
            json_merge::parse_object(overrides_json);
        for (const auto& of : override_fields) {
            ec.sources[of.key] = "overrides";
        }
        fields = json_merge::merge_fields(fields, override_fields);
    }

    // 4. 合并 CLI 覆盖 (最高优先级)
    for (const auto& cli : cli_overrides) {
        // cli.first 是 key (如 "log_level", "frame.filter")
        // cli.second 是 raw_value (如 "\"DEBUG\"", "512")
        // 简化处理: 仅支持顶层 key (frame.filter 等嵌套路径记入 sources 但不实际覆盖嵌套对象)
        std::string key = cli.first;
        // 处理嵌套路径 (如 frame.filter -> 提取 frame 顶层 key)
        size_t dot = key.find('.');
        if (dot != std::string::npos) {
            // 嵌套路径, 记录来源但不实际覆盖 (避免破坏嵌套对象结构)
            ec.sources[key] = "cli";
        } else {
            // 顶层 key
            json_merge::JsonField* existing = json_merge::find_field(fields, key);
            if (existing) {
                existing->raw_value = cli.second;
            } else {
                fields.push_back({key, cli.second});
            }
            ec.sources[key] = "cli";
        }
    }

    // 5. 生成规范 JSON (key 排序) 并计算 SHA-256
    ec.config_json = json_merge::serialize_canonical(fields);
    ec.effective_config_hash = sha256_impl::sha256(ec.config_json);

    return ec;
}

// ============================================================================
// P04-001: output_jsonl_event - 输出 JSONL 事件到 stdout
// 每行一个 JSON 事件, 供机器消费
// ============================================================================
void CliCommand::output_jsonl_event(const std::string& event_type,
                                    const std::string& job_id,
                                    const std::string& stage,
                                    double progress,
                                    const std::string& message,
                                    const std::string& result_json,
                                    const std::string& error_json) {
    std::cout << "{";
    std::cout << "\"schema_version\":1,";
    std::cout << "\"type\":\"" << json_escape(event_type) << "\",";
    std::cout << "\"job_id\":\"" << json_escape(job_id) << "\",";
    std::cout << "\"timestamp\":\"" << get_utc_timestamp() << "\"";
    if (!stage.empty()) {
        std::cout << ",\"stage\":\"" << json_escape(stage) << "\"";
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
    std::cout << "}" << std::endl;
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

// ============================================================================
// P04-001: cmd_inspect - 检查配置, 输出 effective_config (不执行实际任务)
// stdout 输出 effective_config JSON, stderr 输出日志
// P04-002: 错误路径输出 JSONL error + failed 事件 (含数字 exit_code)
// ============================================================================
int CliCommand::cmd_inspect(const std::string& request_path) {
    LOG_INFO("cli", "inspect: 检查配置 (不执行实际任务)");

    // 读取 request JSON
    if (!fs::exists(request_path)) {
        LOG_ERROR("cli", "request 文件不存在: " + request_path);
        std::string err_json = "{\"code\":\"ASTROCS_INPUT_INVALID\",\"numeric_code\":8,\"message\":\"request file not found\"}";
        output_jsonl_event_ex("error", "", "", -1.0, "", "", err_json,
                              AstroCsExitCode::FILE_IO_ERROR);
        output_jsonl_event("failed", "", "", -1.0, "", "", err_json);
        return AstroCsExitCode::FILE_IO_ERROR;
    }
    std::ifstream ifs(request_path, std::ios::binary);
    if (!ifs.is_open()) {
        LOG_ERROR("cli", "无法打开 request 文件: " + request_path);
        std::string err_json = "{\"code\":\"ASTROCS_INPUT_INVALID\",\"numeric_code\":8,\"message\":\"cannot open request file\"}";
        output_jsonl_event_ex("error", "", "", -1.0, "", "", err_json,
                              AstroCsExitCode::FILE_IO_ERROR);
        output_jsonl_event("failed", "", "", -1.0, "", "", err_json);
        return AstroCsExitCode::FILE_IO_ERROR;
    }
    std::stringstream ss;
    ss << ifs.rdbuf();
    std::string request_json = ss.str();

    // 解析 request JSON (用 json_merge::parse_object 提取顶层字段)
    std::vector<json_merge::JsonField> req_fields =
        json_merge::parse_object(request_json);

    std::string command, job_id, config_source, overrides_json;
    for (const auto& f : req_fields) {
        if (f.key == "command") {
            // 去除引号
            if (f.raw_value.size() >= 2 && f.raw_value[0] == '"') {
                command = f.raw_value.substr(1, f.raw_value.size() - 2);
            } else {
                command = f.raw_value;
            }
        } else if (f.key == "job_id") {
            if (f.raw_value.size() >= 2 && f.raw_value[0] == '"') {
                job_id = f.raw_value.substr(1, f.raw_value.size() - 2);
            }
        } else if (f.key == "config") {
            config_source = f.raw_value;
        } else if (f.key == "overrides") {
            overrides_json = f.raw_value;
        }
    }

    if (command.empty()) {
        LOG_ERROR("cli", "request JSON 缺少 command 字段");
        std::string err_json = "{\"code\":\"ASTROCS_CONFIG_INVALID\",\"numeric_code\":7,\"message\":\"missing command field\"}";
        output_jsonl_event_ex("error", job_id, "", -1.0, "", "", err_json,
                              AstroCsExitCode::CONFIG_ERROR);
        output_jsonl_event("failed", job_id, "", -1.0, "", "", err_json);
        return AstroCsExitCode::CONFIG_ERROR;
    }

    // 计算有效配置
    std::map<std::string, std::string> empty_cli;
    EffectiveConfig ec = compute_effective_config(
        command, job_id, config_source, overrides_json, empty_cli, request_path);

    // 输出 effective_config 到 stdout (可读 JSON 格式)
    std::cout << "{" << std::endl;
    std::cout << "  \"schema_version\": 1," << std::endl;
    std::cout << "  \"command\": \"" << json_escape(ec.command) << "\"," << std::endl;
    std::cout << "  \"job_id\": \"" << json_escape(ec.job_id) << "\"," << std::endl;
    std::cout << "  \"effective_config_hash\": \"" << ec.effective_config_hash << "\"," << std::endl;
    std::cout << "  \"created_at\": \"" << ec.created_at << "\"," << std::endl;
    if (!ec.request_path.empty()) {
        std::cout << "  \"request_path\": \"" << json_escape(ec.request_path) << "\"," << std::endl;
    }
    if (!ec.config_path.empty()) {
        std::cout << "  \"config_path\": \"" << json_escape(ec.config_path) << "\"," << std::endl;
    }
    // sources 对象
    std::cout << "  \"sources\": {";
    bool first = true;
    for (const auto& kv : ec.sources) {
        if (!first) std::cout << ", ";
        std::cout << "\"" << json_escape(kv.first) << "\": \"" << kv.second << "\"";
        first = false;
    }
    std::cout << "}," << std::endl;
    // config 对象 (规范 JSON, 紧凑)
    std::cout << "  \"config\": " << ec.config_json << std::endl;
    std::cout << "}" << std::endl;

    LOG_INFO("cli", "inspect: effective_config_hash=" + ec.effective_config_hash);
    return AstroCsExitCode::SUCCESS;
}

// ============================================================================
// P04-003: cmd_inspect_hiss - 检查 HISS 文件元数据
// 优先调用 AIO DLL 的 aio_hiss_read 获取完整元数据;
// DLL 不可用时降级读取二进制头 (magic + 长度前缀)
// stdout 输出 JSONL 事件 (result 事件含元数据), stderr 输出日志
// ============================================================================
int CliCommand::cmd_inspect_hiss(const std::string& hiss_path) {
    LOG_INFO("cli", "inspect --hiss: " + hiss_path);

    // 文件存在性检查
    if (!fs::exists(hiss_path)) {
        LOG_ERROR("cli", "HISS 文件不存在: " + hiss_path);
        std::string err_json = "{\"code\":\"ASTROCS_FILE_IO_ERROR\",\"numeric_code\":8,\"message\":\"hiss file not found\"}";
        output_jsonl_event_ex("error", "", "", -1.0, "", "", err_json,
                              AstroCsExitCode::FILE_IO_ERROR);
        output_jsonl_event("failed", "", "", -1.0, "", "", err_json);
        return AstroCsExitCode::FILE_IO_ERROR;
    }

    // 读取文件大小
    uintmax_t file_size = fs::file_size(hiss_path);
    LOG_INFO("cli", "HISS 文件大小: " + std::to_string(file_size) + " 字节");

    // 读取二进制头 (前 12 字节: magic + uncomp_json_len + comp_json_len)
    std::ifstream ifs(hiss_path, std::ios::binary);
    if (!ifs.is_open()) {
        LOG_ERROR("cli", "无法打开 HISS 文件: " + hiss_path);
        std::string err_json = "{\"code\":\"ASTROCS_FILE_IO_ERROR\",\"numeric_code\":8,\"message\":\"cannot open hiss file\"}";
        output_jsonl_event_ex("error", "", "", -1.0, "", "", err_json,
                              AstroCsExitCode::FILE_IO_ERROR);
        output_jsonl_event("failed", "", "", -1.0, "", "", err_json);
        return AstroCsExitCode::FILE_IO_ERROR;
    }

    char magic[4] = {0};
    ifs.read(magic, 4);
    uint32_t uncomp_json_len = 0, comp_json_len = 0;
    ifs.read(reinterpret_cast<char*>(&uncomp_json_len), 4);
    ifs.read(reinterpret_cast<char*>(&comp_json_len), 4);
    ifs.close();

    bool magic_ok = (magic[0] == 'H' && magic[1] == 'I' &&
                     magic[2] == 'S' && magic[3] == 'S');
    if (!magic_ok) {
        LOG_ERROR("cli", "HISS magic 不匹配: " + std::string(magic, 4));
        std::string err_json = "{\"code\":\"ASTROCS_HISS_INVALID\",\"numeric_code\":25,\"message\":\"invalid HISS magic\"}";
        output_jsonl_event_ex("error", "", "", -1.0, "", "", err_json,
                              AstroCsExitCode::HISS_INVALID);
        output_jsonl_event("failed", "", "", -1.0, "", "", err_json);
        return AstroCsExitCode::HISS_INVALID;
    }

    // 尝试加载 AIO DLL 获取完整元数据
    std::string nside_str = "unknown";
    std::string nested_str = "unknown";
    std::string n_pix_str = "unknown";
    std::string has_snr_str = "unknown";
    std::string snr_format_str = "unknown";
    std::string snr_n_points_str = "unknown";
    std::string meta_json_str = "{}";

    {
        Orchestrator orch;
        std::string err;
        Logger::instance().set_stderr_output(false);
        bool ok = orch.init_dlls("", err);
        Logger::instance().set_stderr_output(true);
        if (ok && orch.get_dll_loader().is_loaded(ModuleId::AIO)) {
            DllLoader& loader = orch.get_dll_loader();
            using HisReadFn = int (*)(const char*, uint32_t*, int*,
                                       uint64_t*, uint64_t**, float**, float**, char**);
            using FreeFn = void (*)(void*);
            auto fn_read = loader.get_function<HisReadFn>(ModuleId::AIO, "aio_hiss_read");
            auto fn_free = loader.get_function<FreeFn>(ModuleId::AIO, "aio_hio_free");
            if (fn_read && fn_free) {
                uint32_t nside = 0;
                int nested = 0;
                uint64_t n_pix = 0;
                uint64_t* ipix = nullptr;
                float* pixel = nullptr;
                float* snr = nullptr;
                char* meta_json = nullptr;
                int ret = fn_read(hiss_path.c_str(), &nside, &nested, &n_pix,
                                   &ipix, &pixel, &snr, &meta_json);
                if (ret == 0) {
                    nside_str = std::to_string(nside);
                    nested_str = nested ? "true" : "false";
                    n_pix_str = std::to_string(n_pix);
                    if (meta_json) {
                        meta_json_str = std::string(meta_json);
                        // 解析 has_snr / snr_format / snr_n_points
                        std::vector<json_merge::JsonField> fields =
                            json_merge::parse_object(meta_json_str);
                        for (const auto& f : fields) {
                            if (f.key == "has_snr") has_snr_str = f.raw_value;
                            else if (f.key == "snr_format") snr_format_str = f.raw_value;
                            else if (f.key == "snr_n_points") snr_n_points_str = f.raw_value;
                        }
                    }
                    // 释放内存
                    if (ipix) fn_free(ipix);
                    if (pixel) fn_free(pixel);
                    if (snr) fn_free(snr);
                    if (meta_json) fn_free(meta_json);
                    LOG_INFO("cli", "HISS 元数据读取成功: nside=" + nside_str +
                             " n_pix=" + n_pix_str);
                } else {
                    LOG_WARN("cli", "aio_hiss_read 返回错误: " + std::to_string(ret) +
                             " (仅输出二进制头元数据)");
                }
            } else {
                LOG_WARN("cli", "AIO DLL 未导出 aio_hiss_read/aio_hio_free (仅输出二进制头元数据)");
            }
        } else {
            LOG_WARN("cli", "AIO DLL 未加载 (仅输出二进制头元数据)");
        }
    }

    // 输出 JSONL result 事件 (含 HISS 元数据)
    std::ostringstream result_oss;
    result_oss << "{\"file\":\"" << json_escape(hiss_path) << "\""
               << ",\"format\":\"HISS\""
               << ",\"file_size\":" << file_size
               << ",\"magic\":\"HISS\""
               << ",\"uncomp_json_len\":" << uncomp_json_len
               << ",\"comp_json_len\":" << comp_json_len
               << ",\"nside\":" << nside_str
               << ",\"nested\":" << nested_str
               << ",\"n_pix\":" << n_pix_str
               << ",\"has_snr\":" << has_snr_str
               << ",\"snr_format\":" << snr_format_str
               << ",\"snr_n_points\":" << snr_n_points_str
               << ",\"meta_json\":" << meta_json_str
               << "}";
    output_jsonl_event_ex("result", "", "", 1.0, "hiss inspect completed",
                          result_oss.str(), "", -1, -1.0, "ok");
    output_jsonl_event("job_completed", "", "", 1.0, "hiss inspect completed", result_oss.str());
    return AstroCsExitCode::SUCCESS;
}

// ============================================================================
// P04-003: cmd_inspect_hcsd - 检查 HCSD 文件元数据
// 优先调用 AIO DLL 的 aio_hcsd_read 获取完整元数据;
// DLL 不可用时降级读取二进制头 (magic + 长度前缀)
// stdout 输出 JSONL 事件 (result 事件含元数据), stderr 输出日志
// ============================================================================
int CliCommand::cmd_inspect_hcsd(const std::string& hcsd_path) {
    LOG_INFO("cli", "inspect --hcsd: " + hcsd_path);

    // 文件存在性检查
    if (!fs::exists(hcsd_path)) {
        LOG_ERROR("cli", "HCSD 文件不存在: " + hcsd_path);
        std::string err_json = "{\"code\":\"ASTROCS_FILE_IO_ERROR\",\"numeric_code\":8,\"message\":\"hcsd file not found\"}";
        output_jsonl_event_ex("error", "", "", -1.0, "", "", err_json,
                              AstroCsExitCode::FILE_IO_ERROR);
        output_jsonl_event("failed", "", "", -1.0, "", "", err_json);
        return AstroCsExitCode::FILE_IO_ERROR;
    }

    // 读取文件大小
    uintmax_t file_size = fs::file_size(hcsd_path);
    LOG_INFO("cli", "HCSD 文件大小: " + std::to_string(file_size) + " 字节");

    // 读取二进制头 (前 12 字节: magic + uncomp_json_len + comp_json_len)
    std::ifstream ifs(hcsd_path, std::ios::binary);
    if (!ifs.is_open()) {
        LOG_ERROR("cli", "无法打开 HCSD 文件: " + hcsd_path);
        std::string err_json = "{\"code\":\"ASTROCS_FILE_IO_ERROR\",\"numeric_code\":8,\"message\":\"cannot open hcsd file\"}";
        output_jsonl_event_ex("error", "", "", -1.0, "", "", err_json,
                              AstroCsExitCode::FILE_IO_ERROR);
        output_jsonl_event("failed", "", "", -1.0, "", "", err_json);
        return AstroCsExitCode::FILE_IO_ERROR;
    }

    char magic[4] = {0};
    ifs.read(magic, 4);
    uint32_t uncomp_json_len = 0, comp_json_len = 0;
    ifs.read(reinterpret_cast<char*>(&uncomp_json_len), 4);
    ifs.read(reinterpret_cast<char*>(&comp_json_len), 4);
    ifs.close();

    bool magic_ok = (magic[0] == 'H' && magic[1] == 'C' &&
                     magic[2] == 'S' && magic[3] == 'D');
    if (!magic_ok) {
        LOG_ERROR("cli", "HCSD magic 不匹配: " + std::string(magic, 4));
        std::string err_json = "{\"code\":\"ASTROCS_HCSD_INVALID\",\"numeric_code\":26,\"message\":\"invalid HCSD magic\"}";
        output_jsonl_event_ex("error", "", "", -1.0, "", "", err_json,
                              AstroCsExitCode::HCSD_INVALID);
        output_jsonl_event("failed", "", "", -1.0, "", "", err_json);
        return AstroCsExitCode::HCSD_INVALID;
    }

    // 尝试加载 AIO DLL 获取完整元数据
    std::string nside_str = "unknown";
    std::string nested_str = "unknown";
    std::string n_pix_str = "unknown";
    std::string meta_json_str = "{}";
    // HCSD leaf_index 固定 49152 项 (12 * 64^2)
    constexpr uint32_t HCSD_N_LEAVES = 49152;
    constexpr uint64_t HCSD_LEAF_INDEX_BYTES = static_cast<uint64_t>(HCSD_N_LEAVES) * 24;

    {
        Orchestrator orch;
        std::string err;
        Logger::instance().set_stderr_output(false);
        bool ok = orch.init_dlls("", err);
        Logger::instance().set_stderr_output(true);
        if (ok && orch.get_dll_loader().is_loaded(ModuleId::AIO)) {
            DllLoader& loader = orch.get_dll_loader();
            using HcsdReadFn = int (*)(const char*, uint32_t*, int*,
                                        uint64_t*, uint64_t**, float**, char**);
            using FreeFn = void (*)(void*);
            auto fn_read = loader.get_function<HcsdReadFn>(ModuleId::AIO, "aio_hcsd_read");
            auto fn_free = loader.get_function<FreeFn>(ModuleId::AIO, "aio_hio_free");
            if (fn_read && fn_free) {
                uint32_t nside = 0;
                int nested = 0;
                uint64_t n_pix = 0;
                uint64_t* ipix = nullptr;
                float* pixel = nullptr;
                char* meta_json = nullptr;
                int ret = fn_read(hcsd_path.c_str(), &nside, &nested, &n_pix,
                                   &ipix, &pixel, &meta_json);
                if (ret == 0) {
                    nside_str = std::to_string(nside);
                    nested_str = nested ? "true" : "false";
                    n_pix_str = std::to_string(n_pix);
                    if (meta_json) {
                        meta_json_str = std::string(meta_json);
                    }
                    // 释放内存
                    if (ipix) fn_free(ipix);
                    if (pixel) fn_free(pixel);
                    if (meta_json) fn_free(meta_json);
                    LOG_INFO("cli", "HCSD 元数据读取成功: nside=" + nside_str +
                             " n_pix=" + n_pix_str);
                } else {
                    LOG_WARN("cli", "aio_hcsd_read 返回错误: " + std::to_string(ret) +
                             " (仅输出二进制头元数据)");
                }
            } else {
                LOG_WARN("cli", "AIO DLL 未导出 aio_hcsd_read/aio_hio_free (仅输出二进制头元数据)");
            }
        } else {
            LOG_WARN("cli", "AIO DLL 未加载 (仅输出二进制头元数据)");
        }
    }

    // 输出 JSONL result 事件 (含 HCSD 元数据)
    std::ostringstream result_oss;
    result_oss << "{\"file\":\"" << json_escape(hcsd_path) << "\""
               << ",\"format\":\"HCSD\""
               << ",\"file_size\":" << file_size
               << ",\"magic\":\"HCSD\""
               << ",\"uncomp_json_len\":" << uncomp_json_len
               << ",\"comp_json_len\":" << comp_json_len
               << ",\"n_leaves\":" << HCSD_N_LEAVES
               << ",\"leaf_index_bytes\":" << HCSD_LEAF_INDEX_BYTES
               << ",\"nside\":" << nside_str
               << ",\"nested\":" << nested_str
               << ",\"n_pix\":" << n_pix_str
               << ",\"meta_json\":" << meta_json_str
               << "}";
    output_jsonl_event_ex("result", "", "", 1.0, "hcsd inspect completed",
                          result_oss.str(), "", -1, -1.0, "ok");
    output_jsonl_event("job_completed", "", "", 1.0, "hcsd inspect completed", result_oss.str());
    return AstroCsExitCode::SUCCESS;
}

// ============================================================================
// P04-003: cmd_inspect_frame - 检查 FITS 帧元数据
// 直接读取 2880 字节头块, 解析关键字 (不依赖 DLL)
// stdout 输出 JSONL 事件 (result 事件含元数据), stderr 输出日志
// ============================================================================
int CliCommand::cmd_inspect_frame(const std::string& fits_path) {
    LOG_INFO("cli", "inspect --frame: " + fits_path);

    // 文件存在性检查
    if (!fs::exists(fits_path)) {
        LOG_ERROR("cli", "FITS 文件不存在: " + fits_path);
        std::string err_json = "{\"code\":\"ASTROCS_FILE_IO_ERROR\",\"numeric_code\":8,\"message\":\"fits file not found\"}";
        output_jsonl_event_ex("error", "", "", -1.0, "", "", err_json,
                              AstroCsExitCode::FILE_IO_ERROR);
        output_jsonl_event("failed", "", "", -1.0, "", "", err_json);
        return AstroCsExitCode::FILE_IO_ERROR;
    }

    // 读取文件大小
    uintmax_t file_size = fs::file_size(fits_path);
    LOG_INFO("cli", "FITS 文件大小: " + std::to_string(file_size) + " 字节");

    // 读取 FITS 头 (2880 字节块)
    std::ifstream ifs(fits_path, std::ios::binary);
    if (!ifs.is_open()) {
        LOG_ERROR("cli", "无法打开 FITS 文件: " + fits_path);
        std::string err_json = "{\"code\":\"ASTROCS_FILE_IO_ERROR\",\"numeric_code\":8,\"message\":\"cannot open fits file\"}";
        output_jsonl_event_ex("error", "", "", -1.0, "", "", err_json,
                              AstroCsExitCode::FILE_IO_ERROR);
        output_jsonl_event("failed", "", "", -1.0, "", "", err_json);
        return AstroCsExitCode::FILE_IO_ERROR;
    }

    // 读取最多 4 个 2880 字节头块 (主头 + 可能的扩展)
    constexpr size_t FITS_BLOCK = 2880;
    constexpr size_t MAX_HEADER_BLOCKS = 4;
    std::vector<char> header_buf(FITS_BLOCK * MAX_HEADER_BLOCKS, 0);
    ifs.read(header_buf.data(), FITS_BLOCK * MAX_HEADER_BLOCKS);
    size_t bytes_read = static_cast<size_t>(ifs.gcount());
    ifs.close();

    // 检查 SIMPLE = T
    bool is_simple_t = (bytes_read >= 80 &&
                        std::string(header_buf.data(), 80).find("SIMPLE") != std::string::npos &&
                        header_buf[29] == 'T');
    if (!is_simple_t) {
        LOG_ERROR("cli", "FITS 头无效 (缺少 SIMPLE = T)");
        std::string err_json = "{\"code\":\"ASTROCS_INPUT_INVALID\",\"numeric_code\":28,\"message\":\"invalid FITS header (SIMPLE=T not found)\"}";
        output_jsonl_event_ex("error", "", "", -1.0, "", "", err_json,
                              AstroCsExitCode::INPUT_INVALID);
        output_jsonl_event("failed", "", "", -1.0, "", "", err_json);
        return AstroCsExitCode::INPUT_INVALID;
    }

    // 解析关键字 (每个关键字 80 字节, 格式: KEY = VALUE / COMMENT)
    // 收集常用关键字: SIMPLE, BITPIX, NAXIS, NAXIS1, NAXIS2, EXPTIME, FILTER, IMAGETYP, OBJECT, INSTRUME, TELESCOP, DATE-OBS
    std::map<std::string, std::string> keywords;
    bool found_end = false;
    for (size_t block = 0; block < MAX_HEADER_BLOCKS && !found_end; ++block) {
        for (size_t i = 0; i < FITS_BLOCK; i += 80) {
            size_t pos = block * FITS_BLOCK + i;
            if (pos + 80 > bytes_read) break;
            std::string card(header_buf.data() + pos, 80);
            // END 卡片
            if (card.substr(0, 4) == "END ") {
                found_end = true;
                break;
            }
            // 解析 KEY = VALUE
            size_t eq_pos = card.find('=');
            if (eq_pos == std::string::npos || eq_pos > 8) continue;
            std::string key = card.substr(0, eq_pos);
            // 去除尾部空格
            while (!key.empty() && key.back() == ' ') key.pop_back();
            if (key.empty()) continue;
            // 提取 VALUE (跳过 = 和空格)
            std::string value = card.substr(eq_pos + 1);
            // 去除前导空格
            size_t vstart = value.find_first_not_of(' ');
            if (vstart == std::string::npos) continue;
            value = value.substr(vstart);
            // 截取 / 前部分 (去掉注释)
            size_t slash = value.find('/');
            if (slash != std::string::npos) {
                value = value.substr(0, slash);
            }
            // 去除尾部空格
            while (!value.empty() && value.back() == ' ') value.pop_back();
            keywords[key] = value;
        }
    }

    LOG_INFO("cli", "FITS 关键字解析完成, 共 " + std::to_string(keywords.size()) + " 个");

    // 构建 keywords JSON 对象
    std::ostringstream kw_oss;
    kw_oss << "{";
    bool first = true;
    for (const auto& kv : keywords) {
        if (!first) kw_oss << ",";
        first = false;
        // 判断 value 是否为字符串 (以 ' 开头) 或数字
        const std::string& v = kv.second;
        bool is_string = (!v.empty() && v.front() == '\'');
        std::string cleaned = v;
        if (is_string) {
            // 去除单引号
            if (cleaned.size() >= 2 && cleaned.back() == '\'') {
                cleaned = cleaned.substr(1, cleaned.size() - 2);
            } else if (cleaned.size() >= 1) {
                cleaned = cleaned.substr(1);
            }
            // 去除尾部空格
            while (!cleaned.empty() && cleaned.back() == ' ') cleaned.pop_back();
        }
        kw_oss << "\"" << json_escape(kv.first) << "\":";
        if (is_string) {
            kw_oss << "\"" << json_escape(cleaned) << "\"";
        } else {
            // 数字或 T/F
            if (v == "T" || v == "F") {
                kw_oss << (v == "T" ? "true" : "false");
            } else {
                kw_oss << json_escape(v);
            }
        }
    }
    kw_oss << "}";

    // 输出 JSONL result 事件 (含 FITS 帧元数据)
    std::ostringstream result_oss;
    result_oss << "{\"file\":\"" << json_escape(fits_path) << "\""
               << ",\"format\":\"FITS\""
               << ",\"file_size\":" << file_size
               << ",\"simple\":true"
               << ",\"keywords\":" << kw_oss.str()
               << "}";
    output_jsonl_event_ex("result", "", "", 1.0, "frame inspect completed",
                          result_oss.str(), "", -1, -1.0, "ok");
    output_jsonl_event("job_completed", "", "", 1.0, "frame inspect completed", result_oss.str());
    return AstroCsExitCode::SUCCESS;
}

// ============================================================================
// P04-001: cmd_capabilities - 查询 CLI 支持的能力
// stdout 输出 capabilities JSON, stderr 输出日志
// P04-002: 扩展 exit_codes 含 numeric_code/code/exit_code_match, 事件类型新增
// P04-003: 扩展 modules 数组 (含 name/version/capabilities), stages, schema_versions
// ============================================================================
int CliCommand::cmd_capabilities() {
    LOG_INFO("cli", "capabilities: 查询 CLI 支持的能力");

    // P04-003: 尝试加载 DLL 获取各模块版本号 (best-effort, 失败不影响 capabilities 输出)
    // 使用局部 Orchestrator 实例, 避免影响全局状态
    std::string aio_ver = "unknown";
    std::string calib_ver = "unknown";
    std::string platesolve_ver = "unknown";
    std::string psf_ver = "unknown";
    std::string photometric_ver = "unknown";
    std::string snr_ver = "unknown";
    std::string drizzle_ver = "unknown";
    std::string stack_ver = "unknown";
    std::string gaia_client_ver = "unknown";
    std::string star_detector_ver = "unknown";

    {
        Orchestrator orch;
        std::string err;
        // 静默加载, 失败不报错 (capabilities 应在 DLL 不可用时仍可工作)
        Logger::instance().set_stderr_output(false);
        bool ok = orch.init_dlls("", err);
        Logger::instance().set_stderr_output(true);
        if (ok) {
            DllLoader& loader = orch.get_dll_loader();
            aio_ver = loader.get_version(ModuleId::AIO);
            calib_ver = loader.get_version(ModuleId::CALIBRATE);
            platesolve_ver = loader.get_version(ModuleId::PLATESOLVE);
            psf_ver = loader.get_version(ModuleId::PSF);
            photometric_ver = loader.get_version(ModuleId::PHOTOMETRIC);
            snr_ver = loader.get_version(ModuleId::SNR);
            drizzle_ver = loader.get_version(ModuleId::DRIZZLE);
            stack_ver = loader.get_version(ModuleId::STACK);
        } else {
            LOG_WARN("cli", "capabilities: DLL 加载失败, 模块版本号将为 unknown");
        }
    }

    std::cout << "{" << std::endl;
    std::cout << "  \"schema_version\": 1," << std::endl;
    std::cout << "  \"version\": \"1.0.0\"," << std::endl;
    // P04-003: modules 数组 (含 name/version/capabilities)
    std::cout << "  \"modules\": [" << std::endl;
    std::cout << "    {\"name\":\"astro_image_io\",\"version\":\"" << aio_ver << "\",\"capabilities\":[\"read_fits\",\"write_hiss\",\"read_hiss\",\"write_hcsd\",\"read_hcsd\"]}," << std::endl;
    std::cout << "    {\"name\":\"calibration\",\"version\":\"" << calib_ver << "\",\"capabilities\":[\"calibrate\"]}," << std::endl;
    std::cout << "    {\"name\":\"star_detector\",\"version\":\"" << star_detector_ver << "\",\"capabilities\":[\"detect\"]}," << std::endl;
    // P09-002: ipv_solver capabilities 显式声明 INTERNAL_DETECTION_SHARED_EXPORT
    //   表示生产路径使用内部单次检测 + callback 共享导出 (P02-003 路径 B 的正式命名)
    //   PSF 阶段通过 star_det 块复用同一份检测结果, 不再二次调用 sdet_detect_ex
    std::cout << "    {\"name\":\"ipv_solver\",\"version\":\"" << platesolve_ver << "\",\"capabilities\":[\"solve_from_memory\",\"solve_from_detections_v1\",\"solve_from_memory_with_callback\",\"internal_detection_shared_export\",\"export_authoritative_pairs\",\"wcs_sip_serialization\"]}," << std::endl;
    std::cout << "    {\"name\":\"dynamic_psf\",\"version\":\"" << psf_ver << "\",\"capabilities\":[\"fit_batch\",\"fit_batch_f32\"]}," << std::endl;
    std::cout << "    {\"name\":\"snr_estimator\",\"version\":\"" << snr_ver << "\",\"capabilities\":[\"estimate\"]}," << std::endl;
    std::cout << "    {\"name\":\"healpix_drizzle\",\"version\":\"" << drizzle_ver << "\",\"capabilities\":[\"drizzle\"]}," << std::endl;
    std::cout << "    {\"name\":\"healpix_stack\",\"version\":\"" << stack_ver << "\",\"capabilities\":[\"stack\"]}," << std::endl;
    std::cout << "    {\"name\":\"photometric_calib\",\"version\":\"" << photometric_ver << "\",\"capabilities\":[\"calibrate\"]}," << std::endl;
    std::cout << "    {\"name\":\"gaia_client\",\"version\":\"" << gaia_client_ver << "\",\"capabilities\":[\"cone_search\",\"query_spectrum\"]}" << std::endl;
    std::cout << "  ]," << std::endl;
    // P04-003: stages 数组 (两段流水线 8 个 stage, spec §2.3.2)
    std::cout << "  \"stages\": [\"READ_FITS\",\"CALIBRATE\",\"PLATESOLVE\",\"PSF\",\"PHOTOMETRIC\",\"SNR\",\"DRIZZLE\",\"STACK\"]," << std::endl;
    std::cout << "  \"commands\": [\"run\", \"run-batch\", \"stage1\", \"stage2\", "
              << "\"inspect\", \"capabilities\", \"status\"]," << std::endl;
    std::cout << "  \"request_commands\": [\"stage1\", \"stage2\", \"inspect\"],"
              << std::endl;
    std::cout << "  \"config_sources\": [\"cli\", \"overrides\", \"config\", \"default\"],"
              << std::endl;
    std::cout << "  \"config_priority\": [\"cli\", \"overrides\", \"config\", \"default\"],"
              << std::endl;
    // P04-003: schema_versions 对象 (各契约文件版本)
    std::cout << "  \"schema_versions\": {\"hiss\":\"1.0\",\"hcsd\":\"1.0\",\"star_det\":\"v1\",\"request\":\"v1\",\"effective_config\":\"v1\",\"jsonl_event\":\"v1\",\"wcs_authoritative_pairs\":\"1.0\",\"wcs_closure_report\":\"1.0\",\"coordinate_convention\":\"2\"},"
              << std::endl;
    // P04-002: exit_codes 扩展为 numeric_code + name + string_code 三元组
    std::cout << "  \"exit_codes\": [" << std::endl;
    std::cout << "    {\"numeric_code\": 0, \"name\": \"SUCCESS\", \"code\": \"ASTROCS_SUCCESS\"}," << std::endl;
    std::cout << "    {\"numeric_code\": 1, \"name\": \"GENERIC_ERROR\", \"code\": \"ASTROCS_INTERNAL\"}," << std::endl;
    std::cout << "    {\"numeric_code\": 2, \"name\": \"DLL_LOAD_FAILED\", \"code\": \"ASTROCS_MODULE_MISSING\"}," << std::endl;
    std::cout << "    {\"numeric_code\": 3, \"name\": \"BLOCK_MISSING\", \"code\": \"ASTROCS_BLOCK_MISSING\"}," << std::endl;
    std::cout << "    {\"numeric_code\": 4, \"name\": \"CALIBRATE_FAILED\", \"code\": \"ASTROCS_CALIBRATION_MISSING\"}," << std::endl;
    std::cout << "    {\"numeric_code\": 5, \"name\": \"PLATESOLVE_FAILED\", \"code\": \"ASTROCS_PLATESOLVE_FAILED\"}," << std::endl;
    std::cout << "    {\"numeric_code\": 6, \"name\": \"DRIZZLE_FAILED\", \"code\": \"ASTROCS_DRIZZLE_FAILED\"}," << std::endl;
    std::cout << "    {\"numeric_code\": 7, \"name\": \"CONFIG_ERROR\", \"code\": \"ASTROCS_CONFIG_INVALID\"}," << std::endl;
    std::cout << "    {\"numeric_code\": 8, \"name\": \"FILE_IO_ERROR\", \"code\": \"ASTROCS_FILE_IO_ERROR\"}," << std::endl;
    std::cout << "    {\"numeric_code\": 9, \"name\": \"TIMEOUT\", \"code\": \"ASTROCS_TIMEOUT\"}," << std::endl;
    std::cout << "    {\"numeric_code\": 10, \"name\": \"CANCELLED\", \"code\": \"ASTROCS_CANCELLED\"}," << std::endl;
    std::cout << "    {\"numeric_code\": 20, \"name\": \"STAR_DETECT_FAILED\", \"code\": \"ASTROCS_STAR_DETECT_FAILED\"}," << std::endl;
    std::cout << "    {\"numeric_code\": 21, \"name\": \"PSF_FAILED\", \"code\": \"ASTROCS_PSF_FAILED\"}," << std::endl;
    std::cout << "    {\"numeric_code\": 22, \"name\": \"PHOTOMETRIC_FAILED\", \"code\": \"ASTROCS_PHOTOMETRIC_FAILED\"}," << std::endl;
    std::cout << "    {\"numeric_code\": 23, \"name\": \"SNR_FAILED\", \"code\": \"ASTROCS_SNR_FAILED\"}," << std::endl;
    std::cout << "    {\"numeric_code\": 24, \"name\": \"STACK_FAILED\", \"code\": \"ASTROCS_STACK_FAILED\"}," << std::endl;
    std::cout << "    {\"numeric_code\": 25, \"name\": \"HISS_INVALID\", \"code\": \"ASTROCS_HISS_INVALID\"}," << std::endl;
    std::cout << "    {\"numeric_code\": 26, \"name\": \"HCSD_INVALID\", \"code\": \"ASTROCS_HCSD_INVALID\"}," << std::endl;
    std::cout << "    {\"numeric_code\": 27, \"name\": \"MODULE_ABI_UNSUPPORTED\", \"code\": \"ASTROCS_MODULE_ABI_UNSUPPORTED\"}," << std::endl;
    std::cout << "    {\"numeric_code\": 28, \"name\": \"INPUT_INVALID\", \"code\": \"ASTROCS_INPUT_INVALID\"}," << std::endl;
    std::cout << "    {\"numeric_code\": 100, \"name\": \"MODULE_SPECIFIC_BASE\", \"code\": \"ASTROCS_MODULE_SPECIFIC\"}" << std::endl;
    std::cout << "  ]," << std::endl;
    // 事件类型: job_started/stage_started/stage_progress/stage_completed/warning/error/job_completed
    std::cout << "  \"events\": [\"job_started\", \"stage_started\", \"stage_progress\", "
              << "\"stage_completed\", \"warning\", \"error\", \"job_completed\"],"
              << std::endl;
    std::cout << "  \"stdout_format\": \"jsonl\"," << std::endl;
    std::cout << "  \"stderr_format\": \"human_readable_log\"," << std::endl;
    std::cout << "  \"jsonl_schema\": \"engineering/contracts/jsonl_event_schema.json\","
              << std::endl;
    std::cout << "  \"error_code_registry\": \"engineering/contracts/error_code_registry.csv\","
              << std::endl;
    std::cout << "  \"hiss_format\": \"engineering/contracts/hiss_format_v1.md\","
              << std::endl;
    std::cout << "  \"hcsd_format\": \"engineering/contracts/hcsd_format_v1.md\""
              << std::endl;
    std::cout << "}" << std::endl;

    return AstroCsExitCode::SUCCESS;
}

// ============================================================================
// P04-001: cmd_request - --request 模式入口
// 解析 request JSON, 合并配置, 输出 JSONL 事件流, 执行任务
// P04-002: 扩展事件流 (stage_start/stage_end/result/error 含数字 exit_code)
// ============================================================================
int CliCommand::cmd_request(const std::string& request_path,
                            const std::map<std::string, std::string>& cli_overrides) {
    LOG_INFO("cli", "request 模式: " + request_path);

    // 读取 request JSON
    if (!fs::exists(request_path)) {
        LOG_ERROR("cli", "request 文件不存在: " + request_path);
        std::string err_json = "{\"code\":\"ASTROCS_INPUT_INVALID\",\"numeric_code\":8,\"message\":\"request file not found\"}";
        output_jsonl_event_ex("error", "", "", -1.0, "", "", err_json,
                              AstroCsExitCode::FILE_IO_ERROR);
        output_jsonl_event("failed", "", "", -1.0, "", "", err_json);
        return AstroCsExitCode::FILE_IO_ERROR;
    }
    std::ifstream ifs(request_path, std::ios::binary);
    if (!ifs.is_open()) {
        LOG_ERROR("cli", "无法打开 request 文件: " + request_path);
        std::string err_json = "{\"code\":\"ASTROCS_INPUT_INVALID\",\"numeric_code\":8,\"message\":\"cannot open request file\"}";
        output_jsonl_event_ex("error", "", "", -1.0, "", "", err_json,
                              AstroCsExitCode::FILE_IO_ERROR);
        output_jsonl_event("failed", "", "", -1.0, "", "", err_json);
        return AstroCsExitCode::FILE_IO_ERROR;
    }
    std::stringstream ss;
    ss << ifs.rdbuf();
    std::string request_json = ss.str();

    // 解析 request JSON
    std::vector<json_merge::JsonField> req_fields =
        json_merge::parse_object(request_json);

    std::string command, job_id, frame, output, config_source, overrides_json;
    for (const auto& f : req_fields) {
        if (f.key == "command") {
            if (f.raw_value.size() >= 2 && f.raw_value[0] == '"') {
                command = f.raw_value.substr(1, f.raw_value.size() - 2);
            } else {
                command = f.raw_value;
            }
        } else if (f.key == "job_id") {
            if (f.raw_value.size() >= 2 && f.raw_value[0] == '"') {
                job_id = f.raw_value.substr(1, f.raw_value.size() - 2);
            }
        } else if (f.key == "frame") {
            if (f.raw_value.size() >= 2 && f.raw_value[0] == '"') {
                frame = f.raw_value.substr(1, f.raw_value.size() - 2);
            }
        } else if (f.key == "output") {
            if (f.raw_value.size() >= 2 && f.raw_value[0] == '"') {
                output = f.raw_value.substr(1, f.raw_value.size() - 2);
            }
        } else if (f.key == "config") {
            config_source = f.raw_value;
        } else if (f.key == "overrides") {
            overrides_json = f.raw_value;
        }
    }

    if (command.empty()) {
        LOG_ERROR("cli", "request JSON 缺少 command 字段");
        std::string err_json = "{\"code\":\"ASTROCS_CONFIG_INVALID\",\"numeric_code\":7,\"message\":\"missing command field\"}";
        output_jsonl_event_ex("error", job_id, "", -1.0, "", "", err_json,
                              AstroCsExitCode::CONFIG_ERROR);
        output_jsonl_event("failed", job_id, "", -1.0, "", "", err_json);
        return AstroCsExitCode::CONFIG_ERROR;
    }

    if (job_id.empty()) job_id = generate_job_id();

    // 计算有效配置
    EffectiveConfig ec = compute_effective_config(
        command, job_id, config_source, overrides_json, cli_overrides, request_path);

    LOG_INFO("cli", "effective_config_hash=" + ec.effective_config_hash);

    // 输出 job_started 事件 (含 effective_config_hash)
    std::string accepted_result = "{\"effective_config_hash\":\"" + ec.effective_config_hash
                                + "\",\"job_id\":\"" + json_escape(job_id) + "\"}";
    output_jsonl_event("job_started", job_id, "", -1.0,
                       "request accepted, effective_config computed", accepted_result);

    // 根据 command 分发
    if (command == "stage1") {
        if (frame.empty()) {
            std::string err_json = "{\"code\":\"ASTROCS_CONFIG_INVALID\",\"numeric_code\":7,\"message\":\"stage1 requires frame field\"}";
            output_jsonl_event_ex("error", job_id, "", -1.0, "", "", err_json,
                                  AstroCsExitCode::CONFIG_ERROR);
            output_jsonl_event("failed", job_id, "", -1.0, "", "", err_json);
            return AstroCsExitCode::CONFIG_ERROR;
        }
        if (output.empty()) {
            std::string err_json = "{\"code\":\"ASTROCS_CONFIG_INVALID\",\"numeric_code\":7,\"message\":\"stage1 requires output field\"}";
            output_jsonl_event_ex("error", job_id, "", -1.0, "", "", err_json,
                                  AstroCsExitCode::CONFIG_ERROR);
            output_jsonl_event("failed", job_id, "", -1.0, "", "", err_json);
            return AstroCsExitCode::CONFIG_ERROR;
        }

        // P04-002: 同时输出 stage_started (旧) 和 stage_start (新, 含 frame)
        output_jsonl_event("stage_started", job_id, "stage1", 0.0, "stage1 started");
        output_jsonl_event_ex("stage_start", job_id, "stage1", 0.0, "stage1 started",
                              "", "", -1, -1.0, "",
                              ",\"frame\":\"" + json_escape(frame) + "\"");

        // 计时开始
        auto t0 = std::chrono::steady_clock::now();

        // 执行 stage1 (使用 ec.config_json 作为配置)
        Orchestrator orch;
        TaskResult r = orch.run_stage1(frame, output, ec.config_json);

        // 计算持续时间
        auto t1 = std::chrono::steady_clock::now();
        double duration_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        // stage_completed / stage_end / result / error 事件
        if (r.success) {
            // 构建 result JSON (含 timings 摘要)
            std::ostringstream result_oss;
            result_oss << "{\"success\":true,\"frame_name\":\"" << json_escape(r.frame_name)
                       << "\",\"output\":\"" << json_escape(output) << "\""
                       << ",\"timings_count\":" << r.timings.size()
                       << ",\"effective_config_hash\":\"" << ec.effective_config_hash << "\"}";
            output_jsonl_event("stage_completed", job_id, "stage1", 1.0,
                               "stage1 completed", result_oss.str());

            // P04-002: stage_end 事件 (含 duration_ms + status)
            output_jsonl_event_ex("stage_end", job_id, "stage1", 1.0,
                                  "stage1 completed", result_oss.str(), "",
                                  -1, duration_ms, "ok");

            // P12-001 子任务B: quality_metric 事件 (photometric 阶段诊断指标)
            // 从 r.photo_stats 读取 PhotometricDiag 17 字段 + N_MATCHED/SCALE_FACTOR/SIGMA_RESIDUAL
            {
                auto get_ps_int = [&](const std::string& key) -> int {
                    auto it = r.photo_stats.find(key);
                    if (it == r.photo_stats.end()) return 0;
                    try { return std::stoi(it->second); } catch (...) { return 0; }
                };
                auto get_ps_double = [&](const std::string& key) -> double {
                    auto it = r.photo_stats.find(key);
                    if (it == r.photo_stats.end()) return 0.0;
                    try { return std::stod(it->second); } catch (...) { return 0.0; }
                };
                std::ostringstream metric_oss;
                metric_oss << ",\"metric\":{"
                    << "\"spectrum_rows_total\":" << get_ps_int("SPECTRUM_ROWS_TOTAL")
                    << ",\"valid_fsyn\":" << get_ps_int("VALID_FSYN")
                    << ",\"gaia_in_frame\":" << get_ps_int("GAIA_IN_FRAME")
                    << ",\"psf_total\":" << get_ps_int("PSF_TOTAL")
                    << ",\"psf_valid\":" << get_ps_int("PSF_VALID")
                    << ",\"spatial_candidates\":" << get_ps_int("SPATIAL_CANDIDATES")
                    << ",\"unique_matches\":" << get_ps_int("UNIQUE_MATCHES")
                    << ",\"rejected_ambiguous\":" << get_ps_int("REJECTED_AMBIGUOUS")
                    << ",\"rejected_distance\":" << get_ps_int("REJECTED_DISTANCE")
                    << ",\"rejected_quality\":" << get_ps_int("REJECTED_QUALITY")
                    << ",\"fit_used\":" << get_ps_int("FIT_USED")
                    << ",\"robust_iterations\":" << get_ps_int("ROBUST_ITERATIONS")
                    << ",\"r_median\":" << get_ps_double("R_MEDIAN")
                    << ",\"r_p90\":" << get_ps_double("R_P90")
                    << ",\"r_max\":" << get_ps_double("R_MAX")
                    << ",\"match_dist_median\":" << get_ps_double("MATCH_DIST_MEDIAN")
                    << ",\"match_dist_p90\":" << get_ps_double("MATCH_DIST_P90")
                    << ",\"match_dist_max\":" << get_ps_double("MATCH_DIST_MAX")
                    << "}";
                output_jsonl_event_ex("quality_metric", job_id, "photometric", -1.0, "", "", "",
                                      -1, -1.0, "", metric_oss.str());
            }

            // P04-002: result 事件 (含 output + hash)
            // 注: stage1 输出 .hiss 文件, hash 字段为 effective_config_hash (输出文件 hash 需读文件)
            std::string output_hash = ec.effective_config_hash;  // 使用 ec hash 作为可追溯性 hash
            output_jsonl_event_ex("result", job_id, "stage1", 1.0,
                                  "stage1 output produced", "", "",
                                  -1, -1.0, "ok",
                                  ",\"output\":\"" + json_escape(output) + "\""
                                  + ",\"hash\":\"" + output_hash + "\"");

            output_jsonl_event("job_completed", job_id, "", 1.0,
                             "request completed successfully", result_oss.str());
            return AstroCsExitCode::SUCCESS;
        } else {
            int ec_exit = r.exit_code != 0 ? r.exit_code : AstroCsExitCode::GENERIC_ERROR;
            const char* ec_str = AstroCsExitCode::error_code_string(ec_exit);
            std::string exit_code_str = std::to_string(ec_exit);
            std::string err_json = "{\"code\":\"" + std::string(ec_str) + "\","
                                 + "\"numeric_code\":" + exit_code_str + ","
                                 + "\"message\":\"" + json_escape(r.error_msg) + "\","
                                 + "\"exit_code\":" + exit_code_str + "}";
            // P04-002: stage_end 事件 (失败时也输出, 含 duration_ms + status=failed)
            output_jsonl_event_ex("stage_end", job_id, "stage1", -1.0,
                                  "stage1 failed", "", err_json,
                                  ec_exit, duration_ms, "failed");
            // P04-002: error 事件 (含数字 exit_code)
            output_jsonl_event_ex("error", job_id, "stage1", -1.0,
                                  "stage1 failed", "", err_json,
                                  ec_exit, -1.0, "failed");
            output_jsonl_event("failed", job_id, "stage1", -1.0,
                             "stage1 failed", "", err_json);
            return ec_exit;
        }
    } else if (command == "stage2") {
        if (output.empty()) {
            std::string err_json = "{\"code\":\"ASTROCS_CONFIG_INVALID\",\"numeric_code\":7,\"message\":\"stage2 requires output field\"}";
            output_jsonl_event_ex("error", job_id, "", -1.0, "", "", err_json,
                                  AstroCsExitCode::CONFIG_ERROR);
            output_jsonl_event("failed", job_id, "", -1.0, "", "", err_json);
            return AstroCsExitCode::CONFIG_ERROR;
        }
        if (frame.empty()) {
            std::string err_json = "{\"code\":\"ASTROCS_CONFIG_INVALID\",\"numeric_code\":7,\"message\":\"stage2 requires frame (hiss dir) field\"}";
            output_jsonl_event_ex("error", job_id, "", -1.0, "", "", err_json,
                                  AstroCsExitCode::CONFIG_ERROR);
            output_jsonl_event("failed", job_id, "", -1.0, "", "", err_json);
            return AstroCsExitCode::CONFIG_ERROR;
        }

        output_jsonl_event("stage_started", job_id, "stage2", 0.0, "stage2 started");
        output_jsonl_event_ex("stage_start", job_id, "stage2", 0.0, "stage2 started",
                              "", "", -1, -1.0, "",
                              ",\"frame\":\"" + json_escape(frame) + "\"");

        auto t0 = std::chrono::steady_clock::now();

        Orchestrator orch;
        TaskResult r = orch.run_stage2(frame, output, ec.config_json);

        auto t1 = std::chrono::steady_clock::now();
        double duration_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        if (r.success) {
            std::ostringstream result_oss;
            result_oss << "{\"success\":true,\"frame_name\":\"" << json_escape(r.frame_name)
                       << "\",\"output\":\"" << json_escape(output) << "\""
                       << ",\"effective_config_hash\":\"" << ec.effective_config_hash << "\"}";
            output_jsonl_event("stage_completed", job_id, "stage2", 1.0,
                               "stage2 completed", result_oss.str());
            output_jsonl_event_ex("stage_end", job_id, "stage2", 1.0,
                                  "stage2 completed", result_oss.str(), "",
                                  -1, duration_ms, "ok");
            std::string output_hash = ec.effective_config_hash;
            output_jsonl_event_ex("result", job_id, "stage2", 1.0,
                                  "stage2 output produced", "", "",
                                  -1, -1.0, "ok",
                                  ",\"output\":\"" + json_escape(output) + "\""
                                  + ",\"hash\":\"" + output_hash + "\"");
            output_jsonl_event("job_completed", job_id, "", 1.0,
                             "request completed successfully", result_oss.str());
            return AstroCsExitCode::SUCCESS;
        } else {
            int ec_exit = r.exit_code != 0 ? r.exit_code : AstroCsExitCode::GENERIC_ERROR;
            const char* ec_str = AstroCsExitCode::error_code_string(ec_exit);
            std::string exit_code_str = std::to_string(ec_exit);
            std::string err_json = "{\"code\":\"" + std::string(ec_str) + "\","
                                 + "\"numeric_code\":" + exit_code_str + ","
                                 + "\"message\":\"" + json_escape(r.error_msg) + "\","
                                 + "\"exit_code\":" + exit_code_str + "}";
            // P04-002: stage_end 事件 (失败时也输出)
            output_jsonl_event_ex("stage_end", job_id, "stage2", -1.0,
                                  "stage2 failed", "", err_json,
                                  ec_exit, duration_ms, "failed");
            output_jsonl_event_ex("error", job_id, "stage2", -1.0,
                                  "stage2 failed", "", err_json,
                                  ec_exit, -1.0, "failed");
            output_jsonl_event("failed", job_id, "stage2", -1.0,
                             "stage2 failed", "", err_json);
            return ec_exit;
        }
    } else if (command == "inspect") {
        // inspect 命令: 只输出 effective_config, 不执行任务
        std::ostringstream result_oss;
        result_oss << "{\"effective_config_hash\":\"" << ec.effective_config_hash
                   << "\",\"config\":" << ec.config_json << "}";
        output_jsonl_event("job_completed", job_id, "", 1.0,
                         "inspect completed (no task executed)", result_oss.str());
        return AstroCsExitCode::SUCCESS;
    } else if (command == "capabilities") {
        // capabilities 命令
        output_jsonl_event("job_completed", job_id, "", 1.0,
                         "capabilities query");
        cmd_capabilities();
        return AstroCsExitCode::SUCCESS;
    } else {
        std::string err_json = "{\"code\":\"ASTROCS_CONFIG_INVALID\",\"numeric_code\":7,\"message\":\"unknown command: "
                             + json_escape(command) + "\"}";
        output_jsonl_event_ex("error", job_id, "", -1.0, "", "", err_json,
                              AstroCsExitCode::CONFIG_ERROR);
        output_jsonl_event("failed", job_id, "", -1.0, "", "", err_json);
        return AstroCsExitCode::CONFIG_ERROR;
    }
}

// ============================================================================
// print_usage - 输出用法说明
// ============================================================================
void CliCommand::print_usage() {
    std::cout << "============================================================" << std::endl;
    std::cout << "Orchestrator CLI (两段流水线版本, spec §2.3)" << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "用法:" << std::endl;
    std::cout << "  orchestrator                                - 启动交互式 REPL" << std::endl;
    std::cout << "  orchestrator --help                         - 显示帮助" << std::endl;
    std::cout << "  orchestrator run <fits> [options]           - 单帧处理 (旧版 5 阶段)" << std::endl;
    std::cout << "  orchestrator run-batch <dir> [options]      - 批量处理 (旧版 5 阶段)" << std::endl;
    std::cout << "  orchestrator stage1 --frame <fits> --output <hiss> [options]" << std::endl;
    std::cout << "                                              - 单帧预处理 (stage 0-7, spec §2.3.3)" << std::endl;
    std::cout << "  orchestrator stage2 --frames <dir> --output <hcsd> [options]" << std::endl;
    std::cout << "                                              - 多帧合并 (stage 8-9, spec §2.3.3)" << std::endl;
    std::cout << "  orchestrator status                         - 状态查询 (无运行实例)" << std::endl;
    std::cout << std::endl;
    std::cout << "run 选项:" << std::endl;
    std::cout << "  --config <json>       配置文件路径" << std::endl;
    std::cout << "  --threads <N>         线程数 (0=自动检测)" << std::endl;
    std::cout << "  --fresh               忽略检查点重新开始 (Task 3)" << std::endl;
    std::cout << "  --log-level <LEVEL>   日志级别 (DEBUG/INFO/WARN/ERROR, 默认 INFO, Task 4)" << std::endl;
    std::cout << std::endl;
    std::cout << "run-batch 选项:" << std::endl;
    std::cout << "  --config <json>       配置文件路径" << std::endl;
    std::cout << "  --threads <N>         线程数 (0=自动检测)" << std::endl;
    std::cout << "  --fresh               忽略检查点重新开始 (Task 3)" << std::endl;
    std::cout << "  --log-level <LEVEL>   日志级别 (DEBUG/INFO/WARN/ERROR, 默认 INFO, Task 4)" << std::endl;
    std::cout << std::endl;
    std::cout << "stage1 选项 (单帧预处理 FITS -> .hiss):" << std::endl;
    std::cout << "  --frame <fits>        输入 FITS 文件路径 (必选)" << std::endl;
    std::cout << "  --output <hiss>       输出 .hiss 文件路径 (必选)" << std::endl;
    std::cout << "  --gaia-data <dir>     Gaia 数据目录" << std::endl;
    std::cout << "  --calibration-dir <dir> 校准文件目录" << std::endl;
    std::cout << "  --filter <name>       滤镜名称" << std::endl;
    std::cout << "  --config <json>       stage1_config.json 配置文件路径" << std::endl;
    std::cout << "  --log-level <LEVEL>   日志级别 (DEBUG/INFO/WARN/ERROR, 默认 INFO)" << std::endl;
    std::cout << std::endl;
    std::cout << "stage2 选项 (多帧合并 .hiss -> .hcsd):" << std::endl;
    std::cout << "  --frames <dir>        输入 .hiss 文件目录 (必选)" << std::endl;
    std::cout << "  --output <hcsd>       输出 .hcsd 文件路径 (必选)" << std::endl;
    std::cout << "  --config <json>       stage2_config.json 配置文件路径" << std::endl;
    std::cout << "  --log-level <LEVEL>   日志级别 (DEBUG/INFO/WARN/ERROR, 默认 INFO)" << std::endl;
    std::cout << "============================================================" << std::endl;
}
