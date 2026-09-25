// acsd CLI — parser (RT-008 拆分自 main.cpp)
// 统一 parser + 参数校验帮助器 + CPU 指纹/hash 帮助器。
// 定义在 namespace astrocs 外(与拆分前一致); 不 include 任何 session/科学内部头 ——
// 只 include CLI 自身的单一声明头 session_commands.h（input_contract()，供 §9.71
// 三命令同构块结构派生输入判据；块内键集则与平铺形态同源 = 本文件 session_keys()，
// 禁止在 parser 内手写第二份键集）。
#include "cli_common.h"

// CLI-001: 用户可见命令树唯一事实源（lib/infrastructure/cli/command_tree.h）。
// 命令名/旗标白名单/help 文本都从这里取；本文件不再自带命令清单。
// 相对路径: 根 CMakeLists.txt 的 acsd target include 目录尚未登记本模块
// （登记属 INT-001 域），此处用工作区相对包含保证根图与 lib/infrastructure/cli/ 独立图都能编译。
#include "command_tree.h"
#include "session_commands.h"   // CLI-MULTIBLOCK: input_contract() 单一声明

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
                                            "--on-resource-gate",
                                            // G3-11: doctor 的 verify 能力旗标
                                            // --run-manifest <manifest.json>（取值；
                                            // 命令树登记见 command_tree.h value_flags()
                                            // 与 doctor 的 allowed 表；不写进 help）。
                                            "--run-manifest"};
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

// ── CLI-MULTIBLOCK（依据 ASTROCS_DESIGN.md §4.3 输入合同）：normalize 多数据块 ──
// 语义（逐条依据 §4.3）：
//   ① 一个 JSON 内可写多个数据块（block），形如「一个 main 下面写很多个函数」；
//   ② 每块自带：一组 input_lights + 一套母版 + 运行参数 + 块级 output_dir；
//   ③ 同一组校准帧和运行参数支持一组 light（不得逐帧重复写校准帧）；
//   ④ 平铺单块简写保留（只有一块时两种写法等价），两形态互斥（同时出现 → 明确报错）。
// 块内键 = 平铺会话键集（session_keys()）+ 块级键（block_keys()）；块内不得再出现 blocks。
// 本文件是「结构可达性」的唯一实现：运行期 validate_config_full 与 CLI 预检
// （subcommand.h config_structure_errors）都调本函数，禁止各写一份。

