#ifndef ASTROCS_CORE_MASTER_UNIT_GUARD_H
#define ASTROCS_CORE_MASTER_UNIT_GUARD_H

// UNIT-001 母版单位/归一化消费门（纯规则，无 I/O、无 JSON、无像素算术）。
//
// 权威：SCI-CAL-001（docs/science/CALIBRATION.md）§3/§6/§8/§11 —— 标度/域声明、
//       flat 归一化判定带、dark bias 约定显式声明；ALG-CAL-001
//       （docs/algorithms/CALIBRATION_ALGORITHMS.md）§2 标度声明表 + §10 DISP-CAL-013；
//       DATA-P1-CAL（docs/contracts/DATA_SEMANTICS.md）§9.1/§9.1a。
// 外部依据：XISF 1.0 §Image（bounds = 可表示域；浮点实型必须声明，无默认域）、
//       PCL XISFReader NormalizeSamples（Float32[0,1] ↔ UInt16[0,65535]）、
//       FITS 3.0 §4.2.1（BSCALE/BZERO）。
//
// 语义边界：本头只做「声明是否自洽 / 观测统计是否与声明域相容」的判定；换算与归一
// 由调用方按声明施加（声明的换算因子必须写进 manifest 供审计）。本头不改变任何
// 科学公式：通过本门后，缓冲仍必须满足 DATA-P1-CAL §9.1 的 ADU / 已归一约定。

#include <cmath>
#include <cstdio>
#include <string>

