// astrocs CLI — 单一用户入口 (V5, CLI-002)
// 统一 parser + JSON/JSONL writer + 退出码映射 + 协作取消 + crash boundary。
// 命令树/协议/退出码唯一权威: 控制包 04 + docs/api/CLI_PROTOCOL_V1.md。
// Windows Unicode: wmain → UTF-16 argv 转 UTF-8, 文件经 std::filesystem::u8path 打开。
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#if !defined(_WIN32)
#include <sched.h>
#endif
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "sha256.h"

#include "hardware_inspect.h"
#include "profile_gen.h"
#include "p1_session.h"
#include "p2_session.h"
#include "p3_session.h"

namespace astrocs {
nlohmann::json g_phase1_session;
nlohmann::json g_phase2_session;
nlohmann::json g_phase3_session;
inline std::string p1_last_error(acs_handle h) { return phase1::last_error(h); }
}

#include "backend_loader.h"

extern "C" {
int astrocs_host_services_default_v1(astrocs_host_services_v1* out, void** state_out);
void astrocs_host_services_destroy_state_v1(void* state);
void astrocs_host_state_set_budget_v1(void* state, uint32_t cpus, uint32_t max_workers,
                                      astrocs_host_services_v1* out);
uint64_t astrocs_cpu_detect_features_v1(void);
}


#include "cancel_token.h"
#include "exit_codes.h"
#include "jsonl.h"
#include "monitor.h"
#include "resource_events.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#include "version_generated.h"

namespace {

// CLI→host 的取消/日志桥接(定义于本 namespace 后段, 此处前向声明供 cmd_run_pipeline 使用)
static int cli_cancel_probe(void*);
static void cli_session_log(void*, int, const char*, const char*);
// MON-002 资源/backend 事件发射(定义于后段, 前向声明供 cmd_run_pipeline 使用)
static void emit_resource_summary(astrocs::JsonlEmitter&, const std::string&,
                                  const astrocs::ProcessMonitor::Summary&, const std::string&,
                                  std::size_t, const std::string&);
static void emit_backend_event(astrocs::JsonlEmitter&, const std::string&, const std::string&,
                               const std::string&, uint32_t, uint32_t);

// 主机可用并行预算(禁硬编码, 04/BENCH 规范): Linux 用 sched_getaffinity, Windows 用有效处理器数; 至少 1。
static uint32_t cli_affinity_cpu_count() {
#if defined(_WIN32)
    unsigned int n = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    return n == 0 ? 1u : static_cast<uint32_t>(n);
#else
    cpu_set_t set; CPU_ZERO(&set);
    if (sched_getaffinity(0, sizeof(set), &set) != 0) return 1u;
    uint32_t n = 0;
    for (int i = 0; i < CPU_SETSIZE; ++i)
        if (CPU_ISSET(i, &set)) ++n;
    return n == 0 ? 1u : n;
#endif
}


const char* kHelp =
    "astrocs --version [--json]\n"
    "astrocs hardware inspect --json\n"
    "astrocs config init --output <path>\n"
    "astrocs config validate --config <path>\n"
    "astrocs config show-effective --config <path> [--cpu-profile <path>] --json\n"
    "astrocs benchmark cpu (--quick|--full) [--output <path>]\n"
    "astrocs doctor --json\n"
    "astrocs test synthetic --group <all|calibration|wcs_psf|noise_snr|drizzle|upm|rejection_integration|pipeline>\n"
    "astrocs phase1 run --config <path> [--cpu-profile <path>] [--events-jsonl]\n"
    "astrocs phase2 run --config <path> [--cpu-profile <path>] [--events-jsonl]\n"
    "astrocs phase3 run --config <path> [--cpu-profile <path>] [--events-jsonl]\n"
    "astrocs run --phases <1|2|3|1,2|1,2,3> --config <path> [--cpu-profile <path>] [--events-jsonl]\n"
    "astrocs verify --run-manifest <path> --json\n";

// ───────────────────────── parser ─────────────────────────

struct ParseError : std::runtime_error {
    explicit ParseError(const std::string& m) : std::runtime_error(m) {}
};

struct Parsed {
    std::vector<std::string> cmd;                // 子命令 token (e.g. {"config","init"}) 或 {"--help"}
    std::map<std::string, std::string> values;   // 带值旗标
    std::set<std::string> flags;                 // 布尔旗标
    std::string join() const {
        std::string s;
        for (const auto& c : cmd) { if (!s.empty()) s += ' '; s += c; }
        return s;
    }
};

const std::set<std::string> kBoolFlags = {"--json", "--events-jsonl", "--quick", "--full"};
const std::set<std::string> kValueFlags = {"--output", "--config", "--cpu-profile",
                                           "--run-manifest", "--group", "--phases",
                                           "--resource-detail"};

// 04 §1 命令树: 每条命令允许的旗标(严格白名单, 未知即 2)
struct CmdRule { const char* path; std::vector<std::string> allowed; };
const CmdRule kRules[] = {
    {"hardware inspect",            {"--json"}},
    {"config init",                 {"--output"}},
    {"config validate",             {"--config"}},
    {"config show-effective",       {"--config", "--cpu-profile", "--json"}},
    {"benchmark cpu",               {"--quick", "--full", "--output"}},
    {"doctor",                      {"--json"}},
    {"test synthetic",              {"--group"}},
    {"phase1 run",                  {"--config", "--cpu-profile", "--events-jsonl", "--resource-detail"}},
    {"phase2 run",                  {"--config", "--cpu-profile", "--events-jsonl", "--resource-detail"}},
    {"phase3 run",                  {"--config", "--cpu-profile", "--events-jsonl", "--resource-detail"}},
    {"run",                         {"--phases", "--config", "--cpu-profile", "--events-jsonl", "--resource-detail"}},
    {"verify",                      {"--run-manifest", "--json"}},
};

[[noreturn]] void parse_fail(const std::string& msg) { throw ParseError(msg); }

Parsed parse_args(int argc, char** argv_utf8) {
    Parsed p;
    std::vector<std::string> raw(argv_utf8 + (argc > 0 ? 1 : 0), argv_utf8 + argc);
    size_t i = 0;
    // 子命令 token: 不以 -- 开头, 逐段拼接直至命中已知命令(或 --version/--help)
    std::vector<std::string> tokens;
    bool known_break = false;
    for (; i < raw.size(); ++i) {
        const std::string& a = raw[i];
        if (!a.empty() && a[0] == '-') {
            // 顶层 dash 命令(--version/--help/-h)视作命令本身
            if (tokens.empty() && (a == "--version" || a == "--help" || a == "-h")) {
                tokens.push_back(a);
                known_break = true;
            }
            break;
        }
        tokens.push_back(a);
        std::string joined;
        for (const auto& t : tokens) { if (!joined.empty()) joined += ' '; joined += t; }
        bool known = false;
        for (const auto& r : kRules) if (joined == r.path) known = true;
        if (known) { known_break = true; break; }
        if (tokens.size() >= 2) parse_fail("unknown command '" + joined + "'");
    }
    p.cmd = tokens;
    if (known_break) ++i;  // 越过已消费的最后一个命令 token
    const std::string joined = p.join();
    if (joined.empty()) parse_fail("no command given");
    bool matched = (joined == "--help" || joined == "-h" || joined == "--version");
    for (; i < raw.size(); ++i) {
        const std::string& a = raw[i];
        if (a.size() < 2 || a[0] != '-') parse_fail("unexpected positional argument '" + a + "'");
        if (kBoolFlags.count(a)) {
            if (!p.flags.insert(a).second) parse_fail("duplicate flag '" + a + "'");
        } else if (kValueFlags.count(a)) {
            if (p.values.count(a)) parse_fail("duplicate flag '" + a + "'");
            const bool next_is_flag =
                (i + 1 < raw.size()) && raw[i + 1].size() > 1 && raw[i + 1][0] == '-' &&
                (kBoolFlags.count(raw[i + 1]) > 0 || kValueFlags.count(raw[i + 1]) > 0);
            if (i + 1 >= raw.size() || raw[i + 1].empty() || next_is_flag)
                parse_fail("flag '" + a + "' requires a value");
            p.values[a] = raw[++i];
        } else {
            parse_fail("unknown flag '" + a + "'");
        }
    }
    // 命令级旗标白名单(顶层命令只允许 --json)
    for (const auto& r : kRules) {
        if (joined != r.path) continue;
        matched = true;
        for (const auto& f : p.values)
            if (std::find(r.allowed.begin(), r.allowed.end(), f.first) == r.allowed.end())
                parse_fail("flag '" + f.first + "' not allowed for '" + joined + "'");
        for (const auto& f : p.flags)
            if (std::find(r.allowed.begin(), r.allowed.end(), f) == r.allowed.end())
                parse_fail("flag '" + f + "' not allowed for '" + joined + "'");
    }
    if (!matched) parse_fail("unknown command '" + joined + "'");
    return p;
}

// ───────────────────── 参数校验帮助器 ─────────────────────

std::string need_value(const Parsed& p, const std::string& flag) {
    auto it = p.values.find(flag);
    if (it == p.values.end() || it->second.empty()) parse_fail("missing required " + flag + " <path>");
    return it->second;
}

const std::set<std::string> kGroups = {"all", "calibration", "wcs_psf", "noise_snr",
                                       "drizzle", "upm", "rejection_integration", "pipeline"};

// crash 报告脱敏(04 §5): 仅保留可打印 ASCII, 截断 200 字符。
std::string sanitize(const std::string& s) {
    std::string out;
    for (char c : s) {
        unsigned char u = static_cast<unsigned char>(c);
        out.push_back((u >= 0x20 && u < 0x7F) ? c : '?');
        if (out.size() >= 200) break;
    }
    return out;
}


// ───────────────────── 具体命令实现 ─────────────────────

// config 模板(CLI-002 最小骨架; 完整 config schema 属 CLI-003)
const char* kConfigTemplate =
    "{\n"
    "  \"schema_version\": \"1\",\n"
    "  \"inputs\": {\"lights\": [], \"darks\": [], \"flats\": [], \"bias\": []},\n"
    "  \"output_dir\": \".\"\n"
    "}\n";

int cmd_config_init(const Parsed& p, astrocs::JsonlEmitter& ev) {
    const std::string out = need_value(p, "--output");
    {
        std::ofstream f(std::filesystem::u8path(out), std::ios::binary | std::ios::trunc);
        if (!f) {
            std::fprintf(stderr, "astrocs: cannot write '%s'\n", out.c_str());
            return astrocs::IO;   // 04: I/O 失败 → 7
        }
        f << kConfigTemplate;
        if (!f.good()) return astrocs::IO;
    }
    ev.emit("artifact", "info", "config", "template written",
            {{"role", "config_template"}, {"path", out}});
    std::printf("%s\n", out.c_str());
    return astrocs::OK;
}

std::string file_sha256(const std::string& u8path, bool* ok) {
    std::ifstream f(std::filesystem::u8path(u8path), std::ios::binary);
    if (!f) { if (ok) *ok = false; return {}; }
    astrocs::crypto::Sha256 h;
    char buf[65536];
    while (f) {
        f.read(buf, sizeof(buf));
        h.update(buf, static_cast<std::size_t>(f.gcount()));
    }
    if (ok) *ok = true;
    return h.final_hex();
}

// 本机 CPU 特征指纹(profile stale 判定; 非调度线程数, 不违反 ARCH-003/AGENTS 禁硬编码)
std::string local_cpu_signature() {
    const std::string seed =
        "astrocs-cpu-amd64-hw=" + std::to_string(std::thread::hardware_concurrency());
    return astrocs::crypto::sha256_hex(seed.data(), seed.size());
}

// pipeline_config.json v1 全量校验(合同: docs/api/MANIFEST_VERIFY_V1.md §1)
// 返回 0 有效(doc 填充); 否则对应退出码, 诊断写 stderr。
int validate_config_full(const std::string& path, nlohmann::json* doc_out) {
    std::ifstream f(std::filesystem::u8path(path), std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "astrocs: config not found '%s'\n", path.c_str());
        return astrocs::INPUT;                       // 04: 输入缺失 → 3
    }
    std::stringstream buf; buf << f.rdbuf();
    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(buf.str());
    } catch (const nlohmann::json::parse_error& e) {
        std::fprintf(stderr, "astrocs: config malformed JSON: %s\n", sanitize(e.what()).c_str());
        return astrocs::INPUT;                       // 04: 格式错 → 3
    }
    if (!doc.is_object()) {
        std::fprintf(stderr, "astrocs: config is not a JSON object\n");
        return astrocs::INPUT;
    }
    static const std::set<std::string> kAllowedKeys = {"schema_version", "inputs",
                                                       "output_dir", "phase3"};
    for (auto it = doc.begin(); it != doc.end(); ++it) {
        if (!kAllowedKeys.count(it.key())) {
            std::fprintf(stderr, "astrocs: config has unknown key '%s'\n", it.key().c_str());
            return astrocs::INPUT;                   // 防拼写静默忽略 → 3
        }
    }
    if (!doc.contains("schema_version")) {
        std::fprintf(stderr, "astrocs: config missing 'schema_version'\n");
        return astrocs::INPUT;
    }
    if (!doc["schema_version"].is_string() || doc["schema_version"].get<std::string>() != "1") {
        std::fprintf(stderr, "astrocs: config schema_version must be \"1\"\n");
        return astrocs::ARGS;                        // 版本错=配置错 → 2
    }
    if (!doc.contains("inputs") || !doc["inputs"].is_object()) {
        std::fprintf(stderr, "astrocs: config missing 'inputs' object\n");
        return astrocs::INPUT;
    }
    for (const char* k : {"lights", "darks", "flats", "bias"}) {
        auto it = doc["inputs"].find(k);
        if (it == doc["inputs"].end() || !it->is_array()) {
            std::fprintf(stderr, "astrocs: config inputs.%s must be an array\n", k);
            return astrocs::INPUT;
        }
        for (const auto& e : *it) {
            if (!e.is_string() || e.get<std::string>().empty()) {
                std::fprintf(stderr, "astrocs: config inputs.%s has empty path\n", k);
                return astrocs::INPUT;
            }
            std::error_code ec;
            if (!std::filesystem::exists(std::filesystem::u8path(e.get<std::string>()), ec)) {
                std::fprintf(stderr, "astrocs: config input not found '%s'\n",
                             e.get<std::string>().c_str());
                return astrocs::INPUT;
            }
        }
    }
    if (!doc.contains("output_dir") || !doc["output_dir"].is_string()) {
        std::fprintf(stderr, "astrocs: config missing 'output_dir'\n");
        return astrocs::INPUT;
    }
    std::error_code ec;
    if (!std::filesystem::exists(std::filesystem::u8path(doc["output_dir"].get<std::string>()), ec)) {
        std::fprintf(stderr, "astrocs: config output_dir not found\n");
        return astrocs::INPUT;
    }
    *doc_out = std::move(doc);
    return astrocs::OK;
}