// 平铺会话键集（唯一声明）：phase1/2/3 平铺直通会话的顶层键白名单。
// 多块形态（CLI-MULTIBLOCK）的块内键 = 本集合；块级 name 见 block_only_keys()。
const std::set<std::string>& session_keys() {
    static const std::set<std::string> k = {
        // phase1 平铺 (p1_session 消费面)
        "input_lights", "master_bias", "master_dark", "master_flat",
        "dark_optimization", "dark_scale_factor", "cosmetic",
        // UNIT-001: 母版标度/归一化声明面（SCI-CAL-001 §3/§6；ALG-CAL-001 §2 标度声明表;
        // 缺声明时消费边界仍 fail-closed，见 module_adapters p1_op_calibrate）。
        "master_units", "master_scale", "master_flat_normalize", "master_flat_median_range",
        // B2-A8: Phase1 产品观测 passband 身份（空 = 显式无 filter）；写入
        // HiPS properties obs_filter，Phase2 coverage 以此分组并 fail-closed。
        "filter_passband",
        // HIPS-IDX-01（合同 = phase_config_normalize.schema.json#/$defs/storage_form）：
        // Phase1 产品落盘形态键 archive|bare（默认 archive；键缺失/空串 ⇒ 默认 + warn）。
        // 只识别并透传到 pdoc；写出侧按形态落盘 = 分阶段实现计划阶段 2 ⇒ 生产零读取
        // 期间由 eng/ci/ledgers/dead_config_keys.json 显式登记（不得静默 no-op）。
        // 仅 normalize 认本键：Phase2/Phase3 固定裸形态，其输入出现本键即 rc=3。
        "storage_form",
        // P1-001: 真实节点域科学参数（drizzle: nside/nested/pixfrac/precision;
        // wcs: ipv 求解链参数——非 silent default, 缺失即节点 DATA 拒绝）
        "drizzle", "wcs",
        // P8-SNR-LINUX (2026-09-14): SNR 科学配置块 (gain_e_per_adu/
        //   read_noise_e/zero_point_mag/sigma_logflux_dex/n_matches 等),
        //   由 p1_op_noise 消费; 缺失 = 未知 gain/ZP (天空受限最优提取,
        //   m_5 = null)。不放宽任何既有键校验。
        "snr",
        // WIRING-W34-01 (W3-CHK-PROD-WIRING): 噪声模型配置段（SCI-NOISE /
        //   docs/science/NOISE_MODEL.md §4/§5/§5a）。段内键 = 生产节点
        //   p1_noise_cfg_apply（lib/infrastructure/scheduler/src/module_adapters.cpp）
        //   实际写入 SnrNoiseModelConfig 的**封闭词表** 14 键
        //   （patch_grid/clip_sigma/spatial_field_enabled/mask_*/min_patch_samples/
        //   max_clip_rounds/source_mask_radius_px/mask_radius_scale/variance_floor/
        //   saturation_level）；缺段 = 全取冻结默认（不是错误）。
        //   合同声明 = eng/contracts/schemas/phase_config_normalize.schema.json
        //   #/$defs/noise_config。段内未知键 ⇒ 节点 rc=3（禁静默忽略）。
        "noise",
        // FIX-P1 (RELEASE-02): 测光归一化配置块 (photometry.fit.enabled/
        //   gaia_data_dir/filter/filters_json/qe_json/qe_name/max_stars),
        //   由 p1_op_photometry 消费 (I_photo=k_photo*I_cal, 02_FROZEN §7)。
        //   缺失 = 不施加 (如实中性 applied=false, 不伪造 1.0)。
        "photometry",
        // STARDET-01（B3）：星表引导检测（权威路径）配置段。段内键 = 生产节点
        //   p1_guided_cfg（module_adapters.cpp）实际读取的键集；缺段 = 全取编译期
        //   默认（mode=auto / max_stars=20000 / parity=pos / limiting_mag 派生），
        //   不是错误。合同声明 =
        //   eng/contracts/schemas/phase_config_normalize.schema.json#/$defs/star_detection_config。
        //   规范：ASTROCS_DESIGN.md §4.2（星表引导检测 = 权威范式，top 2–5 万）
        //   + docs/plugins/algorithms_phase1/03_star_detection.md §5.1。
        "star_detection",
        // phase2 平铺 (p2_session / canonical P2 节点链 消费面)
        // B1-A4: 节点实际消费键必须可达, 否则配置被 parser 拒绝而链路不可闭合。
        // 节点侧键集（module_adapters P2NodeModule::validate_config + op 读取）:
        //   hips_paths, output_dir, upm, reject, reject_profile,
        //   persist_upm/upm_save_path（UPM 持久化落盘键）。
        // §9.73 裁决 A44: weight_mode / legacy_allow_weight_fallback **已摘除**
        // （「权重模式」概念不存在；权重是消费 SNR 时的派生量）⇒ 配置里出现即 rc=3。
        "hips_paths", "upm", "upm_save_path", "persist_upm",
        "reject", "reject_profile",
        // HIPS-IDX-01（合同 = phase_config_mosaic.schema.json#/$defs/coverage_index_ref）：
        // 阶段二输入的**加性可选键**，指向数据集级覆盖索引 coverage.index.json；
        // hips_paths 元素保持字符串（逐帧索引路径按命名规则派生）。同 snr_path：
        // CLI 只识别并透传，消费点（阶段二 coverage 节点）在阶段 2 ⇒ 死键台账已登记。
        "coverage_index",
        //（GAP_AUDIT G05；ASTROCS_DESIGN §3.3「三命令通用输入合同：键名一律以
        // 命令行实际认的键为准」）：合同声明但 CLI 白名单缺的提升键落地。键名**逐字**取
        // 合同声明名（禁止新造同义键）：
        //   phase_config_mosaic.schema.json#/$defs/mosaic_config/properties/snr_path
        // 只识别并透传到 pdoc（phase_config 直通分支）；科学消费点在 scheduler 面。
        // 生产零读取期间由 eng/ci/ledgers/dead_config_keys.json 显式登记（不得静默 no-op）。
        "snr_path",
        // phase3 平铺 (p3_session 消费面)
        "source", "center", "scale_deg_per_px", "width_px", "height_px",
        "projection", "sampler", "longitude_parity", "bitpix",
        "coverage_output", "max_tiles", "frame",
        // P3-STREAM-01（ASTROCS_DESIGN §8.3 export 行 + SCHEDULER_CONTRACT §3）：
        // 导出**编排参数**（子块边长 / 有界队列深度）——与 max_tiles 同款的资源/
        // 编排键（非科学键，不改任何公式与容差），生产消费点 = module_adapters 的
        // phase3 resample2/writer/verify 节点（p3n_sub_block_px）。
        "sub_block_px", "queue_depth",
        //GAP_AUDIT N03：export 提升键（合同声明名逐字，平铺顶层，与
        // center/scale_deg_per_px 同面）：
        //   phase_config_export.schema.json#/$defs/export_wcs/properties/{rotation_deg,crpix_px}
        // 同 snr_path：CLI 只识别并透传，生产消费点未落地 ⇒ 死键台账已登记。
        "rotation_deg", "crpix_px",
        // EXPORT-CROP-01（依据 docs/design/PHASE3_DETAILED_DESIGN.md §8.1/§8.2）：
        // 导出裁剪范围键（可选，缺省不裁剪）。
        // 合同声明 = phase_config_export.schema.json#/$defs/export_crop（键名逐字取
        // 合同声明名，禁新造同义键）；几何唯一实现 = lib/algorithms/projection/p3_wcs.h
        // （CLI 配置面与节点面共用）；生产消费点 = scheduler 的 p3 wcs/writer/verify
        // 节点（P3NodeModule::validate_config + p3_op_wcs/writer/verify）⇒ 不是死键。
        "crop",
        // 计算精度口径（ASTROCS_DESIGN §3.3:256）：阶段一 =
        // drizzle.precision_mode(0=FP32/1=FP64)；阶段二/三 = 位深键 bitpix(-32/-64)。
        // **不新造 precision(fp32/fp64) 同义键**——合同旧键 precision 由死键台账登记
        // （eng/ci/ledgers/dead_config_keys.json#dead_config_key:precision），CLI 面拒绝。
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
        //   phase_config_mosaic.schema.json:    algorithm_upm_gauge
        // §9.73 裁决 A44: algorithm_weight_mode **已摘除**（同 weight_mode）。
        // algorithm_upm_gauge 保留至 §9.71 定案 5 契约面整体重整（前台面）。
        // 键名与 config_separation_anchors.json 的 ^algorithm_* 族一致。
        "algorithm_upm_gauge",
        "algorithm_psf_model", "sparse_snr_layer",
        // DC-503 (§5.3): 输出模式 surface_brightness/point_source_flux/visualization。
        // CLI 只识别并透传到 pdoc；生产 resample/writer 的消费在 scheduler 面。
        "output_mode",
    };
    return k;
}

