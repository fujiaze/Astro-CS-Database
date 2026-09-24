// P1-NOISE-TEST · 独立 oracle
//
// 合同锚: docs/algorithms/NOISE_ESTIMATION.md §13.4 TEST-NOISE-DESIGN-001
// (P1-NOISE-DOC 冻结, 2026-09-07, wave W1); 上游 SCI-NOISE-001..015
// (NOISE_MODEL.md FROZEN T104, 冻结容差: σ 5% SNR-004 / 平面场 10% SNR-006 /
// Poisson 诊断交叉 5% SNR-005 / NumPy 参考复算 rtol 1e-9 SCI §11)。
//
// 独立性规则 (模板 <prefix>-TEST §3, 对齐 p1cos oracle 先例): oracle 不
// 调用被测函数、不复制同一实现。与被测实现 (lib/algorithms/noise_snr/cpp/src/
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


// ---------------------------------------------------------------------------
// SCI-VAR-ADAPT-01 (SCI-NOISE-001 §5d ②): 稳健非负平面拟合的**独立** oracle
//   定义（合同）: min_β Σ w_i (a + b·x_i + c·y_i − v_i)²,  w_i = (v_med/v_i)²,
//                 s.t.  a + b·x_i + c·y_i ≥ 0 ∀i（⇔ 控制点凸包内恒 ≥ 0）。
//   与生产的**路径差异**（独立性, 对齐本文件头部的规则）:
//     * 约束处理: 生产用「4 极值点起步的割平面 + 活跃子集枚举」; 本 oracle 直接对
//       **全部 n 个约束**枚举 |S| ≤ 3 的活跃子集（3 未知量 ⇒ 最优活跃集可取 ≤3 个
//       线性无关约束）;
//     * 法方程: 生产做坐标/权重归一化 + 伴随矩阵求逆; 本 oracle 不做归一化, 用
//       高斯-约当求逆 + 朴素高斯消元解 k×k 系统。
//   两者数学等价 ⇒ 系数一致到浮点舍入（调用方按 rtol 断言, **不**要求逐位; 逐位
//   断言用于 fill 的逐像素判据, 期望值取 build 的 provenance 系数）。
// ---------------------------------------------------------------------------
inline PlaneOracle plane_fit_oracle(const double* cx, const double* cy,
                                    const double* cv, std::uint32_t n) {
    PlaneOracle p;
    if (!cx || !cy || !cv || n < 4) return p;
    std::vector<double> sv(cv, cv + n);
    std::sort(sv.begin(), sv.end());
    const double vmed = (n % 2 == 1) ? sv[n / 2] : 0.5 * (sv[n / 2 - 1] + sv[n / 2]);
    if (!(vmed > 0.0) || !std::isfinite(vmed)) return p;
    std::vector<double> w(n), sw(n), rhs(n);
    for (std::uint32_t i = 0; i < n; ++i) {
        const double ratio = vmed / cv[i];
        w[i] = ratio * ratio;
        if (!std::isfinite(w[i])) return p;
        sw[i] = std::sqrt(w[i]);
        rhs[i] = sw[i] * cv[i];
    }
    // 设计行 r_i = [1, x_i, y_i]（**不做坐标归一化** —— 与生产的中心化+尺度归一化不同源）
    const auto row_dot = [&](const double* z, std::uint32_t i) {
        return z[0] + z[1] * cx[i] + z[2] * cy[i];
    };
    const auto objective = [&](const double* beta) {
        double f = 0.0;
        for (std::uint32_t i = 0; i < n; ++i) {
            const double r = row_dot(beta, i) - cv[i];
            f += w[i] * r * r;
        }
        return f;
    };
    // 可行域判据含浮点比较余量（与生产同一数值口径, 见 noise_model.cpp 的 kFitEpsScale）。
    const auto feasible = [&](const double* beta) {
        for (std::uint32_t i = 0; i < n; ++i) {
            const double t0 = beta[0], t1 = beta[1] * cx[i], t2 = beta[2] * cy[i];
            const double guard = 8.0 * std::numeric_limits<double>::epsilon() *
                                 (std::fabs(t0) + std::fabs(t1) + std::fabs(t2));
            if (!(t0 + t1 + t2 >= -guard)) return false;
        }
        return true;
    };
    // 零空间基（与生产的闭式主元/叉积构造不同源: 这里用对 a 的正交投影 + 叉积）
    const auto null_basis = [&](const double rows[3][3], std::size_t k, double Z[3][3]) {
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) Z[i][j] = (i == j) ? 1.0 : 0.0;
        if (k == 0) return std::size_t{3};
        if (k == 1) {
            std::size_t q = 0;
            for (std::size_t e = 0; e < 3; ++e) {
                double z[3] = {0.0, 0.0, 0.0};
                z[e] = 1.0;
                double proj = 0.0, nn = 0.0;
                for (std::size_t r = 0; r < 3; ++r) { proj += rows[0][r] * z[r]; nn += rows[0][r] * rows[0][r]; }
                if (!(nn > 0.0)) return std::size_t{0};
                for (std::size_t r = 0; r < 3; ++r) z[r] -= (proj / nn) * rows[0][r];
                double nz = 0.0;
                for (std::size_t r = 0; r < 3; ++r) nz += z[r] * z[r];
                if (std::sqrt(nz) <= 1e-12) continue;              // 与约束平行 ⇒ 丢弃
                for (std::size_t r = 0; r < 3; ++r) Z[r][q] = z[r] / std::sqrt(nz);
                ++q;
            }
            return q;
        }
        if (k == 2) {
            const double* a = rows[0];
            const double* b = rows[1];
            const double z[3] = {a[1] * b[2] - a[2] * b[1],
                                 a[2] * b[0] - a[0] * b[2],
                                 a[0] * b[1] - a[1] * b[0]};
            const double na = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
            const double nb = std::sqrt(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
            const double nz = std::sqrt(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
            if (!(nz > 1e-12 * na * nb)) return std::size_t{0};
            for (std::size_t r = 0; r < 3; ++r) Z[r][0] = z[r] / nz;
            return std::size_t{1};
        }
        return std::size_t{0};   // k = 3 ⇒ β = 0
    };
    // 加权缩减设计上的最小二乘: 两次修正 Gram-Schmidt（与生产的 Householder 不同路径）
    std::vector<double> B(static_cast<std::size_t>(n) * 3), Q(static_cast<std::size_t>(n) * 3);
    const auto lstsq_mgs = [&](std::size_t q, double* gamma) -> bool {
        std::vector<double> Rm(q * q, 0.0), col(n, 0.0);
        for (std::size_t j = 0; j < q; ++j) {
            for (std::uint32_t i = 0; i < n; ++i) col[i] = B[static_cast<std::size_t>(i) * q + j];
            for (int pass = 0; pass < 2; ++pass) {
                for (std::size_t l = 0; l < j; ++l) {
                    double dot = 0.0;
                    for (std::uint32_t i = 0; i < n; ++i) dot += Q[static_cast<std::size_t>(i) * q + l] * col[i];
                    Rm[l * q + j] += dot;
                    for (std::uint32_t i = 0; i < n; ++i) col[i] -= dot * Q[static_cast<std::size_t>(i) * q + l];
                }
            }
            double nrm = 0.0;
            for (std::uint32_t i = 0; i < n; ++i) nrm += col[i] * col[i];
            nrm = std::sqrt(nrm);
            if (!(nrm > 0.0) || !std::isfinite(nrm)) return false;
            Rm[j * q + j] = nrm;
            for (std::uint32_t i = 0; i < n; ++i) Q[static_cast<std::size_t>(i) * q + j] = col[i] / nrm;
        }
        std::vector<double> y(q, 0.0);
        for (std::size_t j = 0; j < q; ++j) {
            double dot = 0.0;
            for (std::uint32_t i = 0; i < n; ++i) dot += Q[static_cast<std::size_t>(i) * q + j] * rhs[i];
            y[j] = dot;
        }
        for (std::size_t jj = q; jj-- > 0;) {
            double s = y[jj];
            for (std::size_t k2 = jj + 1; k2 < q; ++k2) s -= Rm[jj * q + k2] * gamma[k2];
            const double d = Rm[jj * q + jj];
            if (!(std::fabs(d) > 0.0) || !std::isfinite(d)) return false;
            gamma[jj] = s / d;
            if (!std::isfinite(gamma[jj])) return false;
        }
        return true;
    };
    const auto solve_subset = [&](const double rows[3][3], std::size_t k, double* beta) -> bool {
        double Z[3][3];
        const std::size_t q = null_basis(rows, k, Z);
        if (k > 0 && q == 0) return false;
        if (q == 0) { beta[0] = beta[1] = beta[2] = 0.0; return true; }
        for (std::uint32_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < q; ++j) {
                double d = 0.0;
                for (std::size_t r = 0; r < 3; ++r) {
                    const double ri = (r == 0) ? 1.0 : ((r == 1) ? cx[i] : cy[i]);
                    d += Z[r][j] * ri;
                }
                B[static_cast<std::size_t>(i) * q + j] = sw[i] * d;
            }
        double gamma[3] = {0.0, 0.0, 0.0};
        if (!lstsq_mgs(q, gamma)) return false;
        for (int r = 0; r < 3; ++r) {
            double s = 0.0;
            for (std::size_t j = 0; j < q; ++j) s += Z[r][j] * gamma[j];
            beta[r] = s;
            if (!std::isfinite(beta[r])) return false;
        }
        return true;
    };
    // 约束枚举: 对**全部 n 个约束**枚举 |S| ≤ 3（生产用割平面 + 4 极值点起步的工作集）
    double best[3] = {0.0, 0.0, 0.0};
    bool have = false;
    double bestf = 0.0;
    const auto consider = [&](const std::uint32_t* S, std::size_t k) {
        double rows[3][3] = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}};
        for (std::size_t j = 0; j < k; ++j) {
            rows[j][0] = 1.0;
            rows[j][1] = cx[S[j]];
            rows[j][2] = cy[S[j]];
        }
        double beta[3];
        if (!solve_subset(rows, k, beta)) return;
        if (!feasible(beta)) return;
        const double f = objective(beta);
        if (!std::isfinite(f)) return;
        if (!have || f < bestf) {
            have = true; bestf = f;
            best[0] = beta[0]; best[1] = beta[1]; best[2] = beta[2];
        }
    };
    std::uint32_t S[3];
    consider(S, 0);
    for (std::uint32_t i = 0; i < n; ++i) { S[0] = i; consider(S, 1); }
    for (std::uint32_t i = 0; i < n; ++i)
        for (std::uint32_t j = i + 1; j < n; ++j) { S[0] = i; S[1] = j; consider(S, 2); }
    for (std::uint32_t i = 0; i < n; ++i)
        for (std::uint32_t j = i + 1; j < n; ++j)
            for (std::uint32_t k = j + 1; k < n; ++k) {
                S[0] = i; S[1] = j; S[2] = k; consider(S, 3);
            }
    if (!have) return p;
    p.a = best[0]; p.b = best[1]; p.c = best[2];
    p.solvable = true;
    return p;
}

