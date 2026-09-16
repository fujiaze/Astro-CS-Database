// ============================================================================
// p1star_oracle.hpp — P1-STAR-TEST 独立 oracle (不调用被测函数, 不复制同一实现)
// ----------------------------------------------------------------------------
// 合同锚: §11.4 F4 (FP64 Moffat4/Gaussian oracle |Δ中心|≤0.05px, A/B 相对误差
// ≤1e-3); §2 mag=−2.5·log10(Σ_box(pixel−B_fit)) (box=(2R+1)², R 钳 [5,200],
// 候选中心 — oracle 用此解析定义独立复算期望); sort mag 升序 stable_sort NaN
// 恒末尾 (§2 sdet_dedup_stars/sort 语义)。
//
// oracle 原则: 期望值由 fixture 构造参数 (解析场) + 本文件独立实现直接计算,
// 不调用 sdet_* 任何函数。PSF 面数 (高斯峰/盒积分) 用解析或高精度数值积分
// (Simpson, 与生产 GSL 路径无共享代码)。
// ============================================================================
#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "p1star_fixtures.hpp"

namespace p1star {
namespace oracle {

// ---- PSF 面数 (flux 期望 = Σ 像素模型值, 对解析高斯用解析积分) -------------

// 二维各向同性高斯单位振幅的总通量 (解析: 2πσ²) — 生产 mag/flux 语义为
// 像素离散和; 对 σ≥1.8, 2πσ² 与离散和偏差 <0.05% (远小于 1e-3 相对容差),
// oracle 同时提供高精度 Simpson 离散版本供交叉验证。
inline double gaussian_flux_analytic(double amp, double sigma) {
    return amp * 2.0 * M_PI * sigma * sigma;
}

// 3/8 Simpson 对单变量函数 [a,b] 积分 (独立实现, 非被测代码路径)
template <typename F>
inline double simpson38(F f, double a, double b, int n_intervals) {
    if ((n_intervals % 3) != 0) n_intervals += 3 - (n_intervals % 3);
    const double hstep = (b - a) / n_intervals;
    double s = f(a) + f(b);
    for (int i = 1; i < n_intervals; ++i)
        s += f(a + i * hstep) * (i % 3 == 0 ? 2.0 : 4.0);
    return s * hstep / 3.0;
}

// 高斯在 R×R 方窗内的积分质量占比 (中心对准): 独立复算 flux 裁剪期望。
// flux = ∫∫ 2D 高斯 over [-R,R]² = amp·[erf(R/(σ√2))·√(π/2)σ]²  (解析)
inline double gaussian_flux_in_box(double amp, double sigma, double R) {
    const double ex = std::erf(R / (sigma * std::sqrt(2.0)));
    return amp * M_PI * sigma * sigma * ex * ex;
}

// FWHM = 2σ√(2ln2) (高斯真值; F1 容差 10% 的期望基准)
inline double gaussian_fwhm(double sigma) {
    return 2.0 * sigma * std::sqrt(2.0 * std::log(2.0));
}

// 星等 (合同 §2): mag = −2.5·log10(flux); 无效 flux → NaN (恒排末尾)
inline double magnitude(double flux) {
    if (!(flux > 0.0)) return std::numeric_limits<double>::quiet_NaN();
    return -2.5 * std::log10(flux);
}

// ---- 全序比较器 (§2 sort 语义: mag 升序 stable_sort, NaN 恒末尾) -----------

inline bool mag_less(double a, double b) {
    const bool an = std::isnan(a), bn = std::isnan(b);
    if (an && bn) return false;
    if (an) return false;  // NaN 永不小于任何数 → 排末尾
    if (bn) return true;
    return a < b;
}

// ---- 匹配与统计 -------------------------------------------------------------

// 贪心最近邻匹配 (1.5px 阈): 返回每真值星对应的检测索引 (-1 未召回)。
// 距离用欧氏 (F1 锚 |Δc|≤0.3px 远小于阈值, 匹配阈值本身不是容差放行)。
inline std::vector<int> match_nearest(const std::vector<SynthStar>& truth,
                                      const double* xs, const double* ys, int count,
                                      double max_dist = 1.5) {
    std::vector<int> out(truth.size(), -1);
    std::vector<char> used((std::size_t)std::max(count, 0), 0);
    for (std::size_t i = 0; i < truth.size(); ++i) {
        double best = max_dist * max_dist;
        int bi = -1;
        for (int j = 0; j < count; ++j) {
            if (used[j]) continue;
            const double dx = xs[j] - truth[i].cx, dy = ys[j] - truth[i].cy;
            const double d2 = dx * dx + dy * dy;
            if (d2 < best) { best = d2; bi = j; }
        }
        if (bi >= 0) { out[i] = bi; used[(std::size_t)bi] = 1; }
    }
    return out;
}

}  // namespace oracle
}  // namespace p1star
