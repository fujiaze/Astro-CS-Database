// saturation_policy.h — 饱和电平的解析策略（SAT-001，claim SC-008）
//
// 权威：docs/science/NOISE_MODEL.md §4「饱和域」（claim SC-008）与 §8 首行；
//       docs/algorithms/NOISE_ESTIMATION.md §13.2（饱和电平语义）；
//       docs/contracts/DATA_SEMANTICS.md §13.1（data/cfg 行）。
//
// 规则（唯一事实源=上述 SCI 条款，本头文件只是它的可测实现）：
//   1. 「饱和像素不参与 blank-sky 统计」是**无条件生效**的输入有效域规则；
//   2. 饱和电平来源优先级 = 显式 cfg(saturation_level>0) > 帧元数据 SATURATE
//      > 帧元数据 DATAMAX；首个 >0 且有限者胜；
//   3. 0 / 负 / 非有限 / 非数值 = **未提供电平（unset）**，**不等于「无饱和」**；
//      此时调用方**必须**产出显式降级声明（DISABLED_NO_METADATA），不得静默通过。
//
// 本头文件为纯函数、无状态、无 I/O；不改变任何 C ABI（snr_estimator.h 未动）。
#ifndef ASTROCS_NOISE_SATURATION_POLICY_H
#define ASTROCS_NOISE_SATURATION_POLICY_H

#include <cmath>
#include <cstdlib>

namespace astrocs {
namespace noise {

// 解析单个 FITS 关键字字符串为饱和电平；0 = 无效/未提供。
// 非法输入（空串、非数值前缀、NaN/Inf、<=0）一律返回 0，不抛异常。
inline double parse_saturation_keyword(const char* keyword) {
    if (!keyword || !keyword[0]) return 0.0;
    char* end = nullptr;
    const double v = std::strtod(keyword, &end);
    if (end == keyword) return 0.0;              // 无数字前缀
    if (!std::isfinite(v)) return 0.0;           // NaN / Inf
    if (!(v > 0.0)) return 0.0;                  // <= 0 = 未提供
    return v;
}

// 未提供电平 ⇒ 0。返回 0 表示「unset」，调用方必须显式声明降级。
inline double resolve_saturation_level(const char* saturate_kw, const char* datamax_kw) {
    const double a = parse_saturation_keyword(saturate_kw);
    if (a > 0.0) return a;
    return parse_saturation_keyword(datamax_kw);
}

// 显式 cfg 优先于帧元数据（优先级 2）：cfg 有效即用 cfg，否则回落到元数据。
inline double resolve_effective_saturation(double cfg_level,
                                           const char* saturate_kw,
                                           const char* datamax_kw) {
    if (std::isfinite(cfg_level) && cfg_level > 0.0) return cfg_level;
    return resolve_saturation_level(saturate_kw, datamax_kw);
}

// 电平来源标签（写 photo_stats 供消费方 fail-closed 判定）。
inline const char* saturation_level_source(double cfg_level,
                                           const char* saturate_kw,
                                           const char* datamax_kw) {
    if (std::isfinite(cfg_level) && cfg_level > 0.0) return "CFG_EXPLICIT";
    if (parse_saturation_keyword(saturate_kw) > 0.0) return "HEADER_SATURATE";
    if (parse_saturation_keyword(datamax_kw) > 0.0) return "HEADER_DATAMAX";
    return "NONE";
}

// 过滤状态：ENABLED = 电平已提供且过滤生效；DISABLED_NO_METADATA = 未提供电平
// （显式降级声明，禁止把它读成「该帧无饱和像素」）。
inline const char* saturation_filter_state(double level) {
    return (std::isfinite(level) && level > 0.0) ? "ENABLED" : "DISABLED_NO_METADATA";
}

}  // namespace noise
}  // namespace astrocs

#endif  // ASTROCS_NOISE_SATURATION_POLICY_H
