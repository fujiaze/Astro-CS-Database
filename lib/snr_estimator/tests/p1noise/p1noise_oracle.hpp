// P1-NOISE-TEST · 独立 oracle
//
// 合同锚: docs/algorithms/NOISE_ESTIMATION.md §13.4 TEST-NOISE-DESIGN-001
// (P1-NOISE-DOC 冻结, 2026-09-07, wave W1); 上游 SCI-NOISE-001..015
// (NOISE_MODEL.md FROZEN T104, 冻结容差: σ 5% SNR-004 / 平面场 10% SNR-006 /
// Poisson 诊断交叉 5% SNR-005 / NumPy 参考复算 rtol 1e-9 SCI §11)。
//
// 独立性规则 (模板 <prefix>-TEST §3, 对齐 p1cos oracle 先例): oracle 不
// 调用被测函数、不复制同一实现。与被测实现 (lib/snr_estimator/cpp/src/
// noise_model.cpp) 的推导路径差异:
//   - 中位数: 复制+std::sort 全排序 (被测: std::nth_element 选择路径);
//     偶数取双中位均值 (0.5*(v[k-1]+v[k])), 数值路径不同源。
//   - MAD/σ: sort 路径 absdev + 冻结常数 1.482602218505602 (SCI 公式常数,
//     非实现私有)。
//   - patch 划分: 按合同公式 y0=py·h/gy (整除划分, ALG-NOISE-001 合同锚)
//     独立重写, 不引用被测 TU。
//   - LS 平面: 独立正规方程求解 (被测: 中心化 2×2 行列式; oracle: 双消元
//     直接解 2×2 中心化方程组同公式不同代码路径 — fill 公式本身为 ALG
//     冻结 var(x,y)=a+b·x+c·y 最小二乘平面)。
//   - 掩膜: 逐像素距判圆盘 (被测: 扫描线 dxmax=floor(sqrt) 路径)。
//   - gain/scale law: SCI-NOISE-005/002 冻结解析式独立编码。
//   - 期望值绝不调用被测函数 (验收: "期望值非被测函数生成")。
#ifndef P1NOISE_ORACLE_HPP
#define P1NOISE_ORACLE_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include "snr_estimator.h"