// 块内允许键 = 平铺会话键集 + 块级键：
//   name       —— 可选；manifest/日志的块归属标识（§9.68 建议形态）；
//   output_dir —— 必填；块级运行产物落点（顶层平铺时是简写的运行级 output_dir，
//                 多块形态下**只允许**写在块内 —— 两形态互斥由下面判据强制）。
const std::set<std::string>& block_keys() {
    static const std::set<std::string> k = {"name", "output_dir"};
    return k;
}

// 退役的逐帧形态判别（§9.68 否决项）{phase_name, config, inputs[]}：
//   phase_name 顶层键（该形态的 phase 判别键），或 inputs 为**数组**
//   （V1 形态的 inputs 是对象；逐帧形态才是每帧一个对象的数组）。
// 唯一实现：运行期 validate_config_full 与 CLI 预检同源（同文案、同退出码 3）。
bool config_is_retired_perframe_form(const nlohmann::json& doc) {
    if (!doc.is_object()) return false;
    return doc.contains("phase_name") || (doc.contains("inputs") && doc["inputs"].is_array());
}

// 迁移提示（预检页与运行期共用同一份文案，禁止各写一份）。按会话给：
//   * normalize：§9.68 否决的「每帧一个对象」旧写法 → 指向多块形态；
//   * mosaic/export：合同形态 {phase_name, config, inputs[]}（键名与 CLI 不一致）→ 指向
//     §9.71 裁决 2（块状结构统一；键名方案属定案 4 的前台裁量面）。
std::string retired_perframe_form_message(const std::string& session_name) {
    if (session_name != "normalize") {
        return "config uses the phase_config contract form {phase_name, config, inputs[]}, "
               "which is not the CLI session form — " + session_name +
               " moves to the same block structure as normalize "
               "(GAP_AUDIT §9.71 ruling 2: one block = {output name, run parameters, "
               "one group of input frames}); the unified key-name scheme is pending "
               "(ruling 2 定案 4). Use the block form "
               "{\"schema_version\":\"1\",\"blocks\":[{...}]} once it lands; "
               "see docs/contracts/CONFIG_CONTRACT.md §3";
    }
    return "config uses the retired per-frame phase_config form "
           "{phase_name, config, inputs[]} — one entry per light is no longer supported "
           "(GAP_AUDIT §9.68); migrate to the multi-block form: "
           "{\"schema_version\":\"1\",\"blocks\":[{\"name\":\"<label>\","
           "\"input_lights\":[...],\"master_bias\":\"...\",\"master_dark\":\"...\","
           "\"master_flat\":\"...\",\"output_dir\":\"...\"}]} — one block per group of "
           "lights sharing the same masters (ASTROCS_DESIGN.md §3.3)";
}