// 由 build 落进 provenance 的平面系数构造 oracle（§5d ②）。
// 由 build 落进 provenance 的平面系数构造 oracle（§5d ②）。
// 用途: fill 的逐像素判据（预测 ≤ 0 ⇒ variance=0 ∧ ivar=0; > 0 ⇒ max(pred, floor)）
// 必须与**生产 build 的系数**逐位一致。期望值取自 build 的 provenance, 而不是产品面
// 自身的输出 ⇒ 判据非恒真（用产品面定义自己的期望才是恒真门）。
inline PlaneOracle plane_oracle_from_model(const NoiseWeightModelV1& m) {
    PlaneOracle p;
    p.a = m.plane_a;
    p.b = m.plane_b;
    p.c = m.plane_c;
    p.solvable = (m.has_spatial_field != 0) && (m.n_control_points >= 4);
    return p;
}

// fill 预测值 oracle (合同正本: SCI-NOISE-001 §5:58-60 / §7:112-113 / §9:136
// + DATA_SEMANTICS §4a 逐像素三态表:53-58)。判据与 floor 取值无关地成立:
//   预测 > 0 ⇒ 该处方差**可用**: variance = max(预测, floor)
//               (floor 是数值保护, **只作用于可用方差**, 保证 ivar 有限);
//   预测 ≤ 0 ⇒ 该处方差**不可用**: variance = 0
//               (显式不可用; **禁** clamp 成 floor —— 那会把「模型在此处失效」
//                伪造成 ivar=1/floor 的极大权重; 也**禁**写 NaN, NaN 保留给产品损坏)。
inline double fill_variance_oracle(const PlaneOracle& p, double x, double y,
                                   double floor_v) {
    const double pred = p.a + p.b * x + p.c * y;
    if (!(pred > 0.0)) return 0.0;
    return std::max(pred, floor_v);
}

