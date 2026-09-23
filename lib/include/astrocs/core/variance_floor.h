// ============================================================================
// variance_floor.h — 「按 dtype 导出的方差地板」唯一实现（纯函数，无状态）
//
// 权威与依据
//   * docs/science/NOISE_MODEL.md §7「Floor 夹逼不变量」：任意**可用** variance 经
//     floor 夹逼后 ivar 有限；clamp **只作用于可用方差**；**不可用一律 ivar=0
//     （不得由 clamp 产生）**；每帧生效 floor 及其来源必须随帧产品登记。
//   * docs/science/NOISE_MODEL.md §9：variance_floor 是**纯数值保护**（「保证 ivar
//     有限」），**不是**最小可分辨方差、**不是**读出噪声下限。
//   * docs/contracts/DATA_SEMANTICS.md §13.2（fill 输出 out_variance/out_ivar 为
//     **float32** 产品）与 §12.4（HiPS variance/ivar 存储 dtype 双轨 f32/f64）。
//
// 为什么不能是绝对常数
//   配置默认 1e-12 的单位是 ADU²（NOISE_MODEL §7），而帧像素可能已被测光归一化
//   到数组标度 x' = α·x（α = frame_photscal，M42 实测 2.3846837130250378e-17）。
//   按 SNR-002 尺度律 floor 必须换算成 α²·1e-12 = 5.6867e-46；该值在 float32 下
//   **精确下溢为 0**（f32 最小正次正规数 1.401298e-45）⇒ 换算后的地板既不能保证
//   ivar 有限，也会把「可用方差」静默降级为「不可用」。同一个绝对常数因此**无法
//   同时服务 ADU 与 α² 两个标度**（跨 1e-17）。
//
// 本实现的口径（正向约束）
//   生效 floor = max(eps_rel · median(该帧可用方差), dtype 可表示下界)
//   * 「该帧可用方差」= 有限且 > 0 的样本；中位数取稳健统计，非有限/非正一律排除。
//   * 「dtype 可表示下界」= 该 dtype 的最小**正**可表示值（f32 denorm_min /
//     f64 denorm_min）。它是「地板本身必须可表示」这一硬要求的下界。
//   * floor 只作用于**可用**方差（v 有限且 > 0）；不可用（v<=0）与非有限**原样透传**，
//     绝不伪装（§7「不可用一律 ivar=0，不得由 clamp 产生」）。
//   * 转产品 dtype 时：**正值但因 dtype 精度下溢为 0** 的样本用 floor 顶住（保住
//     「可用」这一状态，不把可表示性问题伪装成「无方差信息」）；**精确 0 / 负 /
//     非有限原样保留**（那是「不可用」与「损坏」，不得由本函数改写）。
// ============================================================================

#ifndef ASTROCS_CORE_VARIANCE_FLOOR_H
#define ASTROCS_CORE_VARIANCE_FLOOR_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace astrocs {

// dtype 的最小**正**可表示值（denorm_min）。f32 = 1.401298464324817e-45。
inline double variance_dtype_min_positive(bool product_is_f64) {
    return product_is_f64
        ? (double)std::numeric_limits<double>::denorm_min()
        : (double)std::numeric_limits<float>::denorm_min();
}

inline const char* variance_dtype_name(bool product_is_f64) {
    return product_is_f64 ? "float64" : "float32";
}

// 稳健中位数（非有限与非正值一律排除；空集 ⇒ 0）。
inline double variance_median_usable(const std::vector<double>& v) {
    std::vector<double> s;
    s.reserve(v.size());
    for (double x : v) {
        if (std::isfinite(x) && x > 0.0) s.push_back(x);
    }
    if (s.empty()) return 0.0;
    const std::size_t n = s.size();
    std::nth_element(s.begin(), s.begin() + (std::ptrdiff_t)(n / 2), s.end());
    const double hi = s[n / 2];
    if (n % 2 == 1) return hi;
    std::nth_element(s.begin(), s.begin() + (std::ptrdiff_t)(n / 2 - 1), s.end());
    const double lo = s[n / 2 - 1];
    return 0.5 * (lo + hi);
}