int cmd_config_validate(const Parsed& p, astrocs::JsonlEmitter& ev) {
    const std::string path = need_value(p, "--config");
    nlohmann::json doc;
    const int rc = validate_config_full(path, &doc);
    if (rc != astrocs::OK) return rc;
    ev.emit("artifact", "info", "config", "validated", {{"role", "config"}, {"path", path}});
    std::printf("config OK\n");
    return astrocs::OK;
}

// cpu profile 独立文件校验(分离原则): 结构(3)/stale(5) — profile hash 不与 config 混算
int validate_cpu_profile(const std::string& path, nlohmann::json* prof_out) {
    std::ifstream f(std::filesystem::u8path(path), std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "astrocs: cpu profile not found '%s'\n", path.c_str());
        return astrocs::INPUT;
    }
    std::stringstream buf; buf << f.rdbuf();
    nlohmann::json prof;
    try {
        prof = nlohmann::json::parse(buf.str());
    } catch (const nlohmann::json::parse_error& e) {
        std::fprintf(stderr, "astrocs: cpu profile malformed JSON: %s\n", sanitize(e.what()).c_str());
        return astrocs::INPUT;
    }
    if (!prof.is_object() || prof.value("kind", std::string()) != "astrocs_cpu_profile" ||
        prof.value("schema_version", std::string()) != "1" || !prof.contains("kernels") ||
        !prof["kernels"].is_object()) {
        std::fprintf(stderr, "astrocs: cpu profile is not a v1 astrocs_cpu_profile document\n");
        return astrocs::INPUT;
    }
    if (prof.value("cpu_signature", std::string()) != local_cpu_signature()) {
        std::fprintf(stderr, "astrocs: cpu profile is stale (profile cpu_signature=%s, "
                             "local=%s) — rerun 'astrocs benchmark cpu'\n",
                     prof.value("cpu_signature", std::string()).c_str(),
                     local_cpu_signature().c_str());
        return astrocs::BACKEND;                     // 04: CPU 特征 → 5
    }
    *prof_out = std::move(prof);
    return astrocs::OK;
}

