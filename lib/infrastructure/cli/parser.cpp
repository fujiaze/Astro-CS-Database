// astrocs CLI — parser (RT-008 拆分自 main.cpp)
// 统一 parser + 参数校验帮助器 + CPU 指纹/hash 帮助器。
// 定义在 namespace astrocs 外(与拆分前一致); 不 include 任何 session/科学内部头。
#include "cli_common.h"

// CLI-001: 用户可见命令树唯一事实源（lib/infrastructure/cli/command_tree.h）。
// 命令名/旗标白名单/help 文本都从这里取；本文件不再自带命令清单。
// 相对路径: 根 CMakeLists.txt 的 astrocs target include 目录尚未登记本模块
// （登记属 INT-001 域），此处用工作区相对包含保证根图与 lib/infrastructure/cli/ 独立图都能编译。
#include "command_tree.h"

#include "sha256.h"
#include "cpu_routing.h"
#include "hardware_inspect.h"

#include "exit_codes.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

// ───────────────────────── parser ─────────────────────────
// struct ParseError / struct Parsed 定义见 cli_common.h(共享头)

// ── 旗标面（CLI-001）──
// 唯一白名单来自命令树表（lib/infrastructure/cli/command_tree.h 的 allowed）；
// 这里只做「token 是否需要取值」的解析判定，不再自带命令清单。
// 旧表（hardware inspect / config * / modules * / selftest / test synthetic /
// verify* / drizzle / benchmark cpu / phase1|2|3 *）已随 CLI-001 删除：这些
// 用户命令现在一律「unknown command」→ exit 2。
const std::set<std::string>& cmd_boolean_tokens() {
    static const std::set<std::string> k = {
        "--json", "--template", "--help", "-h", "-y", "--yes", "-force", "--events-jsonl",
        // §8/21_observability §8.4: 资源门 enforce 复现开关（消费者 commands.cpp）。
        "--strict-resource-gate"};
    return k;
}
const std::set<std::string>& cmd_value_tokens() {
    static const std::set<std::string> k = {"--json",  "-o",       "--output", "--template",
                                            "--cpu-profile", "--mode", "--export-mode",
                                            // §8/21_observability §8.4: 资源门 enforce 显式写法。
                                            "--on-resource-gate"};
    return k;
}
// 允许裸用（不取值）的旗标: 顶层 --version --json / doctor 的 --json /
// 子命令 --template --json（模板请求机器可读 stdout）。其余取值旗标缺值 → 2。
bool cmd_allows_bare(const std::string& token, const Parsed& p) {
    const std::string joined = p.join();
    // --template 同名两用: `<cmd> --template`（开关，模板→stdout）与
    // `<cmd> --template <path>`（取值，模板→文件；§6.2 的 -o 为等效短格式）。
    if (token == "--template") return true;
    if (token == "--json") {
        // 机器输出开关与 <config.json> 取值在不同用法下同名不同义（§6.2/§6.3）：
        //   --version --json / doctor --json        → 开关
        //   <cmd> --template --json                 → 开关（模板走 JSON 输出）
        //   <cmd> --json <config.json>              → 取值（缺值 → 2）
        const bool switch_only_command = (joined == "--version" || joined == "doctor");
        const bool template_request = p.flags.count("--template") > 0;
        return switch_only_command || template_request;
    }
    return false;
}
bool cmd_command_has_flag(const std::string& joined, const std::string& token) {
    const auto* c = astrocs::cli::cmd::find(joined);
    if (!c) return false;
    for (const auto& f : c->allowed)
        if (f == token) return true;
    return false;
}

// kRules: 命令树表的解析器视图（path + allowed）。保留 CmdRule 类型以免
// cli_common.h 的公开声明漂移；内容 100% 来自 command_tree.h。
struct CmdRuleView { std::string path; std::vector<std::string> allowed; };
const std::vector<CmdRuleView>& kRuleViews() {
    static const std::vector<CmdRuleView> k = [] {
        std::vector<CmdRuleView> v;
        for (const auto& c : astrocs::cli::cmd::commands())
            v.push_back({c.path, c.allowed});
        return v;
    }();
    return k;
}

