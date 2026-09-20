// lib/infrastructure/cli/session_commands.h — 命令层 ↔ 会话层路由（CLI-001）
//
// 「用户命令名 → 内部会话」的机械映射唯一落点。三个用户命令（normalize /
// mosaic / export）是**平级独立命令**（ASTROCS_DESIGN §1.2）：本表只做名字
// 映射，不表达顺序、不串接、不共享进程状态；每个命令各起一个进程、各自
// 校验、各自恢复。
//
// 数值会话号与既有 pipeline/session 内部编号一致（内部指代）；用户可见字符串
// 见 kSessionCommands。本文件不出现任何旧用户命令名。
#pragma once

#include <string>
#include <vector>

#include "command_tree.h"

namespace astrocs::cli::cmd {

struct SessionCommand {
    const char* name;    // 用户可见命令名
    SessionId   session; // 内部会话号（1/2/3）
    const char* op_hint; // 会话内部操作族（诊断用；不参与用户命令面）
};

inline const std::vector<SessionCommand>& session_commands() {
    static const std::vector<SessionCommand> k = {
        {"normalize", SESSION_NORMALIZE, "calibration+normalize"},
        {"mosaic",    SESSION_MOSAIC,    "mosaic+integration"},
        {"export",    SESSION_EXPORT,    "projection+export"},
    };
    return k;
}

inline bool is_session_command(const std::string& name) {
    for (const auto& s : session_commands())
        if (name == s.name) return true;
    return false;
}

inline SessionId session_of(const std::string& name) {
    for (const auto& s : session_commands())
        if (name == s.name) return s.session;
    return static_cast<SessionId>(0);
}

// 会话内部名称（诊断/事件 phase 字段用）。仅在需要人类可读的会话标识时使用；
// 用户命令名一律用 SessionCommand::name。
inline std::string session_internal_name(SessionId s) {
    switch (s) {
        case SESSION_NORMALIZE: return "session1";
        case SESSION_MOSAIC:    return "session2";
        case SESSION_EXPORT:    return "session3";
        default:                return "session?";
    }
}

// 会话**用户命令名**（normalize/mosaic/export）。用于把会话身份传给
// parser.cpp 的形态判定（退役形态迁移提示、块形态支持面），避免各调用点手写字符串。
inline const char* session_cli_name(SessionId s) {
    switch (s) {
        case SESSION_NORMALIZE: return "normalize";
        case SESSION_MOSAIC:    return "mosaic";
        case SESSION_EXPORT:    return "export";
        default:                return "?";
    }
}

// ── 会话输入判据的唯一声明（CLI-002 / GAP-034） ──
//
// 预检（subcommand.h precheck_config）与模板（config_template）都从本表取
// 「哪个顶层键 + 什么形态 = 有输入产品」。表内字段必须与运行期会话
// parse_request 实际消费的字段逐字一致，否则预检放行/拒绝会与运行期分叉：
//   * normalize → input_lights（p1_session 消费的路径数组；多块形态下块内同名键，
//                 见 config_fields 的 scope=="block" —— 预检逐块判定）
//   * mosaic    → hips_paths  （p2_session 消费的路径数组）
//   * export    → source.hips_dir（p3_session.cpp parse_request 的对象字段；
//                权威 docs/api/PHASE3_API_V1.md §请求 与 MANIFEST_VERIFY_V1 样例）
// GAP-034 事实: 旧 precheck 把 source 当 array|string 计数（对象形态 n=0 → 恒
// 「source 为空」），而 export 模板又写 "source": ""/"center": [..]，三处口径互斥，
// 导致 export 无任何配置可在不加 -force 时进入会话。现收敛为「同一声明 + 同一字段」。
struct InputContract {
    const char* key;           // 配置顶层键
    const char* object_field;  // 对象形态下的必需非空字符串子键；nullptr = 键本身即路径列表
    const char* note;          // 预检缺失提示（人可读，不含科学判定）
};

inline const InputContract& input_contract(SessionId s) {
    static const InputContract kNormalize = {"input_lights", nullptr,
                                             "没有输入产品"};
    static const InputContract kMosaic = {"hips_paths", nullptr, "没有输入产品"};
    static const InputContract kExport = {"source", "hips_dir",
                                          "export 不假设输入来自 mosaic"};
    switch (s) {
        case SESSION_NORMALIZE: return kNormalize;
        case SESSION_MOSAIC:    return kMosaic;
        default:                return kExport;   // SESSION_EXPORT
    }
}

// ── 会话配置字段的唯一声明（模板 + --help 字段说明同源；CLI-002/GAP-034/E2E-D02） ──
//
// 一次声明同时供给两处输出，禁止各写一份（手写副本必然漂移，CLI-001 前身正是
// help 与命令表各自维护）：
//   * config_template()      —— §6.1 --template 的 JSON 骨架
//   * config_field_help()    —— --help 的字段说明（CLI_PROTOCOL_V1 §1「子命令帮助
//                               与字段说明」；root-scan CLI-11）
// json == nullptr 的字段只进说明、不进模板（例如 wcs：显式 WCS 与 gaia_data_dir
// 二选一，模板不替用户填任何天测值 —— 伪造 WCS 会静默污染真实数据）。
// 科学默认值不写进模板（唯一家在程序根 config/defaults.json），表中出现的字面量
// 只有「结构骨架」与显式无默认要求键（如 drizzle.precision_mode）。
// FIX-203 例外（唯一，逐条登记）：提升键 snr_path / rotation_deg 的模板值取自
// **合同已登记的默认**（phase_config_mosaic.schema.json#snr_path.default +
// config/defaults.json#snr.path；14_projection.md:38 rotation 默认 0），目的是让
// 合同键在 --template 里可见（FIX-203 步骤 5 / 验收门：模板必须含新键）；默认值的
// 唯一家仍是 schema / defaults.json，本表只是显示，不新增第二份来源。
// crpix_px 的 [512.5, 512.5] 是随模板几何（1024×1024）推出的示例占位，非数值默认。
// scope（CLI-MULTIBLOCK, GAP_AUDIT §9.68）：
//   "top"   —— 配置顶层键（单块简写/所有命令）
//   "block" —— normalize 多块形态的**块内**键（模板里组装进 blocks[] 的每一项；
//              --help 以 blocks[].<key> 形式列出，与模板同源）
// 默认 "top"：mosaic/export 与 normalize 的顶层字段无需逐条改写。
struct ConfigField {
    const char* key;   // 键名（说明行用点号形式可自嵌套）
    const char* json;  // 模板中的 JSON 值文本；nullptr = 不写入模板
    const char* doc;   // --help 字段说明
    const char* scope = "top";   // "top" | "block"
};

// 多块模板的第二块示例取值覆盖（键 → JSON 文本）。两块示例展示 §9.68 的实际用途：
// 多通道（红/氢）各带自己的平场与运行参数，但**共用同一组 bias/dark 母版**
// 与同一个 Gaia 目录 —— 「同一组校准帧和运行参数支持一组 light」。
// name 是块归属标识（日志/run manifest 的 block.name），不是运行参数。
inline const std::vector<std::pair<const char*, const char*>>& config_second_block_overrides() {
    static const std::vector<std::pair<const char*, const char*>> k = {
        {"name", "\"ha\""},
        {"input_lights", "[\"path/to/light3.fits\"]"},
        {"master_flat", "\"path/to/flat_ha.fits\""},
        {"output_dir", "\"path/to/out/ha\""},
        {"filter_passband", "\"Baader 7nm H-alpha\""},
    };
    return k;
}

inline const std::vector<ConfigField>& config_fields(SessionId s) {
    // drizzle.precision_mode 必须显式（docs/algorithms/DRIZZLE_GEOMETRY.md B2-A12：
    // 0=FP32/1=FP64，缺失即 DATA 拒绝，不 silent 降精度）；模板取 1（FP64）与
    // RESCUE-FD-02 的库边界缺省一致（宁可慢，不静默丢精度）。
    // CLI-MULTIBLOCK（GAP_AUDIT §9.68 负责人裁决 2026-09-20）：normalize 配置 =
    // 多数据块 JSON。模板 = 两块示例；块内键 = 平铺会话键集（与 parser.cpp
    // session_keys() 同面）。平铺单块简写保留（单块时两种写法等价），两形态互斥。
    static const std::vector<ConfigField> kNormalize = {
        {"schema_version", "\"1\"", "配置合同版本（恒 \"1\"）"},
        {"name", "\"red\"",
         "块归属标识（可选；写入日志与 run manifest 的 block.name，便于多块归属）", "block"},
        {"input_lights", "[\"path/to/light1.fits\", \"path/to/light2.fits\"]",
         "亮场 FITS 路径数组（必填非空）= 一大组 light；本块的母版与运行参数支持整组，"
         "不得逐帧重复写校准帧（§9.68）", "block"},
        {"master_bias", "\"path/to/bias.fits\"",
         "本块 bias master FITS 路径；缺 → 预检 error（仅 -force 可越）", "block"},
        {"master_dark", "\"path/to/dark.fits\"",
         "本块 dark master FITS 路径；缺 → 预检 error（仅 -force 可越）", "block"},
        {"master_flat", "\"path/to/flat_red.fits\"",
         "本块 flat master FITS 路径；缺 → 预检 error（仅 -force 可越）", "block"},
        {"output_dir", "\"path/to/out/red\"",
         "本块运行产物唯一落点（必填非空字符串；块级，禁 silent default；"
         "一块 = 一次运行 = 一份 run manifest）", "block"},
        {"drizzle",
         "{\"nested\": 1, \"pixfrac\": 1.0, \"precision_mode\": 1}",
         "drizzle 累加参数；nside 缺省即 auto（由最细输入采样派生，"
         "docs/algorithms/DRIZZLE_GEOMETRY.md §3），显式填 nside 或 nside.mode 则按 explicit 校验；"
         "precision_mode 必须显式 0=FP32/1=FP64（无 silent 缺省）", "block"},
        {"filter_passband", "\"Baader R\"",
         "观测滤镜/波段标识（空 = 显式无 filter；非空必须逐字命中 config/filters.json）；"
         "本块整组 light 共用（§9.68）", "block"},
        {"wcs", "{\"gaia_data_dir\": \"path/to/gaia\"}",
         "指向来源与解算输入：填 wcs.gaia_data_dir 走真实 IPV 解算；"
         "或改为显式线性 WCS 八参数 crpix1/crpix2/crval1/crval2/cd11/cd12/cd21/cd22", "block"},
        {"wcs.init_source", nullptr,
         "可选指向来源 header_pointing|config|neighbor_crval（缺省 header_pointing）", "block"},
        {"snr", nullptr,
         "可选 SNR 科学块（gain_e_per_adu/read_noise_e/zero_point_mag 等）", "block"},
    };
    static const std::vector<ConfigField> kMosaic = {
        {"schema_version", "\"1\"", "配置合同版本（恒 \"1\"）"},
        {"hips_paths", "[]", "输入 HiPS 产品目录数组（必填非空；properties 严格校验）"},
        {"output_dir", "\".\"", "运行产物唯一落点（必填非空字符串）"},
        // FIX-203（GAP_AUDIT G05；ASTROCS_DESIGN §3.3「三命令通用输入合同：键名一律以
        // 命令行实际认的键为准」）：合同声明但 CLI 不认的提升键落地。键名**逐字**取合同
        // 声明名（禁止新造同义键）：
        //   contracts/schemas/phase_config_mosaic.schema.json#/$defs/mosaic_config/properties/snr_path
        // 模板值 = 合同默认 sparse_reconstruct（schema default 与 config/defaults.json#snr.path
        // 同值；本表只作可运行骨架，默认值唯一家仍是 schema/defaults.json，不构成第二份来源）。
        // ⚠ 生产科学消费点尚未落地（Phase2 SNR 消费 = FIX-SCI-SNR-CANON-001 §4.2 跟随项）
        //   ⇒ 已在 ci/ledgers/dead_config_keys.json 登记为「合同声明但生产零读取」，不得静默 no-op。
        {"snr_path", "\"sparse_reconstruct\"",
         "Phase2 SNR 重建/消费路径 dense|sparse_reconstruct|frame_reconstruct"
         "（默认 sparse_reconstruct = 消费 Phase1 稀疏控制点 SNR 层重建稠密 SNR；"
         "合同声明 = phase_config_mosaic.schema.json；生产消费点未落地，见死键台账）"},
        // FIX-203 精度口径（ASTROCS_DESIGN §3.3:256）：阶段二/三 = 位深键 bitpix(-32/-64)，
        // 阶段一 = drizzle.precision_mode(0/1)。**不新造 precision(fp32/fp64) 同义键**。
        // §9.73 裁决 A44（「权重模式」概念不存在）: 原 weight_mode / legacy_allow_weight_fallback
        // 两键**已从 CLI 配置面摘除**（模板/help/白名单同撤）。权重是 Phase2 消费 SNR 时的
        // 派生量（帧级 SNR + 稀疏相对 SNR 比），不是配置项；实现/冻结面（module_adapters /
        // v6 provenance / mode gate legacy 分支）由另一分片处置。
        {"upm", nullptr, "可选 UPM 配置块（拒绝/拟合参数）"},
        {"reject", nullptr, "可选 rejection 配置块；reject_profile 选择档位"},
        // DC-401/DC-418/DC-419（§4.5.5）: CLI 只识别并透传，不判科学值域；
        // 留空/0/auto = 按输出像素的 n 自动选择，显式算法名 = 强制，
        // 分段/表达式 = 用户自定义映射（覆盖内置）。消费在 scheduler 面。
        {"algorithm_rejection_method", nullptr,
         "可选：排异算法选择（留空/0/auto=按 n 自动；显式算法名=强制；"
         "按 n 的分段/表达式=用户映射覆盖内置）；方法名不存在/表达式语法错 → error"},
    };
    static const std::vector<ConfigField> kExport = {
        {"schema_version", "\"1\"", "配置合同版本（恒 \"1\"）"},
        {"source", "{\"hips_dir\": \"\"}",
         "输入 HiPS 根目录（必填；对象字段 hips_dir，非字符串）"},
        {"output_dir", "\".\"", "运行产物唯一落点（必填非空字符串）"},
        {"center", "{\"ra_deg\": 0.0, \"dec_deg\": 0.0}",
         "投影中心（对象字段 ra_deg/dec_deg；|dec| ≤ 85°，TAN 极区排除）"},
        {"width_px", "1024", "输出宽度（1..20000）"},
        {"height_px", "1024", "输出高度（1..20000）"},
        {"scale_deg_per_px", "0.001", "输出像素尺度（度/像素，必须 > 0）"},
        // FIX-203（GAP_AUDIT N03；ASTROCS_DESIGN §3.3 键名以 CLI 实际认的键为准）：
        // 合同声明但 CLI 不认的提升键落地。键名**逐字**取合同声明名（禁止新造同义键）：
        //   contracts/schemas/phase_config_export.schema.json#/$defs/export_wcs/properties/{rotation_deg,crpix_px}
        // 平铺顶层与 CLI export 既有几何键（center / scale_deg_per_px / width_px / height_px）同面
        // —— 三命令通用输入合同「块内运行参数平铺、取消 config 子对象」。
        // rotation_deg 模板值 0.0 = 合同默认（docs/plugins/algorithms_phase3/14_projection.md:38）。
        // crpix_px 模板值 [512.5, 512.5] = 本模板 1024×1024 输出的几何中心（FITS 1-based，
        // (1+1024)/2；「缺省 = 中心」），是示例占位、不是数值默认。
        // ⚠ 两键生产科学消费点尚未落地（p3 投影/重采样面）⇒ 已在
        //   ci/ledgers/dead_config_keys.json 登记为「合同声明但生产零读取」，不得静默 no-op。
        {"rotation_deg", "0.0",
         "投影旋转角（度；合同默认 0，见 phase_config_export.schema.json；"
         "生产消费点未落地，见死键台账）"},
        {"crpix_px", "[512.5, 512.5]",
         "参考像素（FITS 1-based；示例值 = 本模板 1024×1024 输出的几何中心，缺省 = 中心；"
         "合同声明 = phase_config_export.schema.json；生产消费点未落地，见死键台账）"},
        // FIX-203 精度口径（ASTROCS_DESIGN §3.3:256）：阶段三精度键 = 位深键 bitpix(-32/-64)
        // —— 既有键、已在 session_keys() 白名单且已被生产消费（lib/phase3_session/p3_session.cpp:126,391；
        // module_adapters.cpp:8890/9693），**不是新造键**。这里只把它列进字段说明（json == nullptr
        // ⇒ 不进模板：它有实现缺省 -32，模板不替用户主张数值），补上「合同 precision 键被拒后
        // 用户找不到精度载体」的缺口。
        {"bitpix", nullptr,
         "可选输出位深 -32(FP32)|-64(FP64)；阶段三计算精度键（ASTROCS_DESIGN §3.3:256），"
         "缺省按现实现 -32"},
        {"projection", nullptr, "可选投影（当前唯一实现 TAN；缺省 TAN）"},
        {"sampler", nullptr, "可选采样核 nearest|bilinear（缺省 bilinear）"},
        {"longitude_parity", nullptr, "可选经度方向 east_left|east_right（缺省 east_left）"},
        {"coverage_output", nullptr, "可选覆盖率输出（当前唯一实现 mask）"},
        // DC-503（§5.3）: CLI 只识别并透传，不判科学值域；消费在 scheduler 面。
        {"output_mode", nullptr,
         "可选输出模式：surface_brightness / point_source_flux / visualization"
         "（缺所选模式所需信息 → 拒绝或明确 unavailable）"},
    };
    switch (s) {
        case SESSION_NORMALIZE: return kNormalize;
        case SESSION_MOSAIC:    return kMosaic;
        default:                return kExport;   // SESSION_EXPORT
    }
}

// 配置模板（§6.1 --template）：由 config_fields 拼装 —— 键名/值/说明单源，
// 与运行期节点必需键同步（E2E-D02：normalize 曾缺 drizzle.precision_mode，
// 用户拿模板补路径后仍被拒）。
// CLI-MULTIBLOCK（§9.68）：scope=="block" 的字段组装为 blocks[] 的两块示例
// （同一 JSON 内并列多块，各带一套母版与运行参数 + 块级 output_dir）。
inline std::string config_template(SessionId s) {
    std::string out = "{\n";
    bool first = true;
    for (const auto& f : config_fields(s)) {
        if (f.json == nullptr || std::string(f.scope) != "top") continue;
        if (!first) out += ",\n";
        first = false;
        out += std::string("  \"") + f.key + "\": " + f.json;
    }
    // 块级字段 → 两块示例：第 2 块按 config_second_block_overrides() 覆盖取值
    // （展示多通道各带平场/参数、共用 bias/dark 母版）。
    auto render_block = [&](int index) {
        std::string blk;
        bool bfirst = true;
        for (const auto& f : config_fields(s)) {
            if (std::string(f.scope) != "block" || f.json == nullptr) continue;
            const char* value = f.json;
            if (index == 1) {
                for (const auto& [k, v] : config_second_block_overrides())
                    if (std::string(f.key) == k) { value = v; break; }
            }
            if (!bfirst) blk += ",\n";
            bfirst = false;
            blk += std::string("      \"") + f.key + "\": " + value;
        }
        return blk;
    };
    bool has_block_fields = false;
    for (const auto& f : config_fields(s))
        if (std::string(f.scope) == "block") has_block_fields = true;
    if (has_block_fields) {
        if (!first) out += ",\n";
        first = false;
        out += "  \"blocks\": [\n    {\n" + render_block(0) + "\n    },\n    {\n" +
               render_block(1) + "\n    }\n  ]";
    }
    out += "\n}\n";
    return out;
}

// --help 字段说明（同一 config_fields 源；json == nullptr 的可选键也列出）。
// 多块形态：块内键以 blocks[].<key> 形式列出（前缀由 scope 派生，不手写副本）。
inline std::string config_field_help(SessionId s) {
    std::string out = "fields:\n";
    bool has_block_fields = false;
    for (const auto& f : config_fields(s))
        if (std::string(f.scope) == "block") has_block_fields = true;
    if (has_block_fields)
        out += "  blocks — 多数据块数组：每块 = 一组 light + 一套母版 + 运行参数 + 块级 "
               "output_dir（一块 = 一次运行 = 一份 run manifest；§9.68）。"
               "只有一块时可用平铺单块简写（input_lights/master_*/output_dir 顶层平铺）；"
               "两种形态互斥\n";
    for (const auto& f : config_fields(s)) {
        const bool blk = std::string(f.scope) == "block";
        out += std::string("  ") + (blk ? "blocks[]." : "") + f.key +
               (f.json == nullptr ? " (optional)" : "") + " — " + f.doc + "\n";
    }
    return out;
}

// 运行确认页（§6.1 / §3.5）：绿色 correct / 橘色 optimize / 红色 error 三级。
// 本层只做**预检提示与确认**；科学判定不在这里（薄入口）。
struct CheckLine { std::string level; std::string text; };

inline bool has_error(const std::vector<CheckLine>& checks) {
    for (const auto& c : checks)
        if (c.level == "error") return true;
    return false;
}

inline std::string render_checks(const std::vector<CheckLine>& checks) {
    std::string out;
    for (const auto& c : checks) {
        const char* mark = (c.level == "error")     ? "[error]"
                           : (c.level == "optimize") ? "[optimize]"
                                                     : "[correct]";
        out += std::string("  ") + mark + " " + c.text + "\n";
    }
    return out;
}

}  // namespace astrocs::cli::cmd
