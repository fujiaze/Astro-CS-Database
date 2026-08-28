// astrocs CLI — 单一用户入口 (V5, CLI-002)
// 统一 parser + JSON/JSONL writer + 退出码映射 + 协作取消 + crash boundary。
// 命令树/协议/退出码唯一权威: 控制包 04 + docs/api/CLI_PROTOCOL_V1.md。
// Windows Unicode: wmain → UTF-16 argv 转 UTF-8, 文件经 std::filesystem::u8path 打开。
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
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

#include "cancel_token.h"
#include "exit_codes.h"
#include "jsonl.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include "version_generated.h"

namespace {

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
                                           "--run-manifest", "--group", "--phases"};

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
    {"phase1 run",                  {"--config", "--cpu-profile", "--events-jsonl"}},
    {"phase2 run",                  {"--config", "--cpu-profile", "--events-jsonl"}},
    {"phase3 run",                  {"--config", "--cpu-profile", "--events-jsonl"}},
    {"run",                         {"--phases", "--config", "--cpu-profile", "--events-jsonl"}},
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

int cmd_config_validate(const Parsed& p, astrocs::JsonlEmitter& ev) {
    const std::string path = need_value(p, "--config");
    std::ifstream f(std::filesystem::u8path(path), std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "astrocs: config not found '%s'\n", path.c_str());
        return astrocs::INPUT;   // 04: 输入缺失 → 3
    }
    std::stringstream buf; buf << f.rdbuf();
    try {
        auto doc = nlohmann::json::parse(buf.str());
        if (!doc.is_object()) {
            std::fprintf(stderr, "astrocs: config is not a JSON object\n");
            return astrocs::INPUT;  // 04: 格式错 → 3
        }
    } catch (const nlohmann::json::parse_error& e) {
        std::fprintf(stderr, "astrocs: config malformed JSON: %s\n", sanitize(e.what()).c_str());
        return astrocs::INPUT;      // 04: 格式错 → 3
    }
    ev.emit("artifact", "info", "config", "validated", {{"role", "config"}, {"path", path}});
    std::printf("config OK\n");
    return astrocs::OK;
}

// stub 命令(科学接线属 CODE/TST 域): 参数已按合同全量校验, 明示 not-wired。
// 测试钩子(ASTROCS_TEST_SLEEP_MS / ASTROCS_TEST_CRASH=1)仅用于协议 golden 测试, 非用户接口。
int cmd_stub(const Parsed& p, const std::string& phase, astrocs::JsonlEmitter& ev) {
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
    if (joined == "config init")      return cmd_config_init(p, ev);
    if (joined == "config validate")  return cmd_config_validate(p, ev);
    if (joined == "test synthetic") {
        const std::string g = need_value(p, "--group");
        if (!kGroups.count(g)) parse_fail("invalid --group '" + g + "'");
    }
    if (joined == "benchmark cpu") {
        const bool q = p.flags.count("--quick") > 0, full = p.flags.count("--full") > 0;
        if (q == full) parse_fail("benchmark cpu requires exactly one of --quick|--full");
    }
    if (joined == "run") {
        const std::string ph = need_value(p, "--phases");
        // --phases: 1|2|3 的非空升序无重复逗号子集(04 示例: 1,2,3)
        std::vector<std::string> parts;
        std::string cur;
        std::stringstream ss(ph);
        while (std::getline(ss, cur, ',')) parts.push_back(cur);
        if (parts.empty() || parts.size() > 3) parse_fail("invalid --phases '" + ph + "'");
        int last = 0;
        for (const auto& s : parts) {
            if (s.size() != 1 || s[0] < '1' || s[0] > '3') parse_fail("invalid --phases '" + ph + "'");
            int v = s[0] - '0';
            if (v <= last) parse_fail("invalid --phases '" + ph + "' (must be ascending, unique)");
            last = v;
        }
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