bool config_has_blocks(const nlohmann::json& doc) {
    return doc.is_object() && doc.contains("blocks");
}

// 平铺单块简写特征：任一平铺会话键，或运行级 output_dir（简写必填、块形态禁顶层写）。
bool config_has_flat_session_keys(const nlohmann::json& doc) {
    if (!doc.is_object()) return false;
    if (doc.contains("output_dir")) return true;
    for (const auto& k : session_keys())
        if (doc.contains(k)) return true;
    return false;
}

// 多块形态结构校验（唯一实现）。判据（§9.68 + 现有 flat_session 纪律）：
//   * blocks 与平铺键互斥（同时出现 → 报错，不静默取一）；
//   * blocks 必须是非空数组，每项必须是对象；
//   * 每块必须有非空 input_lights（非空字符串数组）与非空字符串 output_dir（禁 silent default）；
//   * 块间 output_dir 不得重复 —— 否则两块会写同一份 run manifest，违反
//     「每块独立 output_dir / 独立 manifest」；
//   * 块内未知键拒绝（键集 = 平铺会话键集 session_keys() + block_keys() = {name, output_dir}）；
//     未知键先于运行期，与顶层 unknown key 同码（3）。
// 母版路径存在性按现有纪律：不在本函数判盘（= 平铺同款），由 CLI 预检
// （subcommand.h input_path_errors）逐块核磁盘；-force 越过预检的后果由用户承担。
std::vector<std::string> session_blocks_errors(const std::string& session_name,
                                               const nlohmann::json& doc, int* exit_code,
                                               bool include_unknown_keys) {
    std::vector<std::string> errs;
    if (exit_code) *exit_code = astrocs::ARGS;             // 2: 结构/配置错
    if (!doc.is_object() || !doc.contains("blocks")) {
        errs.push_back("blocks 缺失或配置不是 JSON 对象");
        return errs;
    }
    // §9.71 裁决 2：三命令同构块结构（一个块 = 输出名称 + 运行参数 + 一组输入帧）。
    // 块内键集与「输入帧键」**从单一来源派生**，禁止手写副本：
    //   * 键集 = 平铺会话键集 session_keys() + block_keys()（name/output_dir）
    //     —— §3.3「两形态等价」：块内门与平铺门必须是**同一份**键表。
    //     旧实现用该会话 config_fields()（= --template 骨架键表）派生块内键，两门不等价：
    //     normalize 块内曾拒绝 10 个本阶段科学键（dark_optimization / dark_scale_factor /
    //     cosmetic / master_units / master_scale / master_flat_normalize /
    //     master_flat_median_range / photometry / sparse_snr_layer / algorithm_psf_model），
    //     mosaic 4 个、export 5 个；真实 testdata 多块形态因此无任何可运行配置
    //     （留键 ⇒ rc=3 unknown key；去键 ⇒ cal 节点 fail-closed rc=2）。平铺门本身即
    //     session_keys() 的并集，故块内门同源 = 两形态等价（机器判据见
    //     eng/tests/cli/test_fix210_block_key_parity.py）。
    //   * 输入判据 = input_contract(session)（数组形态 or 对象形态 + 必需子键）；
    //   * §9.73 裁决 A44（「权重模式」概念不存在）⇒ 块面不收 weight_mode /
    //     legacy_allow_weight_fallback（派生量，由 Phase2 消费 SNR 时现场算）。
    //   * schema_version 由两形态各自单列（顶层 kAllowedKeys）⇒ 块内不收。
    const astrocs::cli::cmd::SessionId sess = astrocs::cli::cmd::session_of(session_name);
    std::set<std::string> allowed = block_keys();
    for (const auto& k : session_keys()) allowed.insert(k);
    const astrocs::cli::cmd::InputContract& ic = astrocs::cli::cmd::input_contract(sess);
    if (config_has_flat_session_keys(doc)) {
        errs.push_back("config mixes 'blocks' with flat single-block keys "
                       "(output_dir/input_lights/master_*/...): the two forms are mutually "
                       "exclusive — keep either blocks[] or the flat single-block shorthand "
                       "(ASTROCS_DESIGN.md §3.3; GAP_AUDIT §9.68)");
    }
    const auto& blocks = doc["blocks"];
    if (!blocks.is_array() || blocks.empty()) {
        errs.push_back("blocks must be a non-empty array");
        return errs;
    }
    std::set<std::string> seen_out;
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const std::string at = "blocks[" + std::to_string(i) + "]";
        const auto& b = blocks[i];
        if (!b.is_object()) {
            errs.push_back(at + " must be a JSON object");
            continue;
        }
        if (ic.object_field != nullptr) {
            // 对象形态（如 export 的 source.hips_dir）：块 = 一组输入帧的入口对象。
            if (!b.contains(ic.key) || !b[ic.key].is_object()) {
                errs.push_back(at + "." + ic.key + " must be an object {\"" +
                               ic.object_field + "\": \"<path>\"} "
                               "(one block = one group of input frames)");
            } else if (!b[ic.key].contains(ic.object_field) ||
                       !b[ic.key][ic.object_field].is_string() ||
                       b[ic.key][ic.object_field].get<std::string>().empty()) {
                errs.push_back(at + "." + ic.key + "." + ic.object_field +
                               " must be a non-empty string "
                               "(one block = one group of input frames)");
            }
        } else if (!b.contains(ic.key) || !b[ic.key].is_array() || b[ic.key].empty()) {
            errs.push_back(at + "." + ic.key + " must be a non-empty array "
                                 "(one block = one group of input frames)");
        } else {
            for (const auto& l : b[ic.key]) {
                if (!l.is_string() || l.get<std::string>().empty()) {
                    errs.push_back(at + "." + ic.key + " items must be non-empty strings");
                    break;
                }
            }
        }
        if (!b.contains("output_dir") || !b["output_dir"].is_string() ||
            b["output_dir"].get<std::string>().empty()) {
            errs.push_back(at + ".output_dir is required (block-level, no silent default; "
                                 "each block writes its own products and run manifest)");
        } else if (!seen_out.insert(b["output_dir"].get<std::string>()).second) {
            errs.push_back(at + ".output_dir duplicates an earlier block's output_dir ('" +
                           b["output_dir"].get<std::string>() +
                           "'): blocks must not share output_dir (each block needs its own "
                           "run manifest)");
        }
        if (b.contains("blocks")) errs.push_back(at + ".blocks is not allowed inside a block");
    }
    if (!errs.empty()) return errs;                        // 结构错优先（与平铺同序）
    if (!include_unknown_keys) return errs;                // 预检页：结构错面
    // 块内未知键（结构可达才报；与顶层 unknown key 同码 3）
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const std::string at = "blocks[" + std::to_string(i) + "]";
        for (auto it = blocks[i].begin(); it != blocks[i].end(); ++it) {
            if (allowed.count(it.key()) == 0) {
                errs.push_back(at + " has unknown key '" + it.key() + "'");
            }
        }
    }
    if (!errs.empty() && exit_code) *exit_code = astrocs::INPUT;   // 3: 未知键
    return errs;
}

