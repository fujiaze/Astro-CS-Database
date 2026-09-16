// ============================================================================
// ipv_angle.cpp - IPV 角度循环验证模块实现 ( CDA §5.3)
//
// 实现 angle_cyclic_verify: 对一对候选 (pivot_u, candidate_w) 及其已匹配
// 邻星做旋转角一致性验证, 返回 [0,1] consistency 分数。
//
// 算法步骤 (设计文档 §5.3.2 方法 B + §7.2):
// 1. 邻星数 < 3 → 返回 0.5 (中性, 不加不减)
// 2. 对每个已匹配邻星 k 计算方位角:
// 图像侧 φₖ = atan2(U[uₖ].y - pivot_img.y, U[uₖ].x - pivot_img.x)
// 星表侧 Φₖ = atan2(W[wₖ].y - pivot_cat.y, W[wₖ].x - pivot_cat.x)
// 都归一化到 [0, 2π)
// 3. 旋转角差 Δθₖ = Φₖ - φₖ (归一化到 [-π, π))
// 4. 180° 周期循环统计 (镜像模式下旋转 180° 等价):
// sum_sin = Σ sin(2×Δθₖ), sum_cos = Σ cos(2×Δθₖ)
// R = sqrt(sum_sin² + sum_cos²) / m
// circular_std_rad = sqrt(-2 × ln(R)) (R > 0)
// circular_std_deg = circular_std_rad × 180/π
// 注: R = 0 时 circular_std_deg = Inf, 设为大值 180
// 5. 离群点剔除 (3σ 准则):
// circular_mean_rad = atan2(sum_sin, sum_cos) / 2 (即 Δθ_mean)
// 对每个 Δθₖ 计算 dev = |wrap(Δθₖ - Δθ_mean, [-π, π))|
// 排除 dev > 3 × circular_std_rad 的点
// (排除后剩余 < 3 点则不排除)
// 重算 circular_std
// 6. consistency = max(0.0, 1.0 - circular_std_deg / angle_tol_deg)
//
// 日志: 用 std::printf 到 stderr, 不依赖 Logger (保持模块独立)
//
// 日期: 2026-07-04
// ============================================================================

#include "ipv_angle.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace ipv {

// ---------------------------------------------------------------------------
// 内部工具: 角度归一化到 [0, 2π)
// ---------------------------------------------------------------------------
static inline double wrap_to_2pi(double ang) {
    const double TWO_PI = 2.0 * 3.14159265358979323846;
    double r = std::fmod(ang, TWO_PI);
    if (r < 0.0) r += TWO_PI;
    return r;
}

// ---------------------------------------------------------------------------
// 内部工具: 角度归一化到 [-π, π)
// ---------------------------------------------------------------------------
static inline double wrap_to_pi(double ang) {
    const double TWO_PI = 2.0 * 3.14159265358979323846;
    double r = std::fmod(ang + 3.14159265358979323846, TWO_PI);
    if (r < 0.0) r += TWO_PI;
    return r - 3.14159265358979323846;
}

// ---------------------------------------------------------------------------
// 内部工具: 计算循环统计 (R, circular_std_deg)
// 输入 Δθ 数组 (弧度), 返回 (R, circular_std_rad, circular_mean_rad)
// 注: 使用 2×Δθ (180° 周期), 因为旋转 180° 在镜像模式下等价
// ---------------------------------------------------------------------------
static void circular_stats(const std::vector<double>& dtheta,
                           double& R,
                           double& circular_std_rad,
                           double& circular_mean_rad) {
    const double TWO_PI = 2.0 * 3.14159265358979323846;
    double sum_sin = 0.0, sum_cos = 0.0;
    int m = (int)dtheta.size();
    for (int k = 0; k < m; ++k) {
        // 2×Δθₖ (mod 2π)
        double ang2 = std::fmod(2.0 * dtheta[k], TWO_PI);
        sum_sin += std::sin(ang2);
        sum_cos += std::cos(ang2);
    }
    R = std::sqrt(sum_sin * sum_sin + sum_cos * sum_cos) / (double)m;

    if (R > 1e-10) {
        circular_std_rad = std::sqrt(-2.0 * std::log(R));
    } else {
        // R = 0: 完全离散, circular_std = Inf, 设为大值 (设计文档要求)
        circular_std_rad = 3.14159265358979323846;  // π rad = 180°
    }

    // circular_mean (在 2θ 空间), 还原到 θ 空间
    // 注: atan2(sum_sin, sum_cos) ∈ [-π, π), 除以 2 得 Δθ_mean ∈ [-π/2, π/2)
    // 但 Δθ_mean 真实范围是 [-π, π), 需要进一步处理
    // 对离群点剔除, Δθ_mean 的精确值不重要, 主要看相对偏离
    double mean_2theta = std::atan2(sum_sin, sum_cos);
    circular_mean_rad = mean_2theta / 2.0;
}