namespace astrocs {
namespace core {
namespace master_units {

// 单位 token（唯一词表；SCI-CAL-001 §3）
enum class Unit { ADU = 0, Normalized = 1 };

inline const char *unit_token(Unit u) { return u == Unit::ADU ? "ADU" : "normalized"; }

// 判定带默认值（冻结）：与 config/defaults.json 的
// calibration.master_flat_median_range = [0.5, 2.0] 三处同值（SCI:101-102 / ALG:56）。
// 语义：仅区分「已归一（合同值 1.0）」与「未归一」两种数量级状态，不是科学容差。
inline constexpr double kFlatMedianBandLo = 0.5;
inline constexpr double kFlatMedianBandHi = 2.0;
// ADU 域 / 归一化域的分界（观测中位数）：归一化 [0,1] 浮点母版的中位数 ≤ 1，
// 而真实 16 位 ADU 亮场的中位数量级为 10^3（T2 1481 / T4 1457 ADU，UNIT-001 实测）。
inline constexpr double kNormalizedDomainCeiling = 1.0;
inline constexpr double kAduDomainFloor = 1.0;

// 单个帧类的声明（调用方从 config 解析后的纯值）。
struct ClassDecl {
    bool has_unit = false;    // master_units.<类> 是否显式给出
    Unit unit = Unit::ADU;    // 声明域（缺省 ADU：FITS 16 位链路的合同域）
    bool has_scale = false;   // master_scale.<类> 是否显式给出
    double scale = 1.0;       // 到 ADU 的线性换算因子（须 > 0 且有限）
};

struct Decl {
    ClassDecl light, bias, dark, flat;
    bool dark_convention_given = false;  // dark_optimization 显式给出（U3）
    bool dark_includes_bias = false;     // true = 含 bias（兼容形态）
    bool flat_normalize_given = false;   // master_flat_normalize 显式给出
    bool flat_normalize_median = false;  // "median" = 授权按 flat/median(flat) 归一
    bool flat_band_given = false;        // master_flat_median_range 显式给出
    double flat_band_lo = kFlatMedianBandLo;
    double flat_band_hi = kFlatMedianBandHi;
};

// 观测统计（非有限像素单独标记；判定只用中位数）。
struct Stats {
    double median = 0.0;
    double min_v = 0.0;
    double max_v = 0.0;
    bool all_finite = true;
    bool has_finite = true;
};

struct Verdict {
    bool ok = true;
    const char *rule = "";   // "U1".."U4"
    const char *token = "";  // 结构化诊断 token（门按 token 判红/绿）
    std::string message;
};

inline std::string fmt_double(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6g", v);
    return std::string(buf);
}

inline Verdict ok_verdict() { return Verdict{}; }

inline Verdict reject(const char *rule, const char *token, const std::string &msg) {
    Verdict v;
    v.ok = false;
    v.rule = rule;
    v.token = token;
    v.message = std::string(token) + ": " + msg;
    return v;
}

// ── U4 声明自洽（先于像素校验；声明本身错 ⇒ 拒绝，不得猜）────────────────────
inline Verdict check_declarations(const Decl &d) {
    const ClassDecl *classes[4] = {&d.light, &d.bias, &d.dark, &d.flat};
    const char *names[4] = {"light", "bias", "dark", "flat"};
    for (int i = 0; i < 4; ++i) {
        const ClassDecl &c = *classes[i];
        if (!c.has_scale) continue;
        if (!(c.scale > 0.0) || !std::isfinite(c.scale))
            return reject("U4", "MASTER_UNIT_DECLARATION_INVALID",
                          std::string("master_scale.") + names[i] +
                              " must be finite and > 0 (got " + fmt_double(c.scale) + ")");
    }
    // 声明 normalized ⇒ 必须给出正换算因子（文件不携带该因子：XISF bounds 只是可表示域）
    for (int i = 0; i < 4; ++i) {
        const ClassDecl &c = *classes[i];
        if (c.has_unit && c.unit == Unit::Normalized && !c.has_scale)
            return reject("U4", "MASTER_UNIT_DECLARATION_INVALID",
                          std::string("master_units.") + names[i] +
                              "=\"normalized\" requires an explicit master_scale." + names[i] +
                              " (the ADU factor is not stored in the file; XISF bounds is a"
                              " representable range, not a physical unit)");
    }
    // bias/dark 同域：换算因子必须一致（否则相减无意义）
    if (d.bias.has_scale && d.dark.has_scale) {
        const double a = d.bias.scale, b = d.dark.scale;
        if (std::fabs(a - b) > 1e-9 * (a > b ? a : b))
            return reject("U4", "MASTER_UNIT_DECLARATION_INVALID",
                          "master_scale.bias (" + fmt_double(a) + ") != master_scale.dark (" +
                              fmt_double(b) + "): bias and dark must share one domain");
    }
    return ok_verdict();
}

// ── U3 dark 的 bias 约定必须显式声明 ────────────────────────────────────────
inline Verdict check_dark_convention(const std::string &dark_path, const Decl &d) {
    if (d.dark_convention_given) return ok_verdict();
    return reject("U3", "MASTER_DARK_CONVENTION_UNDECLARED",
                  "master_dark=" + dark_path +
                      " requires an explicit dark_optimization (true = master dark includes"
                      " bias / false = bias-subtracted). The two conventions are different"
                      " scientific inputs (SCI-CAL-001 §5) and must not default silently");
}

// ── U1 bias/dark 与亮场必须同域（观测证据 + 声明）───────────────────────────
inline Verdict check_master_domain(const char *klass, const std::string &path, const ClassDecl &c,
                                   const Stats &master, const Stats &light) {
    if (!master.has_finite)
        return reject("U1", "MASTER_UNIT_MISMATCH",
                      std::string("master_") + klass + "=" + path + " has no finite pixels");
    if (c.has_unit && c.unit == Unit::Normalized) {
        // 已显式声明归一化域 + 换算因子（U4 已保证因子存在）：由调用方换算到 ADU 后消费
        return ok_verdict();
    }
    if (master.median > kNormalizedDomainCeiling) return ok_verdict();  // ADU 域
    if (light.has_finite && light.median > kAduDomainFloor)
        return reject("U1", "MASTER_UNIT_MISMATCH",
                      std::string("master_") + klass + "=" + path + " observed median=" +
                          fmt_double(master.median) +
                          " is in the normalized [0,1] domain while the light frame median=" +
                          fmt_double(light.median) +
                          " ADU: master and light must share one scale. Declare"
                          " master_units." + klass + "=\"normalized\" + master_scale." + klass +
                          " (e.g. 65535 for 16-bit native data) or supply masters already in ADU");
    return ok_verdict();
}

// ── U2 master flat 必须已归一（判定带）或显式声明按 median 归一 ─────────────
inline Verdict check_flat_normalized(const std::string &path, const Decl &d, const Stats &flat) {
    if (!flat.has_finite)
        return reject("U2", "MASTER_FLAT_NOT_NORMALIZED",
                      "master_flat=" + path + " has no finite pixels");
    if (d.flat_normalize_median) return ok_verdict();  // 调用方按 flat/median(flat) 归一
    const double lo = d.flat_band_lo, hi = d.flat_band_hi;
    if (flat.median >= lo && flat.median <= hi) return ok_verdict();
    return reject("U2", "MASTER_FLAT_NOT_NORMALIZED",
                  "master_flat=" + path + " observed median=" + fmt_double(flat.median) +
                      " outside master_flat_median_range=[" + fmt_double(lo) + "," + fmt_double(hi) +
                      "]: the master flat must be normalized to median=1.0 (SCI-CAL-001 §5/§6)."
                      " Normalize it, or declare master_flat_normalize=\"median\" to apply"
                      " flat/median(flat) explicitly");
}

}  // namespace master_units
}  // namespace core
}  // namespace astrocs

#endif  // ASTROCS_CORE_MASTER_UNIT_GUARD_H
