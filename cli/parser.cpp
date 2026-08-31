// astrocs CLI — parser (RT-008 拆分自 main.cpp)
// 统一 parser + 参数校验帮助器 + CPU 指纹/hash 帮助器。
// 定义在 namespace astrocs 外(与拆分前一致); 不 include 任何 session/科学内部头。
#include "cli_common.h"

#include "sha256.h"

#include "exit_codes.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

// ───────────────────────── parser ─────────────────────────
// struct ParseError / struct Parsed 定义见 cli_common.h(共享头)

const std::set<std::string> kBoolFlags = {"--json", "--events-jsonl", "--quick", "--full"};
const std::set<std::string> kValueFlags = {"--output", "--config", "--cpu-profile",
                                           "--run-manifest", "--group", "--phases",
                                           "--resource-detail", "--nside", "--pixfrac",
                                           "--preset", "--profile"};

// 04 §1 命令树: 每条命令允许的旗标(严格白名单, 未知即 2)
// struct CmdRule 定义见 cli_common.h; 此处定义 kRules 表(extern 声明)
const CmdRule kRules[] = {
    {"hardware inspect",            {"--json"}},
    {"config init",                 {"--output"}},
    {"config validate",             {"--config"}},
    {"config show-effective",       {"--config", "--cpu-profile", "--json"}},
    {"benchmark cpu",               {"--quick", "--full", "--output", "--events-jsonl"}},
    {"benchmark verify-profile",    {"--profile", "--json"}},
    {"doctor",                      {"--json"}},
    {"test synthetic",              {"--group"}},
    {"phase1 run",                  {"--config", "--cpu-profile", "--events-jsonl", "--resource-detail"}},
    {"phase2 run",                  {"--config", "--cpu-profile", "--events-jsonl", "--resource-detail"}},
    {"phase3 run",                  {"--config", "--cpu-profile", "--events-jsonl", "--resource-detail"}},
    {"run",                         {"--phases", "--config", "--cpu-profile", "--events-jsonl", "--resource-detail"}},
    {"drizzle",                     {"--config", "--events-jsonl", "--nside", "--pixfrac"}},
    {"verify",                      {"--run-manifest", "--json"}},
    {"verify profile",              {"--profile", "--json"}},
    {"graph",                       {"--config", "--preset", "--phases", "--output"}},
};

const char* kHelp =
    "astrocs --version [--json]\n"
    "astrocs hardware inspect --json\n"
    "astrocs config init --output <path>\n"
    "astrocs config validate --config <path>\n"
    "astrocs config show-effective --config <path> [--cpu-profile <path>] --json\n"
    "astrocs benchmark cpu (--quick|--full) [--output <path>] [--events-jsonl]\n"
    "astrocs verify profile --profile <path> [--json]\n"
    "astrocs doctor --json\n"
    "astrocs test synthetic --group <all|calibration|wcs_psf|noise_snr|drizzle|upm|rejection_integration|pipeline>\n"
    "astrocs phase1 run --config <path> [--cpu-profile <path>] [--events-jsonl]\n"
    "astrocs phase2 run --config <path> [--cpu-profile <path>] [--events-jsonl]\n"
    "astrocs phase3 run --config <path> [--cpu-profile <path>] [--events-jsonl]\n"
    "astrocs run --phases <1|2|3|1,2|1,2,3> --config <path> [--cpu-profile <path>] [--events-jsonl]\n"
    "astrocs verify --run-manifest <path> --json\n";

[[noreturn]] void parse_fail(const std::string& msg) { throw ParseError(msg); }

Parsed parse_args(int argc, char** argv_utf8) {
    Parsed p;
    std::vector<std::string> raw(argv_utf8 + (argc > 0 ? 1 : 0), argv_utf8 + argc);
    size_t i = 0;
    // 子命令 token: 不以 -- 开头, 逐段拼接; 命中已知命令时记录但不立即 break,
    // 继续检查是否还有更长的命令前缀(最长匹配, 支持 verify / verify profile 并存)
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
        if (known) {
            // 已匹配完整命令; 若还有下一个非 dash token 且与其拼接仍是已知命令,
            // 继续循环以取最长匹配; 否则在此 break。
            if (i + 1 >= raw.size() || raw[i + 1].empty() || raw[i + 1][0] == '-') {
                known_break = true;
                break;
            }
            const std::string joined2 = joined + " " + raw[i + 1];
            bool known2 = false;
            for (const auto& r : kRules) if (joined2 == r.path) known2 = true;
            if (!known2) {
                // 规格命令 `benchmark verify-profile <path>` 的位置参数 → 转 --profile
                if (joined == "benchmark verify-profile") {
                    p.values["--profile"] = raw[i + 1];
                    ++i;   // 消费位置参数
                }
                known_break = true;
                break;
            }
            // joined2 是已知命令 → 继续循环消费下一 token
        } else if (tokens.size() >= 2) {
            parse_fail("unknown command '" + joined + "'");
        }
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

// RT-009: 运行图路径脱敏 — 绝对路径 → "<root>/<末2组件>"；相对路径原样返回。
std::string sanitize_path(const std::string& p) {
    if (p.empty()) return p;
    std::string norm = p;
    for (auto& c : norm) if (c == '\\') c = '/';
    const bool abs = !norm.empty() && norm[0] == '/';
    if (!abs) {
        // Windows 盘符 / URI
        if (norm.size() > 1 && norm[1] == ':') return "<root>/" + norm.substr(2);
        if (norm.find("://") != std::string::npos) return "<root>/...";
        return norm;
    }
    std::vector<std::string> parts;
    std::stringstream ss(norm);
    std::string tok;
    while (std::getline(ss, tok, '/')) if (!tok.empty()) parts.push_back(tok);
    if (parts.empty()) return "<root>/";
    std::string tail;
    for (std::size_t i = (parts.size() >= 2 ? parts.size() - 2 : 0); i < parts.size(); ++i) {
        if (!tail.empty()) tail += "/";
        tail += parts[i];
    }
    return "<root>/" + tail;
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

// RT-009: 当前 git HEAD 短 SHA（sidecar source_commit）。
// 不 shell-out：读 .git/HEAD；指向 refs/ 时再读 ref 文件；解析 "ref: " 前缀。
std::optional<std::string> git_head_sha() {
    try {
        std::error_code ec;
        std::ifstream head(".git/HEAD", std::ios::in);
        if (!head) return std::nullopt;
        std::string line;
        std::getline(head, line);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
        if (line.rfind("ref: ", 0) == 0) {
            const std::string ref = line.substr(5);
            std::ifstream rf(std::filesystem::u8path(".git/" + ref), std::ios::in);
            if (!rf) return std::nullopt;
            std::getline(rf, line);
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
        }
        if (line.size() < 7) return std::nullopt;
        return line.substr(0, 12);
    } catch (...) {
        return std::nullopt;
    }
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