// help 文本: 由命令树生成（禁止手写副本）。
const std::string kHelpStorage = astrocs::cli::cmd::help_text();
const char* kHelp = kHelpStorage.c_str();

[[noreturn]] void parse_fail(const std::string& msg) { throw ParseError(msg); }

Parsed parse_args(int argc, char** argv_utf8) {
    Parsed p;
    std::vector<std::string> raw(argv_utf8 + (argc > 0 ? 1 : 0), argv_utf8 + argc);
    size_t i = 0;
    // 命令 token: 逐段拼接做最长匹配（表驱动，命中即定）。命令树来自
    // command_tree.h；不在此硬编码任何命令名。
    std::vector<std::string> tokens;
    bool known_break = false;
    for (; i < raw.size(); ++i) {
        const std::string& a = raw[i];
        if (!a.empty() && a[0] == '-') {
            // 顶层 dash 命令(--version/--help/-h)视作命令本身
            if (tokens.empty() && astrocs::cli::cmd::is_command(a)) {
                tokens.push_back(a);
                known_break = true;
            }
            break;
        }
        tokens.push_back(a);
        std::string joined;
        for (const auto& t : tokens) { if (!joined.empty()) joined += ' '; joined += t; }
        if (astrocs::cli::cmd::is_command(joined)) {
            // 命中完整命令: 只有当下一 token 能拼出更长的已登记命令时才继续
            // （命令树里没有这种前缀对时立即定案，剩余 token 交旗标循环判定）。
            if (i + 1 < raw.size() && !raw[i + 1].empty() && raw[i + 1][0] != '-' &&
                astrocs::cli::cmd::count_with_prefix(joined + " " + raw[i + 1]) > 0)
                continue;
            known_break = true;
            break;
        }
        if (i + 1 >= raw.size() || raw[i + 1].empty() || raw[i + 1][0] == '-') {
            // 拼接已终结(dash token 或 EOF)仍不命中任何已登记命令 → unknown command。
            // 旧用户命令(phase1/2/3、config、modules、verify、selftest、test synthetic、
            // benchmark cpu、drizzle、hardware inspect)全部走这一条 → exit 2。
            parse_fail("unknown command '" + joined + "'");
        }
    }
    p.cmd = tokens;
    if (known_break) ++i;  // 越过已消费的最后一个命令 token
    const std::string joined = p.join();
    if (joined.empty()) parse_fail("no command given");
    const auto* rule = astrocs::cli::cmd::find(joined);
    if (!rule) parse_fail("unknown command '" + joined + "'");
    for (; i < raw.size(); ++i) {
        const std::string& a = raw[i];
        if (a.size() < 2 || a[0] != '-') parse_fail("unexpected positional argument '" + a + "'");
        if (!cmd_command_has_flag(joined, a)) parse_fail("unknown flag '" + a + "'");
        const bool value_token = cmd_value_tokens().count(a) > 0;
        if (!value_token) {
            if (!p.flags.insert(a).second) parse_fail("duplicate flag '" + a + "'");
            continue;
        }
        // 取值旗标: 后随 token 若是「本命令已知旗标」则视为缺值；否则取值。
        const bool next_is_flag =
            (i + 1 < raw.size()) && raw[i + 1].size() > 1 && raw[i + 1][0] == '-' &&
            cmd_command_has_flag(joined, raw[i + 1]);
        const bool bare_ok = cmd_allows_bare(a, p);
        if (i + 1 >= raw.size() || raw[i + 1].empty() || next_is_flag) {
            if (bare_ok) {
                if (!p.flags.insert(a).second) parse_fail("duplicate flag '" + a + "'");
                continue;
            }
            parse_fail("flag '" + a + "' requires a value");
        }
        if (p.values.count(a)) parse_fail("duplicate flag '" + a + "'");
        p.values[a] = raw[++i];
    }
    return p;
}

// ───────────────────── 参数校验帮助器 ─────────────────────