// pipeline_config.json v1 全量校验(合同: docs/api/MANIFEST_VERIFY_V1.md §1)
// 返回 0 有效(doc 填充); 否则对应退出码, 诊断写 stderr。
// session_mode=true (phaseN run): 追加接受两种会话格式 —— RT-008 平铺直通
// (runtime_client phase_config 平铺分支) 与 CLI-MULTIBLOCK 多块形态 (顶层 blocks[],
// §9.68)；未知键拒绝面与 V1 同强度;
// config validate 顶层命令面恒为 V1 合同 (CLI-003 golden, session_mode=false)。
int validate_config_full(const std::string& path, nlohmann::json* doc_out,
                         bool session_mode, const std::string& session_name) {
    std::ifstream f(std::filesystem::u8path(path), std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "acsd: config not found '%s'\n", path.c_str());
        return astrocs::INPUT;                       // 04: 输入缺失 → 3
    }
    std::stringstream buf; buf << f.rdbuf();
    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(buf.str());
    } catch (const nlohmann::json::parse_error& e) {
        std::fprintf(stderr, "acsd: config malformed JSON: %s\n", sanitize(e.what()).c_str());
        return astrocs::INPUT;                       // 04: 格式错 → 3
    }
    if (!doc.is_object()) {
        std::fprintf(stderr, "acsd: config is not a JSON object\n");
        return astrocs::INPUT;
    }
    static const std::set<std::string> kAllowedKeys = {"schema_version", "inputs",
                                                       "output_dir", "phase3"};
    const std::set<std::string>& kSessionKeys = session_keys();
    // CLI-MULTIBLOCK（§9.68 否决项）: 退役的逐帧形态 {phase_name, config, inputs[]}
    // 必须**明确拒绝并给迁移提示**（不静默当成一条 unknown key 一笔带过）。
    // 判别：phase_name 顶层键（该形态的 phase 判别键），或 inputs 为**数组**
    // （V1 形态的 inputs 是对象；逐帧形态才是每帧一个对象的数组）。
    if (session_mode && config_is_retired_perframe_form(doc)) {
        std::fprintf(stderr, "acsd: %s\n",
                     retired_perframe_form_message(session_name).c_str());
        return astrocs::INPUT;                       // 3: 配置形态不可用
    }
    for (auto it = doc.begin(); it != doc.end(); ++it) {
        // blocks 只在会话面（phaseN run / 预检）可达；V1 顶层合同（config validate 面）
        // 仍以 kAllowedKeys 为唯一白名单。
        const bool allowed = kAllowedKeys.count(it.key()) != 0 ||
                             (session_mode &&
                              (kSessionKeys.count(it.key()) != 0 || it.key() == "blocks"));
        if (!allowed) {
            std::fprintf(stderr, "acsd: config has unknown key '%s'\n", it.key().c_str());
            return astrocs::INPUT;                   // 防拼写静默忽略 → 3
        }
    }
    // 平铺会话格式特征: 任一 session 键出现即脱离 V1 顶层必填面
    const bool flat_session = session_mode &&
        std::any_of(kSessionKeys.begin(), kSessionKeys.end(),
                    [&](const std::string& k) { return doc.contains(k) != 0; });
    // CLI-MULTIBLOCK: 多块形态（顶层 blocks）与平铺单块简写互斥；逐块结构校验
    // （非空 input_lights + 块级 output_dir + 块内未知键 + output_dir 不重复）。
    const bool has_blocks = session_mode && config_has_blocks(doc);
    if (has_blocks) {
        int bcode = astrocs::ARGS;
        const std::vector<std::string> berrs = session_blocks_errors(session_name, doc, &bcode);
        if (!berrs.empty()) {
            for (const auto& e : berrs) std::fprintf(stderr, "acsd: %s\n", e.c_str());
            return bcode;                            // 2: 结构错 / 3: 块内未知键
        }
    }
    // 会话面（平铺简写 或 多块）= 脱离 V1 顶层必填面
    const bool session_form = flat_session || has_blocks;
    if (!doc.contains("schema_version")) {
        if (!session_form) {
            std::fprintf(stderr, "acsd: config missing 'schema_version'\n");
            return astrocs::INPUT;
        }
    } else if (!doc["schema_version"].is_string() ||
               doc["schema_version"].get<std::string>() != "1") {
        std::fprintf(stderr, "acsd: config schema_version must be \"1\"\n");
        return astrocs::ARGS;                        // 版本错=配置错 → 2
    }
    if (!session_form && (!doc.contains("inputs") || !doc["inputs"].is_object())) {
        std::fprintf(stderr, "acsd: config missing 'inputs' object\n");
        return astrocs::INPUT;
    }
    if (!session_form) {
        for (const char* k : {"lights", "darks", "flats", "bias"}) {
            auto it = doc["inputs"].find(k);
            if (it == doc["inputs"].end() || !it->is_array()) {
                std::fprintf(stderr, "acsd: config inputs.%s must be an array\n", k);
                return astrocs::INPUT;
            }
            for (const auto& e : *it) {
                if (!e.is_string() || e.get<std::string>().empty()) {
                    std::fprintf(stderr, "acsd: config inputs.%s has empty path\n", k);
                    return astrocs::INPUT;
                }
                std::error_code ec;
                if (!std::filesystem::exists(std::filesystem::u8path(e.get<std::string>()), ec)) {
                    std::fprintf(stderr, "acsd: config input not found '%s'\n",
                                 e.get<std::string>().c_str());
                    return astrocs::INPUT;
                }
            }
        }
    }
    if (!session_form && (!doc.contains("output_dir") || !doc["output_dir"].is_string())) {
        std::fprintf(stderr, "acsd: config missing 'output_dir'\n");
        return astrocs::INPUT;
    }
    // FIX-E2E B1-A8: 平铺会话同样必须显式给 output_dir —— 取消隐式 CWD "." 默认，
    // 否则 phaseN run 的 manifest/资源三件套落进程 CWD（仓库根产物散落）。
    // 配置错 → 2（禁 silent default; 与节点侧 output_dir 必填一致）。
    if (flat_session &&
        (!doc.contains("output_dir") || !doc["output_dir"].is_string() ||
         doc["output_dir"].get<std::string>().empty())) {
        std::fprintf(stderr,
                     "acsd: config missing 'output_dir' (required for phase run; "
                     "run products are written only under output_dir)\n");
        return astrocs::ARGS;                        // 2: 配置错
    }
    std::error_code ec;
    // 平铺会话格式: session 自建输出目录, CLI 仅要求为字符串; V1 顶层格式仍要求已存在。
    // 多块形态不走本条：output_dir 在块级（session_blocks_errors 已逐块校验）。
    if (!session_form &&
        !std::filesystem::exists(std::filesystem::u8path(doc["output_dir"].get<std::string>()), ec)) {
        std::fprintf(stderr, "acsd: config output_dir not found\n");
        return astrocs::INPUT;
    }
    *doc_out = std::move(doc);
    return astrocs::OK;
}

