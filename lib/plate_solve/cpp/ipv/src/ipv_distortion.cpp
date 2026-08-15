// ============================================================================
// ipv_distortion.cpp - V4.11 CDA Phase B: 径向畸变估计实现
//
// 设计参考: ipv_cda_distortion_design.md §4
// 流程:
//   1. 用 WCS₀ 把 W 投影到图像坐标系 → 预测位置 (x̂, ŷ)
//   2. 在 U_full 中做宽容近邻匹配 (τ=15px) → 匹配对
//   3. 计算残差 dx = U.x - x̂, dy = U.y - ŷ
//   4. MAD 清洗外点
//   5. IRLS + Huber 权重拟合 (k1, k2)
//   6. 计算 R² 质量评估
// ============================================================================

#include "ipv_distortion.h"

#include <cmath>
#include <cstdio>
#include <algorithm>
#include <vector>

namespace ipv {

namespace {

// 计算归一化半径 r̃
// U 坐标原点已在图像中心, 所以 (x-cx) → x, (y-cy) → y
// r̃² = x²/(W/2)² + y²/(H/2)²
inline double normalized_radius_sq(double x, double y,
                                    int img_width, int img_height) {
    double half_w = img_width / 2.0;
    double half_h = img_height / 2.0;
    return (x * x) / (half_w * half_w) + (y * y) / (half_h * half_h);
}

// 计算畸变预测的残差 (dx_pred, dy_pred)
// dr = k1·r̃² + k2·r̃⁴
// dx_pred = x × dr, dy_pred = y × dr
inline void distortion_predict(double x, double y,
                                double k1, double k2,
                                int img_width, int img_height,
                                double& dx_pred, double& dy_pred) {
    double r_tilde_sq = normalized_radius_sq(x, y, img_width, img_height);
    double dr = k1 * r_tilde_sq + k2 * r_tilde_sq * r_tilde_sq;
    dx_pred = x * dr;
    dy_pred = y * dr;
}

// 计算向量中位数
inline double median(std::vector<double>& v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    return (n % 2 == 0) ? (v[n/2 - 1] + v[n/2]) / 2.0 : v[n/2];
}

} // namespace

// ----------------------------------------------------------------------------
// estimate_radial_distortion
// ----------------------------------------------------------------------------
DistortionModel estimate_radial_distortion(
    const std::vector<StarPoint>& U_full,
    const std::vector<StarPoint>& W,
    const SimTransform& tf,
    double /*s0*/,
    int img_width,
    int img_height,
    double tau_match,
    Logger* logger)
{
    DistortionModel result;
    result.valid = false;

    if (!tf.valid) {
        if (logger) logger->warn("estimate_radial_distortion: 变换无效");
        return result;
    }

    const double cos_t = std::cos(tf.theta);
    const double sin_t = std::sin(tf.theta);
    const double s = tf.s;

    // --- 步骤 1+2: 投影 W 到图像坐标系, 在 U_full 中找最近邻 ---
    struct MatchPairData {
        double ux, uy;       // U 坐标 (像素, 原点中心)
        double pred_x, pred_y;  // 预测坐标 (像素, 原点中心)
        double dx, dy;       // 残差 = U - pred
        double r_err;        // 径向残差
    };
    std::vector<MatchPairData> matches;

    // V4.11 修复: Phase A 求解的模型是 W' = s·R·U + t (U→W')
    //            要把 W' 投影回 U 坐标系, 需用反变换: U = (1/s)·R⁻¹·(W' - t)
    //            R⁻¹ = Rᵀ = [[cos θ, sin θ], [-sin θ, cos θ]]
    //            注意: 传入的 W 必须是 flip 后的 W' (与 tf 对应)
    const double inv_s = 1.0 / s;
    for (size_t j = 0; j < W.size(); ++j) {
        // 反变换: U_pred = (1/s)·Rᵀ·(W' - t)
        double dx = W[j].x - tf.tx;
        double dy = W[j].y - tf.ty;
        double pred_x = inv_s * (cos_t * dx + sin_t * dy);
        double pred_y = inv_s * (-sin_t * dx + cos_t * dy);

        // 在 U_full 中找最近邻 (暴力搜索, N_U < 500)
        double best_dist = 1e18;
        int best_u = -1;
        for (size_t i = 0; i < U_full.size(); ++i) {
            double ddx = U_full[i].x - pred_x;
            double ddy = U_full[i].y - pred_y;
            double d = ddx * ddx + ddy * ddy;
            if (d < best_dist) {
                best_dist = d;
                best_u = (int)i;
            }
        }

        if (best_u < 0) continue;
        double dist = std::sqrt(best_dist);
        if (dist > tau_match) continue;

        MatchPairData mp;
        mp.ux = U_full[best_u].x;
        mp.uy = U_full[best_u].y;
        mp.pred_x = pred_x;
        mp.pred_y = pred_y;
        mp.dx = mp.ux - pred_x;
        mp.dy = mp.uy - pred_y;
        mp.r_err = std::sqrt(mp.dx * mp.dx + mp.dy * mp.dy);
        matches.push_back(mp);
    }

    if ((int)matches.size() < 10) {
        if (logger) logger->warnf("estimate_radial_distortion: 匹配对数 %d < 10, 拟合失败",
                                  (int)matches.size());
        return result;
    }

    if (logger) logger->infof("  Phase B: 初始匹配对 %d", (int)matches.size());

    // --- 步骤 3: MAD 清洗外点 ---
    // 按径向残差排序, 检测跳变
    std::vector<double> r_errs(matches.size());
    for (size_t i = 0; i < matches.size(); ++i) r_errs[i] = matches[i].r_err;
    std::sort(r_errs.begin(), r_errs.end());

    std::vector<double> deltas(r_errs.size() > 1 ? r_errs.size() - 1 : 0);
    for (size_t i = 0; i + 1 < r_errs.size(); ++i) {
        deltas[i] = r_errs[i+1] - r_errs[i];
    }
    double delta_mad = median(deltas) * 1.4826;
    if (delta_mad < 1e-9) delta_mad = 1.0;  // 防止除零

    // 找第一个跳变 > 5×MAD
    size_t keep_count = matches.size();
    for (size_t i = 0; i + 1 < r_errs.size(); ++i) {
        if (deltas[i] > 5.0 * delta_mad) {
            keep_count = i + 1;
            break;
        }
    }

    // 保留 keep_count 个最小残差的匹配对
    std::vector<MatchPairData> inlier_matches;
    {
        // 用偏序: 把 matches 按 r_err 排序, 取前 keep_count
        std::vector<int> idx(matches.size());
        for (size_t i = 0; i < matches.size(); ++i) idx[i] = (int)i;
        std::sort(idx.begin(), idx.end(),
                  [&](int a, int b) { return matches[a].r_err < matches[b].r_err; });
        for (size_t i = 0; i < keep_count && i < idx.size(); ++i) {
            inlier_matches.push_back(matches[idx[i]]);
        }
    }

    if ((int)inlier_matches.size() < 10) {
        if (logger) logger->warnf("Phase B: 清洗后匹配对 %d < 10", (int)inlier_matches.size());
        return result;
    }

    if (logger) logger->infof("  Phase B: 清洗后匹配对 %d", (int)inlier_matches.size());

    // --- 步骤 4: 计算原始残差统计 (用于 R²) ---
    double raw_mean = 0.0;
    for (const auto& m : inlier_matches) raw_mean += m.r_err;
    raw_mean /= inlier_matches.size();
    double raw_var = 0.0;
    for (const auto& m : inlier_matches) {
        double d = m.r_err - raw_mean;
        raw_var += d * d;
    }
    raw_var /= inlier_matches.size();

    // --- 步骤 5: IRLS + Huber 权重拟合 (k1, k2) ---
    // 模型: dx_pred = x × (k1·r̃² + k2·r̃⁴)
    //       dy_pred = y × (k1·r̃² + k2·r̃⁴)
    // 残差方程: dx_obs = dx_pred + ε → x·r̃²·k1 + x·r̃⁴·k2 = dx_obs
    //          dy_obs = dy_pred + ε → y·r̃²·k1 + y·r̃⁴·k2 = dy_obs
    // 最小二乘: A·[k1, k2]ᵀ = b, A 是 2N×2, b 是 2N×1
    // 加权: wᵢ × Aᵢ·[k1,k2] = wᵢ × bᵢ

    double k1 = 0.0, k2 = 0.0;
    const int max_irls_iter = 15;
    double prev_k1 = 1e9, prev_k2 = 1e9;

    for (int iter = 0; iter < max_irls_iter; ++iter) {
        // 计算当前残差
        std::vector<double> residuals(inlier_matches.size());
        for (size_t i = 0; i < inlier_matches.size(); ++i) {
            double dx_p, dy_p;
            distortion_predict(inlier_matches[i].ux, inlier_matches[i].uy,
                              k1, k2, img_width, img_height, dx_p, dy_p);
            double rx = inlier_matches[i].dx - dx_p;
            double ry = inlier_matches[i].dy - dy_p;
            residuals[i] = std::sqrt(rx * rx + ry * ry);
        }

        // Huber δ = 1.345 × MAD(residuals)
        std::vector<double> r_copy = residuals;
        double mad = median(r_copy) * 1.4826;
        if (mad < 1e-9) mad = 1.0;
        double delta = 1.345 * mad;

        // 加权最小二乘: 形成 2x2 正规方程
        double A00 = 0, A01 = 0, A11 = 0;
        double b0 = 0, b1 = 0;
        for (size_t i = 0; i < inlier_matches.size(); ++i) {
            double w = (residuals[i] < delta) ? 1.0 : (delta / std::max(residuals[i], 1e-9));
            double x = inlier_matches[i].ux;
            double y = inlier_matches[i].uy;
            double r_tilde_sq = normalized_radius_sq(x, y, img_width, img_height);
            double r_tilde_4 = r_tilde_sq * r_tilde_sq;

            // 每个匹配对贡献 2 个方程 (dx 和 dy)
            // dx 方程: x·r̃²·k1 + x·r̃⁴·k2 = dx_obs
            // dy 方程: y·r̃²·k1 + y·r̃⁴·k2 = dy_obs
            double a_dx_0 = x * r_tilde_sq;
            double a_dx_1 = x * r_tilde_4;
            double a_dy_0 = y * r_tilde_sq;
            double a_dy_1 = y * r_tilde_4;

            A00 += w * (a_dx_0 * a_dx_0 + a_dy_0 * a_dy_0);
            A01 += w * (a_dx_0 * a_dx_1 + a_dy_0 * a_dy_1);
            A11 += w * (a_dx_1 * a_dx_1 + a_dy_1 * a_dy_1);
            b0   += w * (a_dx_0 * inlier_matches[i].dx + a_dy_0 * inlier_matches[i].dy);
            b1   += w * (a_dx_1 * inlier_matches[i].dx + a_dy_1 * inlier_matches[i].dy);
        }

        // 求解 2x2: [A00 A01; A01 A11] [k1; k2] = [b0; b1]
        double det = A00 * A11 - A01 * A01;
        if (std::abs(det) < 1e-18) {
            if (logger) logger->warn("Phase B: 正规方程奇异, 拟合失败");
            return result;
        }
        double new_k1 = (A11 * b0 - A01 * b1) / det;
        double new_k2 = (A00 * b1 - A01 * b0) / det;

        // 收敛判定
        if (std::abs(new_k1 - prev_k1) < 1e-6 && std::abs(new_k2 - prev_k2) < 1e-6) {
            k1 = new_k1; k2 = new_k2;
            if (logger) logger->infof("  Phase B: IRLS 收敛 (iter=%d)", iter + 1);
            break;
        }
        k1 = new_k1; k2 = new_k2;
        prev_k1 = new_k1; prev_k2 = new_k2;
    }

    if (logger) logger->infof("  Phase B: IRLS 最终 k1=%.6f, k2=%.6f", k1, k2);

    // --- 步骤 6: 计算拟合 RMS 和 R² ---
    double sum_sq_res = 0.0;
    for (const auto& m : inlier_matches) {
        double dx_p, dy_p;
        distortion_predict(m.ux, m.uy, k1, k2, img_width, img_height, dx_p, dy_p);
        double rx = m.dx - dx_p;
        double ry = m.dy - dy_p;
        sum_sq_res += rx * rx + ry * ry;
    }
    double fit_rms = std::sqrt(sum_sq_res / inlier_matches.size());
    double res_var = sum_sq_res / inlier_matches.size();
    double r_squared = (raw_var > 1e-9) ? (1.0 - res_var / raw_var) : 0.0;

    // V4.12 修复: R² < 0 时退化为单参数模型 (宽 FOV 边缘畸变导致双参数拟合失败)
    // 单参数: k1 = median(dx_i / (x_i * r̃_i²)), k2 = 0
    // 对 x 和 y 两个方向的贡献都计算, 取所有内点的中位数
    if (r_squared < 0.0) {
        if (logger) logger->warnf("Phase B R²=%.3f < 0, 退化为单参数 k₁ 模型", r_squared);
        std::vector<double> k1_samples;
        k1_samples.reserve(inlier_matches.size() * 2);
        for (const auto& m : inlier_matches) {
            double r_tilde_sq = normalized_radius_sq(m.ux, m.uy, img_width, img_height);
            if (r_tilde_sq < 1e-9) continue;
            // x 分量: dx = x * k1 * r̃² → k1 = dx / (x * r̃²)
            if (std::abs(m.ux) > 1e-6) {
                k1_samples.push_back(m.dx / (m.ux * r_tilde_sq));
            }
            // y 分量: dy = y * k1 * r̃² → k1 = dy / (y * r̃²)
            if (std::abs(m.uy) > 1e-6) {
                k1_samples.push_back(m.dy / (m.uy * r_tilde_sq));
            }
        }
        if (!k1_samples.empty()) {
            k1 = median(k1_samples);
            k2 = 0.0;
            // 重新计算单参数模型的 fit_rms
            double sum_sq_res_single = 0.0;
            for (const auto& m : inlier_matches) {
                double dx_p, dy_p;
                distortion_predict(m.ux, m.uy, k1, k2, img_width, img_height, dx_p, dy_p);
                double rx = m.dx - dx_p;
                double ry = m.dy - dy_p;
                sum_sq_res_single += rx * rx + ry * ry;
            }
            fit_rms = std::sqrt(sum_sq_res_single / inlier_matches.size());
            if (logger) logger->infof("  Phase B 单参数: k1=%.6f, k2=0, RMS=%.3f px", k1, fit_rms);
        }
    }

    result.k1 = k1;
    result.k2 = k2;
    result.fit_rms_px = fit_rms;
    result.r_squared = r_squared;
    result.n_pairs = (int)inlier_matches.size();
    result.valid = (std::abs(k1) < 0.1) && (fit_rms < 5.0);

    if (logger) {
        logger->infof("  Phase B 完成: k1=%.4f, k2=%.4f, RMS=%.3f px, R²=%.3f, n=%d, valid=%d",
                      k1, k2, fit_rms, r_squared, (int)inlier_matches.size(),
                      (int)result.valid);
    }

    return result;
}

// ----------------------------------------------------------------------------
// undistort_stars
// ----------------------------------------------------------------------------
std::vector<StarPoint> undistort_stars(
    const std::vector<StarPoint>& U,
    const DistortionModel& dist,
    int img_width,
    int img_height,
    Logger* logger)
{
    std::vector<StarPoint> result;
    result.reserve(U.size());

    if (!dist.valid) {
        // 不做去畸变, 直接复制
        result = U;
        return result;
    }

    for (const auto& u : U) {
        double r_tilde_sq = normalized_radius_sq(u.x, u.y, img_width, img_height);
        double dr = dist.k1 * r_tilde_sq + dist.k2 * r_tilde_sq * r_tilde_sq;
        StarPoint p;
        p.x = u.x - u.x * dr;
        p.y = u.y - u.y * dr;
        p.flux = u.flux;
        p.saturated = u.saturated;
        result.push_back(p);
    }

    if (logger) {
        logger->infof("  undistort_stars: %d 颗星, k1=%.4f, k2=%.4f",
                      (int)result.size(), dist.k1, dist.k2);
    }

    return result;
}

} // namespace ipv