// 与 fill_variance_oracle 同判据的 ivar oracle: 可用 ⇒ 精确倒数;
// 不可用 ⇒ 0 (禁 1/0 → +inf, 禁 NaN)。
inline double fill_ivar_oracle(const PlaneOracle& p, double x, double y,
                               double floor_v) {
    const double v = fill_variance_oracle(p, x, y, floor_v);
    return (v > 0.0) ? (1.0 / v) : 0.0;
}

// 产品 float32 面成对 oracle（SCI-NOISE-001 §7 互倒不变量 + §9 产品 dtype
// 可表示性 + DATA_SEMANTICS §4a 三态表在**输出 dtype**上的形式）:
//   可用态 = variance>0 ∧ isfinite(variance) ∧ ivar>0 ∧ isfinite(ivar);
//   否则取不可用态 (0, 0)。
// 触发后者的一种可构造场景: 生效 floor 在 float32 中下溢为 0
//   （如按 α² 换算后的 1e-46）而预测落在 (0, floor) ⇒ 1/floor 上溢为 +inf。
//   产品面**禁止**发布 (0, +inf) 这种自相矛盾的对。
inline void fill_pair_oracle_f32(const PlaneOracle& p, double x, double y,
                                 double floor_v, float* out_v, float* out_i) {
    const float v = static_cast<float>(fill_variance_oracle(p, x, y, floor_v));
    const float i = static_cast<float>(fill_ivar_oracle(p, x, y, floor_v));
    const bool ok = std::isfinite(v) && v > 0.0f && std::isfinite(i) && i > 0.0f;
    if (out_v) *out_v = ok ? v : 0.0f;
    if (out_i) *out_i = ok ? i : 0.0f;
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