std::string need_value(const Parsed& p, const std::string& flag) {
    auto it = p.values.find(flag);
    if (it == p.values.end() || it->second.empty()) parse_fail("missing required " + flag + " <path>");
    return it->second;
}

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
// session_mode=true (phaseN run): 追加接受 RT-008 平铺直通会话格式
// (runtime_client phase_config 平铺分支), 未知键拒绝面与 V1 同强度;
// config validate 顶层命令面恒为 V1 合同 (CLI-003 golden, session_mode=false)。
int validate_config_full(const std::string& path, nlohmann::json* doc_out,
                         bool session_mode) {
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
    static const std::set<std::string> kSessionKeys = {
        // phase1 平铺 (p1_session 消费面)
        "input_lights", "master_bias", "master_dark", "master_flat",
        "dark_optimization", "dark_scale_factor", "cosmetic",
        // UNIT-001: 母版标度/归一化声明面（SCI-CAL-001 §3/§6；ALG-CAL-001 §2 标度声明表;
        // 缺声明时消费边界仍 fail-closed，见 module_adapters p1_op_calibrate）。
        "master_units", "master_scale", "master_flat_normalize", "master_flat_median_range",
        // B2-A8: Phase1 产品观测 passband 身份（空 = 显式无 filter）；写入
        // HiPS properties obs_filter，Phase2 coverage 以此分组并 fail-closed。
        "filter_passband",
        // P1-001: 真实节点域科学参数（drizzle: nside/nested/pixfrac/precision;
        // wcs: ipv 求解链参数——非 silent default, 缺失即节点 DATA 拒绝）
        "drizzle", "wcs",
        // P8-SNR-LINUX (2026-09-14): SNR 科学配置块 (gain_e_per_adu/
        //   read_noise_e/zero_point_mag/sigma_logflux_dex/n_matches 等),
        //   由 p1_op_noise 消费; 缺失 = 未知 gain/ZP (天空受限最优提取,
        //   m_5 = null)。不放宽任何既有键校验。
        "snr",
        // FIX-P1 (RELEASE-02): 测光归一化配置块 (photometry.fit.enabled/
        //   gaia_data_dir/filter/filters_json/qe_json/qe_name/max_stars),
        //   由 p1_op_photometry 消费 (I_photo=k_photo*I_cal, 02_FROZEN §7)。
        //   缺失 = 不施加 (如实中性 applied=false, 不伪造 1.0)。
        "photometry",
        // phase2 平铺 (p2_session / canonical P2 节点链 消费面)
        // B1-A4: 节点实际消费键必须可达, 否则配置被 parser 拒绝而链路不可闭合。
        // 节点侧键集（module_adapters P2NodeModule::validate_config + op 读取）:
        //   hips_paths, output_dir, upm, reject, weight_mode,
        //   legacy_allow_weight_fallback, reject_profile,
        //   persist_upm/upm_save_path（UPM 持久化落盘键）。
        "hips_paths", "upm", "upm_save_path", "persist_upm",
        "reject", "reject_profile", "weight_mode", "legacy_allow_weight_fallback",
        // phase3 平铺 (p3_session 消费面)
        "source", "center", "scale_deg_per_px", "width_px", "height_px",
        "projection", "sampler", "longitude_parity", "bitpix",
        "coverage_output", "max_tiles", "frame",
        // phase3 平铺直通特征键 (runtime_client phase_config 平铺判定)
        "output_fits_path", "sampler_used", "mode",
        // DC-401/DC-418/DC-419 (§4.5.5): 排异算法选择键（留空/0/auto = 按 n 自动；
        // 显式单算法；按 n 的分段/表达式）。CLI 只识别并透传到 pdoc（phase_config
        // 直通分支），消费与 per-pixel 路由在 scheduler 面（另一分片）。
        "algorithm_rejection_method",
        // RELEASE-02 SD-15 补白名单: 合同 schema 已声明但 CLI kSessionKeys 缺的
        // 4 键（CLI 判 unknown key 退出 3 ⇒ 配置不可达）。CLI 只识别并透传到
        // pdoc（phase_config 直通分支），科学消费在 scheduler 面。
        //   phase_config_normalize.schema.json: sparse_snr_layer / algorithm_psf_model
        //   phase_config_mosaic.schema.json:    algorithm_weight_mode / algorithm_upm_gauge
        // 键名与 config_separation_anchors.json 的 ^algorithm_* 族一致。
        "algorithm_weight_mode", "algorithm_upm_gauge",
        "algorithm_psf_model", "sparse_snr_layer",
        // DC-503 (§5.3): 输出模式 surface_brightness/point_source_flux/visualization。
        // CLI 只识别并透传到 pdoc；生产 resample/writer 的消费在 scheduler 面。
        "output_mode",
    };
    for (auto it = doc.begin(); it != doc.end(); ++it) {
        const bool allowed = kAllowedKeys.count(it.key()) != 0 ||
                             (session_mode && kSessionKeys.count(it.key()) != 0);
        if (!allowed) {
            std::fprintf(stderr, "astrocs: config has unknown key '%s'\n", it.key().c_str());
            return astrocs::INPUT;                   // 防拼写静默忽略 → 3
        }
    }
    // 平铺会话格式特征: 任一 session 键出现即脱离 V1 顶层必填面
    const bool flat_session = session_mode &&
        std::any_of(kSessionKeys.begin(), kSessionKeys.end(),
                    [&](const std::string& k) { return doc.contains(k) != 0; });
    if (!doc.contains("schema_version")) {
        if (!flat_session) {
            std::fprintf(stderr, "astrocs: config missing 'schema_version'\n");
            return astrocs::INPUT;
        }
    } else if (!doc["schema_version"].is_string() ||
               doc["schema_version"].get<std::string>() != "1") {
        std::fprintf(stderr, "astrocs: config schema_version must be \"1\"\n");
        return astrocs::ARGS;                        // 版本错=配置错 → 2
    }
    if (!flat_session && (!doc.contains("inputs") || !doc["inputs"].is_object())) {
        std::fprintf(stderr, "astrocs: config missing 'inputs' object\n");
        return astrocs::INPUT;
    }
    if (!flat_session) {
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
    }
    if (!flat_session && (!doc.contains("output_dir") || !doc["output_dir"].is_string())) {
        std::fprintf(stderr, "astrocs: config missing 'output_dir'\n");
        return astrocs::INPUT;
    }
    // FIX-E2E B1-A8: 平铺会话同样必须显式给 output_dir —— 取消隐式 CWD "." 默认，
    // 否则 phaseN run 的 manifest/资源三件套落进程 CWD（仓库根产物散落）。
    // 配置错 → 2（禁 silent default; 与节点侧 output_dir 必填一致）。
    if (flat_session &&
        (!doc.contains("output_dir") || !doc["output_dir"].is_string() ||
         doc["output_dir"].get<std::string>().empty())) {
        std::fprintf(stderr,
                     "astrocs: config missing 'output_dir' (required for phase run; "
                     "run products are written only under output_dir)\n");
        return astrocs::ARGS;                        // 2: 配置错
    }
    std::error_code ec;
    // 平铺会话格式: session 自建输出目录, CLI 仅要求为字符串; V1 顶层格式仍要求已存在。
    if (!flat_session &&
        !std::filesystem::exists(std::filesystem::u8path(doc["output_dir"].get<std::string>()), ec)) {
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
    // CPU-004: v2 profile 校验(结构 + 机器一致性: arch/quota_signature/logical_available)
    const std::string hw = astrocs::backend_host::hardware_inspect_json_v1(ASTROCS_VERSION_STRING);
    const auto verdict = astrocs::backend_host::validate_profile_v2_for_machine(
        buf.str(), ASTROCS_COMMIT_SHA, hw);
    if (!verdict.valid) {
        std::fprintf(stderr, "astrocs: cpu profile invalid: %s — rerun 'astrocs benchmark cpu'\n",
                     verdict.stale_reason.c_str());
        return astrocs::BACKEND;                     // 04: CPU 特征/损坏 → 5
    }
    *prof_out = std::move(prof);
    return astrocs::OK;
}