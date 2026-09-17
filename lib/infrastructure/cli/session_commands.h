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

// ── 会话输入判据的唯一声明（CLI-002 / GAP-034） ──
//
// 预检（subcommand.h precheck_config）与模板（config_template）都从本表取
// 「哪个顶层键 + 什么形态 = 有输入产品」。表内字段必须与运行期会话
// parse_request 实际消费的字段逐字一致，否则预检放行/拒绝会与运行期分叉：
//   * normalize → input_lights（p1_session 消费的路径数组）
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
struct ConfigField {
    const char* key;   // 顶层键（说明行用点号形式可自嵌套）
    const char* json;  // 模板中的 JSON 值文本；nullptr = 不写入模板
    const char* doc;   // --help 字段说明
};

inline const std::vector<ConfigField>& config_fields(SessionId s) {
    // drizzle.precision_mode 必须显式（docs/algorithms/DRIZZLE_GEOMETRY.md B2-A12：
    // 0=FP32/1=FP64，缺失即 DATA 拒绝，不 silent 降精度）；模板取 1（FP64）与
    // RESCUE-FD-02 的库边界缺省一致（宁可慢，不静默丢精度）。
    static const std::vector<ConfigField> kNormalize = {
        {"schema_version", "\"1\"", "配置合同版本（恒 \"1\"）"},
        {"input_lights", "[]", "亮场 FITS 路径数组（必填非空）"},
        {"master_bias", "\"\"", "bias master FITS 路径；缺 → 预检 error（仅 -force 可越）"},
        {"master_dark", "\"\"", "dark master FITS 路径；缺 → 预检 error（仅 -force 可越）"},
        {"master_flat", "\"\"", "flat master FITS 路径；缺 → 预检 error（仅 -force 可越）"},
        {"output_dir", "\".\"", "运行产物唯一落点（必填非空字符串）"},
        {"drizzle",
         "{\"nested\": 1, \"pixfrac\": 1.0, \"precision_mode\": 1}",
         "drizzle 累加参数；nside 缺省即 auto（由最细输入采样派生，"
         "docs/algorithms/DRIZZLE_GEOMETRY.md §3），显式填 nside 或 nside.mode 则按 explicit 校验；"
         "precision_mode 必须显式 0=FP32/1=FP64（无 silent 缺省）"},
        {"filter_passband", "\"\"", "观测滤镜/波段标识（空 = 显式无 filter；写入 HiPS properties）"},
        {"wcs", "{\"gaia_data_dir\": \"\"}",
         "指向来源与解算输入：填 wcs.gaia_data_dir 走真实 IPV 解算；"
         "或改为显式线性 WCS 八参数 crpix1/crpix2/crval1/crval2/cd11/cd12/cd21/cd22"},
        {"wcs.init_source", nullptr,
         "可选指向来源 header_pointing|config|neighbor_crval（缺省 header_pointing）"},
        {"snr", nullptr, "可选 SNR 科学块（gain_e_per_adu/read_noise_e/zero_point_mag 等）"},
    };
    static const std::vector<ConfigField> kMosaic = {
        {"schema_version", "\"1\"", "配置合同版本（恒 \"1\"）"},
        {"hips_paths", "[]", "输入 HiPS 产品目录数组（必填非空；properties 严格校验）"},
        {"output_dir", "\".\"", "运行产物唯一落点（必填非空字符串）"},
        {"weight_mode", "2",
         "1=等权 / 2=ivar（科学方差面）；2 要求输入含 variance/ivar 子产品，缺失即 fail-closed"},
        {"legacy_allow_weight_fallback", nullptr,
         "可选：ivar 缺失时是否允许显式降级为等权（默认不允许）"},
        {"upm", nullptr, "可选 UPM 配置块（拒绝/拟合参数）"},
        {"reject", nullptr, "可选 rejection 配置块；reject_profile 选择档位"},
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
        {"projection", nullptr, "可选投影（当前唯一实现 TAN；缺省 TAN）"},
        {"sampler", nullptr, "可选采样核 nearest|bilinear（缺省 bilinear）"},
        {"longitude_parity", nullptr, "可选经度方向 east_left|east_right（缺省 east_left）"},
        {"coverage_output", nullptr, "可选覆盖率输出（当前唯一实现 mask）"},
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
inline std::string config_template(SessionId s) {
    std::string out = "{\n";
    bool first = true;
    for (const auto& f : config_fields(s)) {
        if (f.json == nullptr) continue;
        if (!first) out += ",\n";
        first = false;
        out += std::string("  \"") + f.key + "\": " + f.json;
    }
    out += "\n}\n";
    return out;
}

// --help 字段说明（同一 config_fields 源；json == nullptr 的可选键也列出）。
inline std::string config_field_help(SessionId s) {
    std::string out = "fields:\n";
    for (const auto& f : config_fields(s)) {
        out += std::string("  ") + f.key + (f.json == nullptr ? " (optional)" : "") +
               " — " + f.doc + "\n";
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