namespace p1noise {

// 冻结容差 (ALG §13.4; 不得放宽, P1-NOISE-TEST 无权改)
inline constexpr double kSigmaRtol = 0.05;        // SNR-004: σ 相对真值 5%
inline constexpr double kPlaneRtol = 0.10;        // SNR-006: 平面场系数 10%
inline constexpr double kPoissonRtol = 0.05;      // SNR-005: Poisson 交叉 5%
inline constexpr double kRefRtol = 1e-9;          // SCI §11: NumPy 等价参考复算
// σ → MAD 冻结常数 (SCI-NOISE-001 σ_bg 公式; 与被测/文档一致的科学常数)
inline constexpr double kMadToSigma = 1.482602218505602;
// variance_floor 冻结默认 (SCI/DATA-P1-NOISE §4a)
inline constexpr double kVarFloor = 1e-12;

// 复制+sort 中位数 (double 域; 偶数取双中位均值) — sort 全排序路径,
// 与被测 nth_element 选择路径不同源
inline double median_oracle(std::vector<double> v) {
    if (v.empty()) return std::numeric_limits<double>::quiet_NaN();
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    if (n % 2 == 1) return v[n / 2];
    return 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

// 稳健 σ oracle: 1.482602218505602 · median(|x − median(x)|), sort 路径
inline double robust_sigma_oracle(std::vector<double> v) {
    if (v.empty()) return 0.0;
    const double med = median_oracle(v);
    std::vector<double> dev(v.size());
    for (std::size_t i = 0; i < v.size(); ++i) dev[i] = std::fabs(v[i] - med);
    return kMadToSigma * median_oracle(dev);
}

// patch 整除划分 (合同公式独立重写): [py·h/gy, (py+1)·h/gy)
inline int patch_x0(int px, int w, int gx) {
    return static_cast<int>(static_cast<std::int64_t>(px) * w / gx);
}
inline int patch_x1(int px, int w, int gx) {
    return static_cast<int>(static_cast<std::int64_t>(px + 1) * w / gx);
}
inline double patch_center_x(int px, int w, int gx) {
    return 0.5 * (patch_x0(px, w, gx) + patch_x1(px, w, gx));
}

// 相对差 (oracle 对照)
inline double rel_diff(double got, double want) {
    const double d = std::fabs(got - want);
    const double s = std::fabs(want);
    if (s == 0.0) return d;
    return d / s;
}

// ---------------------------------------------------------------------------
// blank-sky 模型 oracle: 对同帧按合同 patch 划分独立复算每 patch 稳健 σ /
// variance, 全局兜底 median(patch_var) → σ/var/ivar (与被测输出对照)。
// 前提 (FIX-NOISE-A 设计保证): 帧无饱和/无 cosmic (样本全在 5σ 内),
// 裁剪轮确定性零剔除 → oracle 与被测同样本集; sort 复算 bitwise 对照。
// 注意: 本 oracle 只消费帧 + cfg + 掩膜 (输入侧), 绝不调用被测函数。
// ---------------------------------------------------------------------------
struct BlankSkyOracle {
    std::vector<double> ctrl_sigma;     // 每 patch (行主序 py*gx+px)
    std::vector<double> ctrl_variance;
    std::vector<double> ctrl_ivar;
    std::vector<double> ctrl_x;         // patch 中心 (合同公式)
    std::vector<double> ctrl_y;
    std::vector<char>   qualified;      // 1=合格 (sig>0)
    double sigma_global = 0.0;
    double variance_global = 0.0;
    double ivar_global = 0.0;
    int n_qualified = 0;
};

template <typename T>
inline BlankSkyOracle blank_sky_oracle(const std::vector<T>& data, int w, int h,
                                       const std::vector<float>& mask,
                                       const SnrNoiseModelConfig& cfg) {
    const int gx = cfg.patch_grid_x < 2 ? 2 : cfg.patch_grid_x;
    const int gy = cfg.patch_grid_y < 2 ? 2 : cfg.patch_grid_y;
    const double floor_v = cfg.variance_floor > 0.0 ? cfg.variance_floor : 0.0;
    BlankSkyOracle o;
    o.ctrl_sigma.assign(static_cast<std::size_t>(gx) * gy, 0.0);
    o.ctrl_variance.assign(static_cast<std::size_t>(gx) * gy, 0.0);
    o.ctrl_ivar.assign(static_cast<std::size_t>(gx) * gy, 0.0);
    o.ctrl_x.assign(static_cast<std::size_t>(gx) * gy, 0.0);
    o.ctrl_y.assign(static_cast<std::size_t>(gx) * gy, 0.0);
    o.qualified.assign(static_cast<std::size_t>(gx) * gy, 0);
    std::vector<double> vars;
    for (int py = 0; py < gy; ++py) {
        const int y0 = patch_x0(py, h, gy);
        const int y1 = patch_x1(py, h, gy);
        for (int px = 0; px < gx; ++px) {
            const int x0 = patch_x0(px, w, gx);
            const int x1 = patch_x1(px, w, gx);
            std::vector<double> s;
            s.reserve(static_cast<std::size_t>(x1 - x0) * (y1 - y0));
            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x) {
                    const std::size_t idx = static_cast<std::size_t>(y) * w + x;
                    if (!mask.empty() && mask[idx] != 0.0f) continue;
                    const double v = static_cast<double>(data[idx]);
                    if (!std::isfinite(v)) continue;
                    if (cfg.saturation_level > 0.0 && v >= cfg.saturation_level) continue;
                    s.push_back(v);
                }
            }
            const std::size_t slot = static_cast<std::size_t>(py) * gx + px;
            o.ctrl_x[slot] = 0.5 * (x0 + x1);
            o.ctrl_y[slot] = 0.5 * (y0 + y1);
            if (static_cast<int>(s.size()) < std::max(1, cfg.min_patch_samples)) continue;
            const double sig = robust_sigma_oracle(s);
            if (!std::isfinite(sig) || sig <= 0.0) continue;
            o.ctrl_sigma[slot] = sig;
            o.ctrl_variance[slot] = std::max(sig * sig, floor_v);
            o.ctrl_ivar[slot] = 1.0 / o.ctrl_variance[slot];
            o.qualified[slot] = 1;
            o.n_qualified += 1;
            vars.push_back(o.ctrl_variance[slot]);
        }
    }
    if (!vars.empty()) {
        o.variance_global = std::max(median_oracle(vars), floor_v);
        o.sigma_global = std::sqrt(o.variance_global);
        o.ivar_global = 1.0 / o.variance_global;
    }
    return o;
}