// cpu profile 独立文件校验(分离原则): 结构(3)/stale(5) — profile hash 不与 config 混算
int validate_cpu_profile(const std::string& path, nlohmann::json* prof_out) {
    std::ifstream f(std::filesystem::u8path(path), std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "acsd: cpu profile not found '%s'\n", path.c_str());
        return astrocs::INPUT;
    }
    std::stringstream buf; buf << f.rdbuf();
    nlohmann::json prof;
    try {
        prof = nlohmann::json::parse(buf.str());
    } catch (const nlohmann::json::parse_error& e) {
        std::fprintf(stderr, "acsd: cpu profile malformed JSON: %s\n", sanitize(e.what()).c_str());
        return astrocs::INPUT;
    }
    // CPU-004: v2 profile 校验(结构 + 机器一致性: arch/quota_signature/logical_available)
    const std::string hw = astrocs::backend_host::hardware_inspect_json_v1(ASTROCS_VERSION_STRING);
    const auto verdict = astrocs::backend_host::validate_profile_v2_for_machine(
        buf.str(), ASTROCS_COMMIT_SHA, hw);
    if (!verdict.valid) {
        std::fprintf(stderr, "acsd: cpu profile invalid: %s — rerun 'acsd benchmark cpu'\n",
                     verdict.stale_reason.c_str());
        return astrocs::BACKEND;                     // 04: CPU 特征/损坏 → 5
    }
    *prof_out = std::move(prof);
    return astrocs::OK;
}