// 来源标签（随帧产品登记用，NOISE_MODEL §7）。
enum class VarianceFloorSource : std::uint8_t {
    kRelativeFrameScale = 0,   // eps_rel·median 项占优
    kDtypeRepresentableMin = 1,// dtype 下界项占优（相对项不可用或更小）
    kNoUsableVariance = 2,     // 该帧无可用方差 ⇒ 地板 = dtype 下界，仅供可表示性
};

inline const char* variance_floor_source_name(VarianceFloorSource s) {
    switch (s) {
        case VarianceFloorSource::kRelativeFrameScale: return "relative_frame_scale";
        case VarianceFloorSource::kDtypeRepresentableMin: return "dtype_representable_min";
        case VarianceFloorSource::kNoUsableVariance: return "no_usable_variance";
    }
    return "unknown";
}

struct VarianceFloor {
    double value = 0.0;                 // 生效地板
    double frame_median_variance = 0.0; // 该帧可用方差的稳健中位数
    double dtype_min_positive = 0.0;    // 产品 dtype 的最小正可表示值
    double eps_rel = 0.0;               // 相对系数
    bool product_is_f64 = false;
    VarianceFloorSource source = VarianceFloorSource::kNoUsableVariance;
};

// 生效 floor = max(eps_rel · median, dtype_min_positive)。
// eps_rel 非有限或 < 0 ⇒ 视为 0（只用 dtype 下界）。
inline VarianceFloor derive_variance_floor(double frame_median_variance,
                                           double eps_rel,
                                           bool product_is_f64) {
    VarianceFloor f;
    f.product_is_f64 = product_is_f64;
    f.dtype_min_positive = variance_dtype_min_positive(product_is_f64);
    f.eps_rel = (std::isfinite(eps_rel) && eps_rel > 0.0) ? eps_rel : 0.0;
    f.frame_median_variance =
        (std::isfinite(frame_median_variance) && frame_median_variance > 0.0)
            ? frame_median_variance : 0.0;
    const double rel = f.eps_rel * f.frame_median_variance;
    f.value = std::max(rel, f.dtype_min_positive);
    if (f.frame_median_variance <= 0.0) {
        f.source = VarianceFloorSource::kNoUsableVariance;
    } else if (rel >= f.dtype_min_positive) {
        f.source = VarianceFloorSource::kRelativeFrameScale;
    } else {
        f.source = VarianceFloorSource::kDtypeRepresentableMin;
    }
    return f;
}

// 只对**可用**方差（有限且 > 0）夹逼；不可用（<=0）与非有限原样透传。
inline double apply_variance_floor(double v, const VarianceFloor& f) {
    if (!std::isfinite(v) || v <= 0.0) return v;   // 不可用/损坏: 不伪装
    return std::max(v, f.value);
}

// 转产品 dtype：保住「可用」这一状态。
//   * v 有限且 > 0，但 (dtype)v == 0（精确下溢）⇒ 返回 floor（可表示且 > 0）
//   * 其余（v<=0、非有限、下溢后仍 > 0）⇒ 原样按 dtype 转换
// 返回 bool* rescued 可选地报告是否发生下溢救援。
inline float to_product_dtype_keeping_availability(double v,
                                                   const VarianceFloor& f,
                                                   bool* rescued = nullptr) {
    if (rescued) *rescued = false;
    const float c = (float)v;
    if (std::isfinite(v) && v > 0.0 && !(c > 0.0f)) {
        if (rescued) *rescued = true;
        return (float)f.value;
    }
    return c;
}

// 一帧的完整推导 + 登记载荷（键名与 §7 登记要求对齐）。
inline std::string variance_floor_provenance_json(const VarianceFloor& f) {
    std::string s = "{\"variance_floor\":";
    s += std::to_string(f.value);
    s += ",\"variance_floor_source\":\"";
    s += variance_floor_source_name(f.source);
    s += "\",\"variance_floor_dtype\":\"";
    s += variance_dtype_name(f.product_is_f64);
    s += "\",\"variance_floor_eps_rel\":";
    s += std::to_string(f.eps_rel);
    s += ",\"variance_floor_frame_median\":";
    s += std::to_string(f.frame_median_variance);
    s += "}";
    return s;
}

}  // namespace astrocs

#endif  // ASTROCS_CORE_VARIANCE_FLOOR_H