// show-effective: config 与 profile 分别校验 → 合成 effective(--json 固定, 04 §1)
int cmd_show_effective(const Parsed& p, astrocs::JsonlEmitter& ev) {
    (void)p; (void)ev;
    if (!p.flags.count("--json")) parse_fail("config show-effective requires --json");
    const std::string cfg = need_value(p, "--config");
    nlohmann::json doc;
    int rc = validate_config_full(cfg, &doc);
    if (rc != astrocs::OK) return rc;
    nlohmann::json out = {
        {"schema_version", "1"},
        {"config", doc},
        {"effective", {{"phases", doc.value("inputs", nlohmann::json::object()).contains("lights") &&
                                            !doc["inputs"]["lights"].empty()
                                        ? nlohmann::json({1, 2})
                                        : nlohmann::json({3})}}},
    };
    if (p.values.count("--cpu-profile")) {
        nlohmann::json prof;
        rc = validate_cpu_profile(p.values.at("--cpu-profile"), &prof);
        if (rc != astrocs::OK) return rc;
        out["cpu_profile"] = prof;
        bool ok = false;
        out["effective"]["cpu_profile_sha256"] = file_sha256(p.values.at("--cpu-profile"), &ok);
    }
    std::printf("%s\n", out.dump().c_str());
    return astrocs::OK;
}

