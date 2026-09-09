// ============================================================================
// P1-PSF-TEST · 独立 oracle (测试侧科学真值 + 期望值, 不调用被测函数)
// ----------------------------------------------------------------------------
// 合同锚: docs/science/PSF.md (Moffat4 模型), docs/algorithms/
// STAR_PSF_ALGORITHMS.md §11 TEST-PSF-DESIGN-001, lib/dynamic_psf/README.md
// §5 (容差元数据), 11_MODULE_SOURCE_TEST_STANDARD.md §5 (容差须引用来源)。
//
// 独立性: 本文件对 Moffat4 的采样/积分/偏心率的实现与生产源
// (lib/dynamic_psf/src/dpsf_psf.cpp) 无共享代码路径——生产代码用
// p1/p2/p3 二次型 + 分解 LM, 测试侧用旋转主轴投影直接公式; 两侧仅在
// 数学定义 (PSF.md 冻结) 上等价。期望值全部由下列独立实现生成, 绝不
// 调用被测函数 (模板 <prefix>-TEST 验收硬性要求)。
// ============================================================================
#ifndef P1PSF_ORACLE_HPP
#define P1PSF_ORACLE_HPP

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "dynamic_psf.h"  // 仅类型 (DPSFFitResult/DPSFFitParams), 不调用函数