// 全帧兜底 oracle (无合格 patch 路径): 全帧有效样本稳健 σ
template <typename T>
inline bool global_fallback_oracle(const std::vector<T>& data, int w, int h,
                                   const std::vector<float>& mask,
                                   const SnrNoiseModelConfig& cfg,
                                   double* sigma_out) {
    std::vector<double> all;
    all.reserve(static_cast<std::size_t>(w) * h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t idx = static_cast<std::size_t>(y) * w + x;
            if (!mask.empty() && mask[idx] != 0.0f) continue;
            const double v = static_cast<double>(data[idx]);
            if (!std::isfinite(v)) continue;
            if (cfg.saturation_level > 0.0 && v >= cfg.saturation_level) continue;
            all.push_back(v);
        }
    }
    if (static_cast<int>(all.size()) < std::max(1, cfg.min_patch_samples / 2)) return false;
    const double sig = robust_sigma_oracle(all);
    if (!std::isfinite(sig) || sig <= 0.0) return false;
    *sigma_out = sig;
    return true;
}

// ---------------------------------------------------------------------------
// LS 平面 oracle (fill 语义对照, FIX-NOISE-G): 输入 = 被测 build 输出的
// ctrl_* 数组 (合法 fill 输入面, 非被测函数内部状态); 复算路径 = 合同公式
// var(x,y)=a+b·x+c·y 最小二乘平面的独立实现 (均值+协方差 2×2 方程组,
// 逐项独立累加顺序)。
// ---------------------------------------------------------------------------
struct PlaneOracle {
    double a = 0.0, b = 0.0, c = 0.0;
    bool solvable = false;
};

inline PlaneOracle plane_ls_oracle(const double* cx, const double* cy,
                                   const double* cv, std::uint32_t n) {
    PlaneOracle p;
    if (n < 4) return p;
    double mx = 0, my = 0, mv = 0;
    for (std::uint32_t i = 0; i < n; ++i) {
        mx += cx[i];
        my += cy[i];
        mv += cv[i];
    }
    mx /= static_cast<double>(n);
    my /= static_cast<double>(n);
    mv /= static_cast<double>(n);
    double sxx = 0, sxy = 0, syy = 0, sxv = 0, syv = 0;
    for (std::uint32_t i = 0; i < n; ++i) {
        const double X = cx[i] - mx;
        const double Y = cy[i] - my;
        const double V = cv[i] - mv;
        sxx += X * X;
        sxy += X * Y;
        syy += Y * Y;
        sxv += X * V;
        syv += Y * V;
    }
    const double det = sxx * syy - sxy * sxy;
    if (std::fabs(det) > 1e-24) {
        p.b = (sxv * syy - syv * sxy) / det;
        p.c = (syv * sxx - sxv * sxy) / det;
        p.a = mv - p.b * mx - p.c * my;
        p.solvable = true;
    }
    return p;
}

// fill 预测值 oracle (合同: var=max(a+b·x+c·y, floor); floor 由模型注册
// 表或回退 1e-12 — 手工/未注册模型回退 kVarFloor, DISP-NOISE-002 现状)
inline double fill_variance_oracle(const PlaneOracle& p, double x, double y,
                                   double floor_v) {
    const double pred = p.a + p.b * x + p.c * y;
    return std::max(pred, floor_v);
}

// ---------------------------------------------------------------------------
// gain 诊断 oracle (SCI-NOISE-005 冻结解析式独立编码):
// var_ADU = max(signal,0)/gain + (rn/gain)²; gain≤0 → 0 (显式无效)
// ---------------------------------------------------------------------------
inline double gain_variance_oracle(double signal, double gain, double rn) {
    if (gain <= 0.0) return 0.0;
    const double s = signal > 0.0 ? signal : 0.0;
    return s / gain + (rn * rn) / (gain * gain);
}

// ---------------------------------------------------------------------------
// scale law oracle (SCI-NOISE-002 冻结: x'=αx → var'=α²var、ivar'=ivar/α²;
// 与被测同语序展开 v·α·α — 表达式顺序由冻结公式线性展开唯一给出)
// ---------------------------------------------------------------------------
inline void scale_law_oracle(double alpha, double* variance, double* ivar) {
    if (variance) *variance = (*variance) * alpha * alpha;
    if (ivar) {
        const double a2 = alpha * alpha;
        if (a2 > 0.0 && std::isfinite(*ivar)) *ivar = (*ivar) / a2;
    }
}

// bitwise 相等 (double, NaN 不等价 — NaN 断言用专用 helper)
inline bool bit_eq(double a, double b) {
    if (std::isnan(a) && std::isnan(b)) return true;  // NaN==NaN 语义对齐
    if (std::isnan(a) || std::isnan(b)) return false;
    return std::memcmp(&a, &b, sizeof(double)) == 0;
}

inline bool is_nan_bits(double v) {
    return std::isnan(v) != 0;
}

}  // namespace p1noise

#endif  // P1NOISE_ORACLE_HPP