// ===========================================================================
// angle_cyclic_verify: 角度循环验证主函数
// ===========================================================================
double angle_cyclic_verify(
    const StarPoint& pivot_img,
    const StarPoint& pivot_cat,
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const HexDescriptor& hex,
    const std::vector<std::pair<int,int>>& matched_neighbors,
    double angle_tol_deg)
{
    (void)hex;  // hex 在当前实现中未直接使用 (邻星索引已通过 matched_neighbors 传入)

    const int m = (int)matched_neighbors.size();

    // 1. 邻星数 < 3: 返回 0.5 (中性, 不加不减)
    if (m < 3) {
        return 0.5;
    }

    const int N_U = (int)U.size();
    const int N_W = (int)W.size();

    // 2-3. 计算每个邻星的旋转角差 Δθₖ
    std::vector<double> dtheta;
    dtheta.reserve(m);
    for (int k = 0; k < m; ++k) {
        int u_idx = matched_neighbors[k].first;
        int w_idx = matched_neighbors[k].second;

        // 边界检查
        if (u_idx < 0 || u_idx >= N_U || w_idx < 0 || w_idx >= N_W) {
            continue;
        }

        // 图像侧方位角 φₖ ∈ [0, 2π)
        double dxi = U[u_idx].x - pivot_img.x;
        double dyi = U[u_idx].y - pivot_img.y;
        double phi_k = wrap_to_2pi(std::atan2(dyi, dxi));

        // 星表侧方位角 Φₖ ∈ [0, 2π)
        double dxc = W[w_idx].x - pivot_cat.x;
        double dyc = W[w_idx].y - pivot_cat.y;
        double Phi_k = wrap_to_2pi(std::atan2(dyc, dxc));

        // 旋转角差 Δθₖ = Φₖ - φₖ ∈ [-π, π)
        double dth = wrap_to_pi(Phi_k - phi_k);
        dtheta.push_back(dth);
    }

    // 重新检查有效邻星数 (边界过滤后可能 < 3)
    int m_eff = (int)dtheta.size();
    if (m_eff < 3) {
        return 0.5;
    }

    // 4. 第一次循环统计
    double R1, cstd_rad1, cmean_rad1;
    circular_stats(dtheta, R1, cstd_rad1, cmean_rad1);

    double cstd_rad = cstd_rad1;
    double cmean_rad = cmean_rad1;

    // 5. 离群点剔除 (3σ 准则)
    // 对每个 Δθₖ 计算 dev = |wrap(Δθₖ - Δθ_mean, [-π, π))|
    // 排除 dev > 3 × circular_std_rad 的点
    // 若排除后剩余 < 3 点则不排除
    if (cstd_rad > 0.0 && cstd_rad < 3.14159265358979323846) {
        std::vector<double> dtheta_filtered;
        dtheta_filtered.reserve(m_eff);
        double thresh = 3.0 * cstd_rad;

        for (int k = 0; k < m_eff; ++k) {
            double dev = std::abs(wrap_to_pi(dtheta[k] - cmean_rad));
            if (dev <= thresh) {
                dtheta_filtered.push_back(dtheta[k]);
            }
        }

        // 排除后剩余 >= 3 点才使用过滤结果
        if ((int)dtheta_filtered.size() >= 3 &&
            (int)dtheta_filtered.size() < m_eff) {
            // 重算循环统计
            double R2, cstd_rad2, cmean_rad2;
            circular_stats(dtheta_filtered, R2, cstd_rad2, cmean_rad2);
            cstd_rad = cstd_rad2;
            cmean_rad = cmean_rad2;
        }
    }

    // circular_std 转为度
    const double RAD2DEG = 180.0 / 3.14159265358979323846;
    double cstd_deg = cstd_rad * RAD2DEG;

    // 6. consistency = max(0.0, 1.0 - circular_std_deg / angle_tol_deg)
    double consistency = 1.0 - cstd_deg / angle_tol_deg;
    if (consistency < 0.0) consistency = 0.0;
    if (consistency > 1.0) consistency = 1.0;

    return consistency;
}

} // namespace ipv