namespace p1psf {

// ---------------------------------------------------------------------------
// Moffat4 剖面 (β=4, α=√2σ): I = B + A/(1+Q)^4
//   Q = 0.5·r^T M^{-1} r; 主轴投影 u = cosθ·dx + sinθ·dy (σ=sx),
//   v = −sinθ·dx + cosθ·dy (σ=sy)  → Q = 0.5·(u²/sx² + v²/sy²)
// 来源: docs/science/PSF.md Moffat4 定义 (kMoffat4Beta=4, kMoffat4AlphaScale=√2)
// ---------------------------------------------------------------------------
inline double moffat4_eval(double B, double A, double cx, double cy,
                           double sx, double sy, double theta,
                           double x, double y) {
    const double dx = x - cx, dy = y - cy;
    const double c = std::cos(theta), s = std::sin(theta);
    const double u = c * dx + s * dy;    // 主轴投影 (σ=sx)
    const double v = -s * dx + c * dy;   // 垂轴投影 (σ=sy)
    const double Q = 0.5 * (u * u / (sx * sx) + v * v / (sy * sy));
    return B + A / std::pow(1.0 + Q, 4.0);
}

// ---------------------------------------------------------------------------
// 解析恒等式 (PSF.md 冻结定义, 独立于生产实现)
// ---------------------------------------------------------------------------
// β=4 Moffat 整平面解析积分: flux = 2π·A·sx·sy/3
inline double oracle_flux(double A, double sx, double sy) {
    return 2.0 * M_PI * A * sx * sy / 3.0;
}

// FWHM 因子: 2√2·√(2^{1/4}−1) = 1.230310 (MOFFAT4_FWHM_FACTOR)
inline constexpr double kMoffat4FwhmFactor = 1.230310;
inline double oracle_fwhm(double sigma) { return kMoffat4FwhmFactor * sigma; }

// 偏心率: e = √(1 − (smin/smax)²), smin/smax 为次/主轴 sigma
inline double oracle_eccentricity(double sx, double sy) {
    const double smax = std::max(sx, sy);
    const double smin = std::min(sx, sy);
    return std::sqrt(1.0 - (smin / smax) * (smin / smax));
}

// ---------------------------------------------------------------------------
// 椭圆等价 oracle (θ 简并无关的独立表述): 由 (sx, sy, θ) 重建对称协方差矩阵
//   M = R(θ)·diag(sx², sy²)·R(θ)^T, R 为标准旋转矩阵
// 与生产源 p1/p2/p3 二次型代数 (dpsf_psf.cpp:19-30 注释块) 无共享代码——
// 本处从旋转矩阵定义直接出发。θ 简并四元组 {θ, π/2−θ, π/2+θ, π−θ} 生成
// 逐元素相同的 M (Moffat4 椭圆等价, PSF.md), 因此 M 等价是稳健的形状判据。
//   returns row-major M = [Mxx, Mxy; Myx, Myy]
// ---------------------------------------------------------------------------
inline void oracle_m_matrix(double sx, double sy, double theta, double M[4]) {
    const double c = std::cos(theta), s = std::sin(theta);
    const double a = sx * sx, b = sy * sy;
    M[0] = c * c * a + s * s * b;   // Mxx = cos²θ·sx² + sin²θ·sy²
    M[1] = c * s * (a - b);         // Mxy = cosθ·sinθ·(sx² − sy²)
    M[2] = M[1];                    // 对称
    M[3] = s * s * a + c * c * b;   // Myy = sin²θ·sx² + cos²θ·sy²
}

// 相对误差 (双参考点容差)
inline double rel_err(double got, double want) {
    return std::fabs(got - want) / std::max(1e-30, std::fabs(want));
}

// 混合容差判定 (绝对 + 相对): want≈0 的分量 rel_err 发散 (got 的机器噪声
// 除以 1e-30 地板), M 矩阵等价判据等含零分量的场合必须用本函数。
// 容差语义: |got-want| ≤ rel · max(1, |want|) — 非零分量等效相对容差,
// 零分量退化为绝对容差 rel。
inline bool close_hybrid(double got, double want, double rel) {
    return std::fabs(got - want) <= rel * std::max(1.0, std::fabs(want));
}

// ---------------------------------------------------------------------------
// 特征轴 sigma (2x2 对称正定矩阵解析特征值, 谱 = {sx², sy²}):
//   λ1,2 = (Mxx+Myy)/2 ± √[((Mxx−Myy)/2)² + Mxy²],  σi = √λi (降序)
// 与 (sx, sy, θ) 具体代表无关 (θ 简并等价类共享同一谱), 且对噪声场下
// M 交叉项 (Mxy) 的估计噪声不敏感 — 多星批量回收的形状判据用谱, 无噪声
// 回收才用 M 元素逐项等价 (probe 无噪声 M rel ≤2e-9)。
// ---------------------------------------------------------------------------
inline void oracle_sigma_axes(const double M[4], double sigma[2]) {
    const double mid = 0.5 * (M[0] + M[3]);
    const double d = 0.5 * (M[0] - M[3]);
    const double rad = std::sqrt(std::max(0.0, d * d + M[1] * M[1]));
    sigma[0] = std::sqrt(std::max(0.0, mid + rad));  // 主轴
    sigma[1] = std::sqrt(std::max(0.0, mid - rad));  // 次轴
}

// ---------------------------------------------------------------------------
// 容差表 (README §5 + 11_MODULE_SOURCE_TEST_STANDARD §5 元数据: 每项引用来源)
// 无噪声解析回收实证 (probe, 2026-09-09): f64 路径 |ΔB|≤7e-9, |ΔA|≤1.5e-6,
// Δcx/cy≤1e-14, |Δsx/sy|≤4e-9, M 元素 rel ≤2e-9 → 紧容差冻结如下。
// 容差为测试合同, 不改科学定义 (AstroCS_ENGINEERING_CONSTRAINTS §E)。
// ---------------------------------------------------------------------------
struct Tolerances {
    double b_abs   = 1e-6;    // B 绝对 (背景水平 ~50-200, 无噪声回收 7e-9)
    double a_rel   = 5e-6;    // A 相对 (无噪声回收 1.46e-6@A=3000, 留 ~3.4× 余量)
    double cen_abs = 1e-9;    // cx/cy 绝对 px (无噪声回收 ≤1e-14)
    double s_rel   = 1e-6;    // sx/sy 相对 (无噪声回收 ≤4.3e-9)
    double m_rel   = 3e-3;    // M 元素相对 (θ 简并等价判据; 噪声场回退用)
    double fwhm_rel = 1e-12;  // fwhm_x = kFwhmFactor·sx 恒等式 (同一 double 乘法)
    double flux_rel = 1e-12;  // flux = 2πA·sx·sy/3 恒等式 (oracle 同式重算)
};

inline const Tolerances& tol() {
    static const Tolerances t;
    return t;
}

}  // namespace p1psf

#endif  // P1PSF_ORACLE_HPP