// stub 命令(科学接线属 CODE/TST 域): 参数已按合同全量校验, 明示 not-wired。
// 测试钩子(ASTROCS_TEST_SLEEP_MS / ASTROCS_TEST_CRASH=1)仅用于协议 golden 测试, 非用户接口。
int cmd_stub(const Parsed& p, const std::string& phase, astrocs::JsonlEmitter& ev) {
    (void)p;
    const char* sleep_ms = std::getenv("ASTROCS_TEST_SLEEP_MS");
    if (sleep_ms) {
        long ms = std::strtol(sleep_ms, nullptr, 10);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        ev.stage("stub_wait", true);
        while (std::chrono::steady_clock::now() < deadline) {
            if (astrocs::is_cancelled()) {
                ev.stage("stub_wait", false);
                ev.emit_final(astrocs::CANCELLED, "cancelled", nullptr, "cancelled by user");
                std::fprintf(stderr, "astrocs: cancelled\n");
                return astrocs::CANCELLED;   // 04: 取消 → 9, 不留伪完整产物
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        ev.stage("stub_wait", false);
    }
    if (std::getenv("ASTROCS_TEST_CRASH")) {
        throw std::runtime_error("selftest-crash");   // crash boundary 落锤(→70)
    }
    ev.emit_final(astrocs::ARGS, "not_wired", nullptr,
                  "command is declared by the CLI contract but science handlers are wired in later tasks");
    std::fprintf(stderr, "astrocs: '%s' is declared by the CLI contract but not wired in this build "
                         "(see docs/api/CLI_PROTOCOL_V1.md)\n", phase.c_str());
    return astrocs::ARGS;
}

// run manifest v1 原子写(tmp+rename; ARCH-002 §5 单元): stub/not-wired/cancelled 恒 incomplete
int write_run_manifest(const std::string& out_dir, astrocs::JsonlEmitter& ev, const std::string& status,
                       const std::string& summary, const std::string& config_path,
                       const std::string& config_sha, const std::vector<int>& phases,
                       const nlohmann::json& artifacts = nlohmann::json::array()) {
    nlohmann::json m = {
        {"schema_version", "1"},
        {"kind", "astrocs_run_manifest"},
        {"run_id", ev.run_id()},
        {"astrocs_version", ASTROCS_VERSION_STRING},
        {"platform", {{"os",
#ifdef _WIN32
                       "windows"
#else
                       "linux"
#endif
                       },
                      {"arch", "amd64"}}},
        {"config_path", config_path},
        {"config_sha256", config_sha},
        {"cpu_profile_path", nullptr},
        {"cpu_profile_sha256", nullptr},
        {"phases", phases},
        {"artifacts", artifacts},
        {"status", status},
        {"started_utc", astrocs::iso8601_utc_now()},
        {"finished_utc", astrocs::iso8601_utc_now()},
        {"summary", summary},
    };
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::u8path(out_dir), ec);
    const std::string final_path = out_dir + "/astrocs_run_" + ev.run_id() + ".json";
    const std::string tmp_path = final_path + ".tmp";
    {
        std::ofstream f(std::filesystem::u8path(tmp_path), std::ios::binary | std::ios::trunc);
        if (!f) {
            std::fprintf(stderr, "astrocs: cannot write run manifest '%s'\n", tmp_path.c_str());
            return astrocs::IO;
        }
        f << m.dump(2) << "\n";
        if (!f.good()) return astrocs::IO;
    }
    std::filesystem::rename(std::filesystem::u8path(tmp_path), std::filesystem::u8path(final_path), ec);
    if (ec) {
        std::fprintf(stderr, "astrocs: cannot finalize run manifest: %s\n", ec.message().c_str());
        return astrocs::IO;
    }
    ev.emit("artifact", "info", "manifest", "run manifest written",
            {{"role", "run_manifest"}, {"path", final_path},
             {"sha256", [&]{ bool ok=false; return file_sha256(final_path, &ok); }() }});
    // --events-jsonl 模式下 stdout 只能是 JSON 事件(04 §3): 路径已入 artifact 事件
    if (!ev.enabled()) std::printf("%s\n", final_path.c_str());
    return astrocs::OK;
}

int cmd_run_pipeline(const Parsed& p, astrocs::JsonlEmitter& ev) {
    const std::string cfg = need_value(p, "--config");
    nlohmann::json doc;
    int rc = validate_config_full(cfg, &doc);
    if (rc != astrocs::OK) return rc;
    bool ok = false;
    const std::string cfg_sha = file_sha256(cfg, &ok);
    if (!ok) return astrocs::INPUT;
    // MON-002: --resource-detail summary|timeseries(07 §1); summary 默认强制。
    std::string resource_detail = p.values.count("--resource-detail")
                                      ? p.values.at("--resource-detail") : std::string("summary");
    if (resource_detail != "summary" && resource_detail != "timeseries")
        parse_fail("invalid --resource-detail (summary|timeseries)");
    // MON-002: 进程监控(07 §1 强制): run 阶段采样, 摘要内嵌于 resource 事件。
    astrocs::ProcessMonitor proc_mon(0.25);
    // --phases: 1|2|3 的非空升序无重复逗号子集(04 示例: 1,2,3) — 先于任何写操作
    std::vector<int> phases;
    int last = 0;
    for (char c : p.values.at("--phases")) {
        if (c == ',') continue;
        if (c < '1' || c > '3') parse_fail("invalid --phases");
        const int v = c - '0';
        if (v <= last) parse_fail("invalid --phases (must be ascending, unique)");
        last = v;
        phases.push_back(v);
    }
    if (phases.empty() || phases.size() > 3) parse_fail("invalid --phases");
    // 取消检查点(真实 sleep 钩子沿用 cmd_stub 语义; 内核取消点在 CODE 域接线)
    const char* sleep_ms = std::getenv("ASTROCS_TEST_SLEEP_MS");
    if (sleep_ms) {
        const long ms = std::strtol(sleep_ms, nullptr, 10);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        ev.stage("run_wait", true);
        while (std::chrono::steady_clock::now() < deadline) {
            if (astrocs::is_cancelled()) {
                ev.stage("run_wait", false);
                rc = write_run_manifest(doc.value("output_dir", "."), ev, "incomplete",
                                        "cancelled by user", cfg, cfg_sha, phases);
                if (rc != astrocs::OK) return rc;
                ev.emit_final(astrocs::CANCELLED, "cancelled", nullptr, "cancelled by user");
                std::fprintf(stderr, "astrocs: cancelled\n");
                return astrocs::CANCELLED;               // 04: 取消 → 9, manifest=incomplete
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        ev.stage("run_wait", false);
    }
    if (std::getenv("ASTROCS_TEST_CRASH")) throw std::runtime_error("selftest-crash");
    // ── real orchestrator: run requested phases in-process (no shell-out) ──
    // 每个 phase 从 run config 派生自身 config 并驱动对应 session; 逐 phase 收集 artifact。
    astrocs_host_services_v1 host;
    void* hstate = nullptr;
    astrocs_host_services_default_v1(&host, &hstate);
    host.cancel.is_cancelled = &cli_cancel_probe;
    host.logger.log = &cli_session_log;
    {
        const uint32_t n = cli_affinity_cpu_count();
        astrocs_host_state_set_budget_v1(hstate, n, n, &host);
    }
    const std::string out_dir = doc.value("output_dir", std::string("."));
    nlohmann::json all_artifacts = nlohmann::json::array();
    std::string fail_reason;
    int fail_phase = 0;

    // resume / hash-mismatch: 校验 output_dir 中所有 prior astrocs_run_*.json 记录的 artifact 哈希链;
    // 任一 prior artifact 磁盘 sha 与其记录不符 → 8(绝不静默跳过验证)→不进入新一轮运行。
    {
        bool mismatch = false;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::u8path(out_dir), ec)) {
            const std::string fn = entry.path().filename().u8string();
            if (!entry.is_regular_file() || fn.rfind("astrocs_run_", 0) != 0 || fn.size() <= 14 ||
                fn.substr(fn.size() - 5) != ".json")
                continue;
            try {
                std::ifstream pf(entry.path(), std::ios::binary);
                nlohmann::json pm = nlohmann::json::parse(
                    std::string(std::istreambuf_iterator<char>(pf), {}));
                if (pm.value("kind", std::string()) != "astrocs_run_manifest") continue;
                for (const auto& a : pm.value("artifacts", nlohmann::json::array())) {
                    const std::string ap = a.value("path", std::string());
                    if (ap.empty()) continue;
                    if (!std::filesystem::exists(std::filesystem::u8path(ap), ec)) { mismatch = true; break; }
                    bool ok = false; const std::string sha = file_sha256(ap, &ok);
                    if (!ok || sha != a.value("sha256", std::string())) { mismatch = true; break; }
                }
            } catch (...) { mismatch = true; }
            if (ec) { mismatch = false; ec.clear(); }   // 目录遍历错误不算哈希不匹配
            if (mismatch) break;
        }
        if (mismatch) {
            rc = write_run_manifest(out_dir, ev, "incomplete", "resume hash mismatch",
                                    cfg, cfg_sha, phases);
            if (rc != astrocs::OK) return rc;
            ev.emit_final(astrocs::INTEGRITY, "resume_hash_mismatch", nullptr,
                          "prior artifact hash mismatch");
            std::fprintf(stderr, "astrocs: resume hash mismatch\n");
            return astrocs::INTEGRITY;   // 04/07: 输出完整性/验证失败 → 8
        }
    }

    // MON-002: 背景采样线程(进程 CPU/RSS 在 run 期间累计), 000 后 join; 禁硬编码。
    std::atomic<bool> mon_stop{false};
    std::thread mon_thread([&proc_mon, &mon_stop]() {
        while (!mon_stop.load()) {
            proc_mon.tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    });

    for (int phase : phases) {
        ev.stage(("run_phase" + std::to_string(phase)).c_str(), true);
        nlohmann::json pdoc;
        if (phase == 3) {
            // run config 用 `phase3` 子对象承载 phase3 请求(03 CLI-006 冻结); 缺则失败
            if (!doc.contains("phase3") || !doc["phase3"].is_object()) {
                fail_reason = "run config missing 'phase3' object for --phases 3";
                ev.stage("run_phase3", false);
                break;
            }
            pdoc = doc["phase3"];
            pdoc["output_dir"] = out_dir;
        } else if (phase == 2) {
            // phase2 需要 hips_paths + output_dir; run config 的 inputs.lights 映射为 hips_paths
            if (!doc.contains("inputs") || !doc["inputs"].is_object() ||
                !doc["inputs"].contains("lights")) {
                fail_reason = "run config missing 'inputs.lights' for --phases 2";
                ev.stage("run_phase2", false);
                break;
            }
            pdoc = nlohmann::json::object();
            pdoc["hips_paths"] = doc["inputs"]["lights"];
            pdoc["output_dir"] = out_dir;
        } else {  // phase 1
            // phase1 需要 input_lights + output_dir(另有 master_* 可选); run config 的 inputs.lights 映射
            if (!doc.contains("inputs") || !doc["inputs"].is_object() ||
                !doc["inputs"].contains("lights")) {
                fail_reason = "run config missing 'inputs.lights' for --phases 1";
                ev.stage("run_phase1", false);
                break;
            }
            pdoc = nlohmann::json::object();
            pdoc["input_lights"] = doc["inputs"]["lights"];
            pdoc["output_dir"] = out_dir;
        }
        const std::string ptext = pdoc.dump();
        const acs_span_u8 span{reinterpret_cast<uint8_t*>(const_cast<char*>(ptext.data())),
                               static_cast<uint64_t>(ptext.size())};

        if (phase == 3) {
            acs_handle s3 = nullptr;
            if (p3_session_create(&host, &s3) != ACS_OK) { fail_reason = "p3_session_create"; ev.stage("run_phase3",false); break; }
            acs_status r3 = p3_session_validate(s3, span);
            if (r3 != ACS_OK) { fail_reason = astrocs::phase3::last_error(s3); p3_session_destroy(s3); ev.stage("run_phase3",false); break; }
            r3 = p3_session_run(s3, span);
            acs_span_u8 res{};
            if (p3_session_inspect(s3, &res) == ACS_OK) {
                try { astrocs::g_phase3_session = nlohmann::json::parse(std::string(
                          reinterpret_cast<const char*>(res.data), static_cast<size_t>(res.count))); }
                catch (...) {}
                host.allocator.free(host.allocator.user_data, res.data);
            }
            const std::string serr = astrocs::phase3::last_error(s3);
            p3_session_destroy(s3);
            if (r3 != ACS_OK) { fail_reason = serr; ev.stage("run_phase3",false); break; }
            const std::string op = astrocs::g_phase3_session.value("output_fits_path", std::string());
            if (!op.empty()) {
                bool sok = false; const std::string sha = file_sha256(op, &sok);
                std::error_code ec2; const auto sz = std::filesystem::file_size(std::filesystem::u8path(op), ec2);
                all_artifacts.push_back({{"role", "phase3_output"}, {"path", op},
                                         {"sha256", sok ? sha : ""},
                                         {"size_bytes", ec2 ? 0ULL : static_cast<unsigned long long>(sz)}});
            }
        } else if (phase == 2) {
            acs_handle s2 = nullptr;
            if (p2_session_create(&host, &s2) != ACS_OK) { fail_reason = "p2_session_create"; ev.stage("run_phase2",false); break; }
            acs_status r2 = p2_session_validate(s2, span);
            if (r2 != ACS_OK) { fail_reason = astrocs::phase2::last_error(s2); p2_session_destroy(s2); ev.stage("run_phase2",false); break; }
            r2 = p2_session_run(s2, span);
            acs_span_u8 man{};
            if (p2_session_inspect(s2, &man) == ACS_OK) {
                try { astrocs::g_phase2_session = nlohmann::json::parse(std::string(
                          reinterpret_cast<const char*>(man.data), static_cast<size_t>(man.count))); }
                catch (...) {}
                host.allocator.free(host.allocator.user_data, man.data);
            }
            const std::string serr = astrocs::phase2::last_error(s2);
            p2_session_destroy(s2);
            if (r2 != ACS_OK) { fail_reason = serr; ev.stage("run_phase2",false); break; }
            // 收集 phase2 artifacts(若有)
            for (const auto& a : astrocs::g_phase2_session.value("artifacts", nlohmann::json::array())) {
                const std::string ap = a.get<std::string>();
                bool sok = false; const std::string sha = file_sha256(ap, &sok);
                std::error_code ec2; const auto sz = std::filesystem::file_size(std::filesystem::u8path(ap), ec2);
                all_artifacts.push_back({{"role", "phase2_output"}, {"path", ap},
                                         {"sha256", sok ? sha : ""},
                                         {"size_bytes", ec2 ? 0ULL : static_cast<unsigned long long>(sz)}});
            }
        } else {
            acs_handle s1 = nullptr;
            if (p1_session_create(&host, &s1) != ACS_OK) { fail_reason = "p1_session_create"; ev.stage("run_phase1",false); break; }
            acs_status r1 = p1_session_validate(s1, span);
            if (r1 != ACS_OK) { fail_reason = astrocs::phase1::last_error(s1); p1_session_destroy(s1); ev.stage("run_phase1",false); break; }
            r1 = p1_session_run(s1, span, 0);
            acs_span_u8 man{};
            if (p1_session_inspect(s1, &man) == ACS_OK) {
                try { astrocs::g_phase1_session = nlohmann::json::parse(std::string(
                          reinterpret_cast<const char*>(man.data), static_cast<size_t>(man.count))); }
                catch (...) {}
                host.allocator.free(host.allocator.user_data, man.data);
            }
            const std::string serr = astrocs::phase1::last_error(s1);
            p1_session_destroy(s1);
            if (r1 != ACS_OK) { fail_reason = serr; ev.stage("run_phase1",false); break; }
            for (const auto& a : astrocs::g_phase1_session.value("artifacts", nlohmann::json::array())) {
                const std::string ap = a.get<std::string>();
                bool sok = false; const std::string sha = file_sha256(ap, &sok);
                std::error_code ec2; const auto sz = std::filesystem::file_size(std::filesystem::u8path(ap), ec2);
                all_artifacts.push_back({{"role", "phase1_output"}, {"path", ap},
                                         {"sha256", sok ? sha : ""},
                                         {"size_bytes", ec2 ? 0ULL : static_cast<unsigned long long>(sz)}});
            }
        }
        ev.stage(("run_phase" + std::to_string(phase)).c_str(), false);
        if (!fail_reason.empty()) { fail_phase = phase; break; }
    }
    mon_stop.store(true);
    mon_thread.join();
    astrocs_host_services_destroy_state_v1(hstate);

    // 取消检查(任一阶段取消)
    if (astrocs::is_cancelled()) {
        rc = write_run_manifest(out_dir, ev, "incomplete", "cancelled by user",
                                cfg, cfg_sha, phases, all_artifacts);
        if (rc != astrocs::OK) return rc;
        ev.emit_final(astrocs::CANCELLED, "cancelled", nullptr, "cancelled by user");
        return astrocs::CANCELLED;
    }
    if (!fail_reason.empty()) {
        rc = write_run_manifest(out_dir, ev, "incomplete", "phase" + std::to_string(fail_phase) +
                                " failed: " + fail_reason, cfg, cfg_sha, phases, all_artifacts);
        if (rc != astrocs::OK) return rc;
        ev.emit_final(astrocs::SCIENCE, "run_failed", nullptr, fail_reason);
        std::fprintf(stderr, "astrocs: run failed: %s\n", sanitize(fail_reason).c_str());
        return astrocs::SCIENCE;
    }

    // (resume/hash-mismatch 校验移到 phase 循环之前执行)
    rc = write_run_manifest(out_dir, ev, "complete", "run ok", cfg, cfg_sha, phases, all_artifacts);
    if (rc != astrocs::OK) return rc;
    // MON-002: 结束监控, 发射资源分层事件(07 §1 强制 summary) + backend 事件(07 §2 必采)。
    const auto proc_summary = proc_mon.summary();
    emit_resource_summary(ev, "pipeline", proc_summary, out_dir, proc_summary.n_samples,
                          resource_detail);
    // backend 选择: 从 config 读取(无则 baseline); workers 取 affinity(有效核)。禁硬编码。
    emit_backend_event(ev, "pipeline", doc.value("backend", "baseline"), "ok",
                       std::max(1u, proc_mon.n_cores_hint()), proc_mon.n_cores_hint());
    ev.emit("resource", "info", "pipeline", "run summary",
            {{"n_phases", phases.size()}, {"n_artifacts", all_artifacts.size()}});
    ev.emit_final(astrocs::OK, "ok", nullptr, "run complete");
    return astrocs::OK;
}

// phase1 run: CLI-004 — 进程内调用 p1_session(无 shell-out); cancel/budget/monitor 注入
static int cli_cancel_probe(void*) { return astrocs::is_cancelled() ? 1 : 0; }
static void cli_session_log(void*, int level, const char* component, const char* msg) {
    static const char* kLv[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    std::fprintf(stderr, "[astrocs:%s][%s] %s\n",
                 kLv[level & 3], component ? component : "phase1", msg ? msg : "");
}

// MON-002: 发射资源分层事件(summary 强制; timeseries 详略受 --resource-detail 控制)。
// summary 事件内嵌指标; 原始 timeseries 只记录留存路径+样本数(不内嵌几十 MB 数据)。
static void emit_resource_summary(astrocs::JsonlEmitter& ev, const std::string& phase,
                                  const astrocs::ProcessMonitor::Summary& s,
                                  const std::string& raw_dir, std::size_t raw_n,
                                  const std::string& detail) {
    const auto p = astrocs::summarize(s);
    nlohmann::json payload = {
        {"n_samples", p.n_samples},
        {"wall_seconds", p.wall_seconds},
        {"avg_equivalent_cores", p.avg_equivalent_cores},
        {"peak_equivalent_cores", p.peak_equivalent_cores},
        {"peak_rss_bytes", p.peak_rss_bytes},
        {"rss_slope_bytes_per_s", p.rss_slope_bytes_per_s},
        {"total_read_bytes", p.total_read_bytes},
        {"total_write_bytes", p.total_write_bytes},
        {"total_ctx_switches", p.total_ctx_switches},
        {"max_threads", p.max_threads},
        {"sample_overhead_ms", p.sample_overhead_ms},
        {"resource_detail", detail},
        {"raw_dir", raw_dir},
        {"raw_n", raw_n},
    };
    if (detail == "timeseries") {
        payload["curve_points"] = nlohmann::json::array();
        payload["downsample_max"] = astrocs::kDownsampleMax;
    }
    ev.emit("resource", "info", phase, "resource summary", payload);
}

// MON-002: backend 事件(backend_id/status); 反映所选 backend 与 worker 选择(07 §2 必采)。
static void emit_backend_event(astrocs::JsonlEmitter& ev, const std::string& phase,
                               const std::string& backend_id, const std::string& status,
                               uint32_t workers_used, uint32_t available_cpus) {
    ev.emit("backend", "info", phase, status,
            {{"backend_id", backend_id}, {"workers_used", workers_used},
             {"available_cpus", available_cpus}});
}

// MON-002: 无标注 >5s 区间判 P1(供 MON-003 gating; 本函数仅供测试与 stage 落地校验)。
[[maybe_unused]] static bool is_stage_priority(const char* annotation, double wall_seconds) {
    return astrocs::is_unannotated_priority(annotation, wall_seconds);
}

int cmd_phase2_run(const Parsed& p, astrocs::JsonlEmitter& ev) {
    const std::string cfg = need_value(p, "--config");
    std::ifstream f(std::filesystem::u8path(cfg), std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "astrocs: config not found '%s'\n", cfg.c_str());
        return astrocs::INPUT;
    }
    std::stringstream buf; buf << f.rdbuf();
    const std::string cfg_text = buf.str();
    bool ok = false;
    const std::string cfg_sha = file_sha256(cfg, &ok);
    if (!ok) return astrocs::INPUT;
    astrocs_host_services_v1 host;
    void* hstate = nullptr;
    astrocs_host_services_default_v1(&host, &hstate);
    host.cancel.is_cancelled = &cli_cancel_probe;
    host.logger.log = &cli_session_log;
    {
        const uint32_t n = cli_affinity_cpu_count();
        astrocs_host_state_set_budget_v1(hstate, n, n, &host);
    }
    acs_handle sess = nullptr;
    acs_status rc = p2_session_create(&host, &sess);
    if (rc != ACS_OK) return astrocs::INTERNAL;
    const acs_span_u8 cfg_span{reinterpret_cast<uint8_t*>(const_cast<char*>(cfg_text.data())),
                               static_cast<uint64_t>(cfg_text.size())};
    rc = p2_session_validate(sess, cfg_span);
    if (rc != ACS_OK) {
        const std::string verr = astrocs::phase2::last_error(sess);
        p2_session_destroy(sess);
        astrocs_host_services_destroy_state_v1(hstate);
        std::fprintf(stderr, "astrocs: config invalid: %s\n", sanitize(verr).c_str());
        ev.emit_final(astrocs::ARGS, "config_invalid", nullptr, verr);
        return astrocs::ARGS;
    }
    ev.stage("phase2_session", true);
    if (const char* sleep_ms = std::getenv("ASTROCS_TEST_SLEEP_MS")) {
        const long ms = std::strtol(sleep_ms, nullptr, 10);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        while (std::chrono::steady_clock::now() < deadline) {
            if (astrocs::is_cancelled()) {
                ev.stage("phase2_session", false);
                p2_session_destroy(sess);
                const int wrc = write_run_manifest(".", ev, "incomplete", "cancelled by user",
                                                   cfg, cfg_sha, {2});
                astrocs_host_services_destroy_state_v1(hstate);
                if (wrc != astrocs::OK) return wrc;
                ev.emit_final(astrocs::CANCELLED, "cancelled", nullptr, "cancelled by user");
                std::fprintf(stderr, "astrocs: cancelled\n");
                return astrocs::CANCELLED;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    rc = p2_session_run(sess, cfg_span);
    ev.stage("phase2_session", false);
    acs_span_u8 man{};
    if (p2_session_inspect(sess, &man) == ACS_OK) {
        try { astrocs::g_phase2_session = nlohmann::json::parse(std::string(
                  reinterpret_cast<const char*>(man.data), static_cast<size_t>(man.count))); }
        catch (...) {}
        host.allocator.free(host.allocator.user_data, man.data);
    }
    nlohmann::json artifacts = nlohmann::json::array();
    for (const auto& a : astrocs::g_phase2_session.value("artifacts", nlohmann::json::array())) {
        const std::string ap = a.get<std::string>();
        bool ok2 = false;
        const std::string sha = file_sha256(ap, &ok2);
        std::error_code ec;
        const auto size = std::filesystem::file_size(std::filesystem::u8path(ap), ec);
        artifacts.push_back({{"path", ap}, {"sha256", ok2 ? sha : ""},
                             {"size_bytes", ec ? 0ULL : static_cast<unsigned long long>(size)}});
    }
    const std::string out_dir = [&] {
        try { return nlohmann::json::parse(cfg_text).value("output_dir", std::string(".")); }
        catch (...) { return std::string("."); }
    }();
    const std::string sess_err = astrocs::phase2::last_error(sess);
    p2_session_destroy(sess);
    if (rc == ACS_ERR_CANCELLED) {
        const int wrc = write_run_manifest(out_dir, ev, "incomplete", "cancelled by user",
                                           cfg, cfg_sha, {2}, artifacts);
        astrocs_host_services_destroy_state_v1(hstate);
        if (wrc != astrocs::OK) return wrc;
        ev.emit_final(astrocs::CANCELLED, "cancelled", nullptr, "cancelled by user");
        std::fprintf(stderr, "astrocs: cancelled\n");
        return astrocs::CANCELLED;
    }
    if (rc != ACS_OK) {
        astrocs_host_services_destroy_state_v1(hstate);
        const int wrc = write_run_manifest(out_dir, ev, "incomplete", "phase2 failed: " + sess_err,
                                           cfg, cfg_sha, {2}, artifacts);
        if (wrc != astrocs::OK) return wrc;
        // 映射: config 参数错→2; 输入数据缺失/无效(error_kind=input)→3; production fail(rc=2)→4;
        // IO→7; 其余→70
        const bool input_err = astrocs::g_phase2_session.value("error_kind", std::string()) == "input";
        const int exit_code = input_err ? astrocs::INPUT
                              : (rc == ACS_ERR_PARAM) ? astrocs::ARGS
                              : (rc == ACS_ERR_STATE) ? astrocs::SCIENCE
                              : (rc == ACS_ERR_IO) ? astrocs::IO : astrocs::INTERNAL;
        ev.emit_final(exit_code, "phase2_failed", nullptr, sess_err);
        std::fprintf(stderr, "astrocs: phase2 failed: %s\n", sanitize(sess_err).c_str());
        return exit_code;
    }
    const int wrc = write_run_manifest(out_dir, ev, "complete", "phase2 ok", cfg, cfg_sha, {2},
                                       artifacts);
    astrocs_host_services_destroy_state_v1(hstate);
    if (wrc != astrocs::OK) return wrc;
    ev.emit("resource", "info", "phase2", "session summary",
            {{"n_inputs", astrocs::g_phase2_session.value("n_inputs", 0)},
             {"n_obs", astrocs::g_phase2_session.value("n_obs", 0ull)}});
    ev.emit_final(astrocs::OK, "ok", nullptr, "phase2 complete");
    return astrocs::OK;
}

int cmd_phase3_run(const Parsed& p, astrocs::JsonlEmitter& ev) {
    const std::string cfg = need_value(p, "--config");
    std::ifstream f(std::filesystem::u8path(cfg), std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "astrocs: config not found '%s'\n", cfg.c_str());
        return astrocs::INPUT;
    }
    std::stringstream buf; buf << f.rdbuf();
    const std::string cfg_text = buf.str();
    bool ok = false;
    const std::string cfg_sha = file_sha256(cfg, &ok);
    if (!ok) return astrocs::INPUT;
    astrocs_host_services_v1 host;
    void* hstate = nullptr;
    astrocs_host_services_default_v1(&host, &hstate);
    host.cancel.is_cancelled = &cli_cancel_probe;
    host.logger.log = &cli_session_log;
    {
        const uint32_t n = cli_affinity_cpu_count();
        astrocs_host_state_set_budget_v1(hstate, n, n, &host);
    }
    acs_handle sess = nullptr;
    acs_status rc = p3_session_create(&host, &sess);
    if (rc != ACS_OK) return astrocs::INTERNAL;
    const acs_span_u8 cfg_span{reinterpret_cast<uint8_t*>(const_cast<char*>(cfg_text.data())),
                               static_cast<uint64_t>(cfg_text.size())};
    rc = p3_session_validate(sess, cfg_span);
    if (rc != ACS_OK) {
        const std::string verr = astrocs::phase3::last_error(sess);
        p3_session_destroy(sess);
        astrocs_host_services_destroy_state_v1(hstate);
        std::fprintf(stderr, "astrocs: request invalid: %s\n", sanitize(verr).c_str());
        ev.emit_final(astrocs::ARGS, "config_invalid", nullptr, verr);
        return astrocs::ARGS;
    }
    ev.stage("phase3_session", true);
    rc = p3_session_run(sess, cfg_span);
    ev.stage("phase3_session", false);
    acs_span_u8 res{};
    if (p3_session_inspect(sess, &res) == ACS_OK) {
        try { astrocs::g_phase3_session = nlohmann::json::parse(std::string(
                  reinterpret_cast<const char*>(res.data), static_cast<size_t>(res.count))); }
        catch (...) {}
        host.allocator.free(host.allocator.user_data, res.data);
    }
    const std::string sess_err = astrocs::phase3::last_error(sess);
    const std::string out_dir = [&] {
        try { return nlohmann::json::parse(cfg_text).value("output_dir", std::string(".")); }
        catch (...) { return std::string("."); }
    }();
    p3_session_destroy(sess);
    if (rc == ACS_ERR_CANCELLED) {
        const int wrc = write_run_manifest(out_dir, ev, "incomplete", "cancelled by user",
                                           cfg, cfg_sha, {3});
        astrocs_host_services_destroy_state_v1(hstate);
        if (wrc != astrocs::OK) return wrc;
        ev.emit_final(astrocs::CANCELLED, "cancelled", nullptr, "cancelled by user");
        return astrocs::CANCELLED;
    }
    if (rc != ACS_OK) {
        astrocs_host_services_destroy_state_v1(hstate);
        const int wrc = write_run_manifest(out_dir, ev, "incomplete",
                                           "phase3 failed: " + sess_err, cfg, cfg_sha, {3});
        if (wrc != astrocs::OK) return wrc;
        const bool input_err = astrocs::g_phase3_session.value("error_kind", std::string()) == "input";
        const int exit_code = input_err ? astrocs::INPUT
                              : (rc == ACS_ERR_PARAM) ? astrocs::ARGS
                              : (rc == ACS_ERR_UNSUPPORTED) ? astrocs::SCIENCE
                              : (rc == ACS_ERR_STATE) ? astrocs::SCIENCE
                              : (rc == ACS_ERR_IO) ? astrocs::IO : astrocs::INTERNAL;
        ev.emit_final(exit_code, "phase3_failed", nullptr, sess_err);
        std::fprintf(stderr, "astrocs: phase3 failed: %s\n", sanitize(sess_err).c_str());
        return exit_code;
    }
    const std::string opath = astrocs::g_phase3_session.value("output_fits_path", std::string());
    nlohmann::json objects = nlohmann::json::array();
    if (!opath.empty()) {
        bool ok2 = false;
        const std::string sha = file_sha256(opath, &ok2);
        std::error_code ec;
        const auto size = std::filesystem::file_size(std::filesystem::u8path(opath), ec);
        objects.push_back({{"path", opath}, {"sha256", ok2 ? sha : ""},
                           {"size_bytes", ec ? 0ULL : static_cast<unsigned long long>(size)}});
    }
    const int wrc = write_run_manifest(out_dir, ev, "complete", "phase3 ok", cfg, cfg_sha, {3}, objects);
    astrocs_host_services_destroy_state_v1(hstate);
    if (wrc != astrocs::OK) return wrc;
    ev.emit("resource", "info", "phase3", "session summary",
            {{"order_sel_used", astrocs::g_phase3_session.value("order_sel_used", -1)},
             {"sampler_used", astrocs::g_phase3_session.value("sampler_used", std::string())}});
    ev.emit_final(astrocs::OK, "ok", nullptr, "phase3 complete");
    return astrocs::OK;
}

int cmd_phase1_run(const Parsed& p, astrocs::JsonlEmitter& ev) {
    const std::string cfg = need_value(p, "--config");
    std::ifstream f(std::filesystem::u8path(cfg), std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "astrocs: config not found '%s'\n", cfg.c_str());
        return astrocs::INPUT;
    }
    std::stringstream buf; buf << f.rdbuf();
    const std::string cfg_text = buf.str();
    bool ok = false;
    const std::string cfg_sha = file_sha256(cfg, &ok);
    if (!ok) return astrocs::INPUT;

    // host services: cancel 桥接 CLI 信号; budget=有效 affinity(禁硬编码); monitor=logger→事件
    astrocs_host_services_v1 host;
    void* hstate = nullptr;
    astrocs_host_services_default_v1(&host, &hstate);
    host.cancel.is_cancelled = &cli_cancel_probe;
    host.logger.log = &cli_session_log;
    {
        const uint32_t n = cli_affinity_cpu_count();
        astrocs_host_state_set_budget_v1(hstate, n, n, &host);
    }

    acs_handle sess = nullptr;
    acs_status rc = p1_session_create(&host, &sess);
    if (rc != ACS_OK) return astrocs::INTERNAL;
    const acs_span_u8 cfg_span{reinterpret_cast<uint8_t*>(const_cast<char*>(cfg_text.data())),
                               static_cast<uint64_t>(cfg_text.size())};
    rc = p1_session_validate(sess, cfg_span);
    if (rc != ACS_OK) {
        const std::string verr = astrocs::p1_last_error(sess);   // 先取, 后 destroy
        p1_session_destroy(sess);
        astrocs_host_services_destroy_state_v1(hstate);
        std::fprintf(stderr, "astrocs: config invalid: %s\n", sanitize(verr).c_str());
        // 04 §3: JSONL 模式下错误也须以 final 事件收尾(stdout 纯 JSON)
        ev.emit_final(astrocs::ARGS, "config_invalid", nullptr, verr);
        return astrocs::ARGS;                       // 04: 配置错 → 2
    }
    ev.stage("phase1_session", true);
    // 测试钩子(非用户接口): 阶段间等待, 供取消/无子进程证明(与 run/stub 同语义)
    if (const char* sleep_ms = std::getenv("ASTROCS_TEST_SLEEP_MS")) {
        const long ms = std::strtol(sleep_ms, nullptr, 10);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        while (std::chrono::steady_clock::now() < deadline) {
            if (astrocs::is_cancelled()) {
                ev.stage("phase1_session", false);
                acs_span_u8 cm{};
                if (p1_session_inspect(sess, &cm) == ACS_OK)
                    host.allocator.free(host.allocator.user_data, cm.data);
                p1_session_destroy(sess);
                const int wrc = write_run_manifest(".", ev, "incomplete", "cancelled by user",
                                                   cfg, cfg_sha, {1});
                astrocs_host_services_destroy_state_v1(hstate);
                if (wrc != astrocs::OK) return wrc;
                ev.emit_final(astrocs::CANCELLED, "cancelled", nullptr, "cancelled by user");
                std::fprintf(stderr, "astrocs: cancelled\n");
                return astrocs::CANCELLED;             // 04: 取消 → 9
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    rc = p1_session_run(sess, cfg_span, 0);
    ev.stage("phase1_session", false);
    acs_span_u8 man{};
    if (p1_session_inspect(sess, &man) == ACS_OK) {
        try { astrocs::g_phase1_session = nlohmann::json::parse(std::string(
                  reinterpret_cast<const char*>(man.data), static_cast<size_t>(man.count))); }
        catch (...) {}
        host.allocator.free(host.allocator.user_data, man.data);
    }
    nlohmann::json artifacts = nlohmann::json::array();
    for (const auto& a : astrocs::g_phase1_session.value("artifacts", nlohmann::json::array())) {
        const std::string ap = a.get<std::string>();
        bool ok2 = false;
        const std::string sha = file_sha256(ap, &ok2);
        std::error_code ec;
        const auto size = std::filesystem::file_size(std::filesystem::u8path(ap), ec);
        artifacts.push_back({{"path", ap}, {"sha256", ok2 ? sha : ""},
                             {"size_bytes", ec ? 0ULL : static_cast<unsigned long long>(size)}});
    }
    const std::string out_dir = [&] {
        try { return nlohmann::json::parse(cfg_text).value("output_dir", std::string(".")); }
        catch (...) { return std::string("."); }
    }();
    const std::string sess_err = astrocs::p1_last_error(sess);   // 先取, 后 destroy
    p1_session_destroy(sess);

    if (rc == ACS_ERR_CANCELLED) {
        const int wrc = write_run_manifest(out_dir, ev, "incomplete", "cancelled by user",
                                           cfg, cfg_sha, {1}, artifacts);
        astrocs_host_services_destroy_state_v1(hstate);
        if (wrc != astrocs::OK) return wrc;
        ev.emit_final(astrocs::CANCELLED, "cancelled", nullptr, "cancelled by user");
        std::fprintf(stderr, "astrocs: cancelled\n");
        return astrocs::CANCELLED;                   // 04: 取消 → 9, manifest=incomplete
    }
    if (rc != ACS_OK) {
        const std::string why = sess_err;
        const bool input_err = astrocs::g_phase1_session.value("error_kind", std::string()) == "input" ||
                               rc == ACS_ERR_PARAM;
        astrocs_host_services_destroy_state_v1(hstate);
        const int wrc = write_run_manifest(out_dir, ev, "incomplete", "phase1 failed: " + why,
                                           cfg, cfg_sha, {1}, artifacts);
        if (wrc != astrocs::OK) return wrc;
        ev.emit_final(input_err ? astrocs::INPUT : (rc == ACS_ERR_IO ? astrocs::IO : astrocs::INTERNAL),
                      "phase1_failed", nullptr, why);
        std::fprintf(stderr, "astrocs: phase1 failed: %s\n", sanitize(why).c_str());
        return input_err ? astrocs::INPUT
                         : (rc == ACS_ERR_IO ? astrocs::IO : astrocs::INTERNAL);
    }
    const int wrc = write_run_manifest(out_dir, ev, "complete", "phase1 ok", cfg, cfg_sha, {1},
                                       artifacts);
    astrocs_host_services_destroy_state_v1(hstate);
    if (wrc != astrocs::OK) return wrc;
    ev.emit("resource", "info", "phase1", "frames processed",
            {{"frames", astrocs::g_phase1_session.value("frames", 0)}});
    ev.emit_final(astrocs::OK, "ok", nullptr, "phase1 complete");
    return astrocs::OK;
}

// verify: 04 §3 — manifest→status→version→输入 hash→逐 artifact(存在→sha→size)
int cmd_verify(const Parsed& p, astrocs::JsonlEmitter& ev) {
    (void)ev;
    if (!p.flags.count("--json")) parse_fail("verify requires --json");
    const std::string mp = need_value(p, "--run-manifest");
    std::ifstream f(std::filesystem::u8path(mp), std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "astrocs: run manifest not found '%s'\n", mp.c_str());
        return astrocs::INPUT;
    }
    std::stringstream buf; buf << f.rdbuf();
    nlohmann::json m;
    try {
        m = nlohmann::json::parse(buf.str());
    } catch (const nlohmann::json::parse_error& e) {
        std::fprintf(stderr, "astrocs: manifest malformed JSON: %s\n", sanitize(e.what()).c_str());
        return astrocs::INPUT;
    }
    if (!m.is_object() || m.value("kind", std::string()) != "astrocs_run_manifest" ||
        m.value("schema_version", std::string()) != "1") {
        std::fprintf(stderr, "astrocs: not a v1 astrocs_run_manifest document\n");
        return astrocs::INPUT;
    }
    if (m.value("status", std::string()) != "complete") {
        std::fprintf(stderr, "astrocs: run manifest status='%s' (incomplete run cannot be verified)\n",
                     m.value("status", std::string()).c_str());
        return astrocs::INTEGRITY;                        // 04: 输出完整性/验证失败 → 8
    }
    if (m.value("astrocs_version", std::string()) != ASTROCS_VERSION_STRING) {
        std::fprintf(stderr, "astrocs: manifest was produced by version '%s', this is '%s'\n",
                     m.value("astrocs_version", std::string()).c_str(), ASTROCS_VERSION_STRING);
        return astrocs::BACKEND;                          // 04 §5(换版本不可 verify 旧 run)
    }
    int checked = 1;
    if (m.contains("config_path") && !m["config_path"].is_null()) {
        bool ok = false;
        const std::string cur = file_sha256(m["config_path"].get<std::string>(), &ok);
        if (!ok) {
            std::fprintf(stderr, "astrocs: config input no longer readable\n");
            return astrocs::INPUT;
        }
        if (cur != m.value("config_sha256", std::string())) {
            std::fprintf(stderr, "astrocs: config changed since the run (hash mismatch)\n");
            return astrocs::INPUT;
        }
        ++checked;
    }
    for (const auto& a : m.value("artifacts", nlohmann::json::array())) {
        const std::string apath = a.value("path", std::string());
        std::error_code ec;
        if (!std::filesystem::exists(std::filesystem::u8path(apath), ec)) {
            std::fprintf(stderr, "astrocs: artifact missing '%s'\n", apath.c_str());
            return astrocs::INPUT;
        }
        bool ok = false;
        const std::string sha = file_sha256(apath, &ok);
        if (!ok || sha != a.value("sha256", std::string())) {
            std::fprintf(stderr, "astrocs: artifact sha256 mismatch '%s'\n", apath.c_str());
            return astrocs::INTEGRITY;
        }
        const auto size = std::filesystem::file_size(std::filesystem::u8path(apath), ec);
        if (ec || static_cast<unsigned long long>(size) != a.value("size_bytes", 0ULL)) {
            std::fprintf(stderr, "astrocs: artifact size mismatch '%s'\n", apath.c_str());
            return astrocs::INTEGRITY;
        }
        ++checked;
    }
    nlohmann::json out = {{"verify", "ok"}, {"checked", checked}, {"manifest", mp}};
    std::printf("%s\n", out.dump().c_str());
    return astrocs::OK;
}

int dispatch(const Parsed& p) {
    const std::string joined = p.join();
    const bool events = p.flags.count("--events-jsonl") > 0;
    const std::string phase_name =
        (joined == "run") ? "pipeline" : (joined.rfind("phase", 0) == 0 ? joined.substr(0, 6) : joined);
    astrocs::JsonlEmitter ev(events, astrocs::make_run_id(), phase_name);

    if (joined == "--version") {
        if (p.flags.count("--json")) {
            std::printf("{\"schema_version\":\"1\",\"name\":\"astrocs\",\"version\":\"%s\"}\n",
                        ASTROCS_VERSION_STRING);
        } else {
            std::printf("astrocs %s\n", ASTROCS_VERSION_STRING);
        }
        return astrocs::OK;
    }
    if (joined == "--help" || joined == "-h") {
        std::fputs(kHelp, stdout);
        return astrocs::OK;
    }
    if (joined == "benchmark cpu") {
        const bool quick = p.flags.count("--quick") > 0;
        const bool full = p.flags.count("--full") > 0;
        if (quick == full) parse_fail("benchmark cpu requires exactly one of --quick|--full");
        const std::string out_path = p.values.count("--output") ? p.values.at("--output")
                                                                : "cpu_profile.json";
        const std::string mode = quick ? "quick" : "full";
        const std::string commit = ASTROCS_COMMIT_SHA;
        const std::string json = astrocs::backend_host::generate_profile_json(
            mode, ASTROCS_VERSION_STRING, commit, ASTROCS_COMMIT_SHA);
        {
            std::ofstream f(std::filesystem::u8path(out_path), std::ios::binary | std::ios::trunc);
            if (!f) {
                std::fprintf(stderr, "astrocs: cannot write profile '%s'\n", out_path.c_str());
                return astrocs::IO;
            }
            f << json;
        }
        // 机器可读结果(stdout 简洁结果): 输出路径+verdict
        try {
            auto doc = nlohmann::json::parse(json);
            std::printf("%s %s\n", out_path.c_str(),
                        doc.value("verdict", "FAIL").c_str());
        } catch (...) {
            std::printf("%s\n", out_path.c_str());
        }
        ev.emit("artifact", "info", "benchmark", "cpu profile written",
                {{"role", "cpu_profile"}, {"path", out_path}});
        return astrocs::OK;
    }
    if (joined == "doctor") {
        if (!p.flags.count("--json")) parse_fail("doctor requires --json");
        const std::string hw = astrocs::backend_host::hardware_inspect_json_v1(ASTROCS_VERSION_STRING);
        auto hwd = nlohmann::json::parse(hw);
        astrocs_host_services_v1 host;
        void* hstate = nullptr;
        astrocs_host_services_default_v1(&host, &hstate);
        astrocs_backend_api_v1 api{};
        std::memset(&api, 0, sizeof(api));
        const int grc = astrocs_backend_get_api_v1(ACS_ABI_VERSION_V1,
                                                   sizeof(astrocs_host_services_v1), &host, &api);
        nlohmann::json checks = nlohmann::json::array();
        checks.push_back(nlohmann::json{
            {"name", "baseline_selftest"},
            {"status", (grc == ACS_OK && api.self_test &&
                        api.self_test(&host) == ACS_OK) ? "pass" : "fail"}});
        checks.push_back(nlohmann::json{
            {"name", "hardware_sanity"},
            {"status", (hwd.value("available_logical_cpus", 0u) >= 1 &&
                        hwd.value("ram_bytes", 0ull) > 0) ? "pass" : "fail"}});
        // shipped backend 核查(05 §7): 安全检测但不执行不支持指令——预检 manifest 内条目
        std::ifstream mf("backends.manifest.json");
        if (mf) {
            std::stringstream mbuf; mbuf << mf.rdbuf();
            std::vector<astrocs::backend_host::ManifestEntry> entries;
            std::string merr;
            astrocs::backend_host::parse_backends_manifest(mbuf.str(), &entries, &merr);
            for (const auto& e : entries) {
                std::string why;
                auto pr = astrocs::backend_host::preflight_entry(
                    ".", e, astrocs_cpu_detect_features_v1(), &why);
                nlohmann::json ck;
                ck["name"] = "backend_preflight:" + e.backend_id;
                ck["status"] = pr.decision == astrocs::backend_host::LoadResult::OK
                                   ? "pass" : "skipped";
                ck["detail"] = why;
                checks.push_back(ck);
            }
        } else {
            checks.push_back(nlohmann::json{{"name", "backends_manifest"},
                                            {"status", "pass"},
                                            {"detail", "no shipped DSO (builtin baseline)"}});
        }
        bool all = true;
        for (const auto& c : checks)
            if (c.value("status", "") == "fail") all = false;
        nlohmann::json doc = {{"schema_version", 1}, {"kind", "astrocs_doctor"},
                              {"checks", checks}, {"verdict", all ? "PASS" : "FAIL"}};
        std::printf("%s\n", doc.dump(2).c_str());
        return all ? astrocs::OK : astrocs::SCIENCE;
    }
    if (joined == "hardware inspect") {
        if (!p.flags.count("--json")) parse_fail("hardware inspect requires --json");
        std::fputs(astrocs::backend_host::hardware_inspect_json_v1(ASTROCS_VERSION_STRING).c_str(), stdout);
        return astrocs::OK;
    }
    if (joined == "config init")           return cmd_config_init(p, ev);
    if (joined == "config validate")       return cmd_config_validate(p, ev);
    if (joined == "config show-effective") return cmd_show_effective(p, ev);
    if (joined == "phase1 run")            return cmd_phase1_run(p, ev);
    if (joined == "phase2 run")            return cmd_phase2_run(p, ev);
    if (joined == "phase3 run")            return cmd_phase3_run(p, ev);
    if (joined == "verify")                return cmd_verify(p, ev);
    if (joined == "run")                   return cmd_run_pipeline(p, ev);
    if (joined == "test synthetic") {
        const std::string g = need_value(p, "--group");
        if (!kGroups.count(g)) parse_fail("invalid --group '" + g + "'");
    }
    return cmd_stub(p, joined, ev);
}

}  // namespace

// ─────────────── crash boundary + 平台入口 ───────────────

int real_main(int argc, char** argv_utf8) {
    astrocs::install_cancel_handlers();
    std::string joined_for_report;
    try {
        Parsed p = parse_args(argc, argv_utf8);
        joined_for_report = p.join();
        if (joined_for_report.empty() || joined_for_report == "--help" || joined_for_report == "-h") {
            std::fputs(kHelp, joined_for_report.empty() ? stderr : stdout);
            return joined_for_report.empty() ? astrocs::ARGS : astrocs::OK;
        }
        return dispatch(p);
    } catch (const ParseError& e) {
        std::fprintf(stderr, "astrocs: %s\n", e.what());
        std::fputs(kHelp, stderr);
        return astrocs::ARGS;   // 04: CLI 参数错 → 2
    } catch (const std::exception& e) {
        // 04 §5: 未捕获异常 → 70 + run_id + 阶段 + 最小脱敏 crash report, 不泄露凭据
        std::fprintf(stderr,
                     "astrocs: CRASH run_id=%s command='%s' detail='%s' (sanitized; no credentials)\n",
                     astrocs::make_run_id().c_str(),
                     sanitize(joined_for_report).c_str(), sanitize(e.what()).c_str());
        return astrocs::INTERNAL;
    } catch (...) {
        std::fprintf(stderr, "astrocs: CRASH run_id=%s command='%s' detail='unknown exception'\n",
                     astrocs::make_run_id().c_str(), sanitize(joined_for_report).c_str());
        return astrocs::INTERNAL;
    }
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    std::vector<std::string> u8;
    u8.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        int n = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
        std::string s(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
        if (n > 1) WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, s.data(), n, nullptr, nullptr);
        u8.push_back(std::move(s));
    }
    std::vector<char*> ptrs;
    for (auto& s : u8) ptrs.push_back(s.data());
    ptrs.push_back(nullptr);
    return real_main(static_cast<int>(u8.size()), ptrs.data());
}
#else
int main(int argc, char** argv) { return real_main(argc, argv); }
#endif
