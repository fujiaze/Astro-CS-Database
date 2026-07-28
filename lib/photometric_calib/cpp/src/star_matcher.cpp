// star_matcher.cpp - 星-图匹配器 (GAP-013 改进版)
// 功能: 将Gaia参考星与图像PSF拟合星进行空间匹配, IRLS+Tukey 稳健清洗
// 算法:
//   - KD-tree 最近邻匹配 (在 Gaia 星像素坐标上建树, 对每颗 PSF 星查询)
//   - 星等一致性预过滤 (|delta - median_delta| > 3 mag 拒绝)
//   - IRLS + Tukey biweight 稳健位置估计 (c=4.685, 50 次迭代, 收敛 1e-6)
// 参考: lib/photometric_calib/flux_calibrator/python/star_matcher.py

#include "star_matcher.h"
#include "log_macros.h"
#include "photometric_calib.h"  // P12-001: PhotometricDiag 定义

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>
#include <numeric>

namespace pc {

static constexpr double _MAD_SCALE = 0.6745;
// Tukey biweight 常数 c=4.685 (标准稳健统计, 对应 95% 正态效率)
static constexpr double _TUKEY_C = 4.685;
// IRLS 最大迭代次数
static constexpr int _IRLS_MAX_ITER = 50;
// IRLS 收敛阈值 (|scale_new - scale_old| < 1e-6)
static constexpr double _IRLS_CONVERGE = 1e-6;

// ============================================================================
// P12-001: 百分位数计算 (线性插值), 用于 r_inliers 和 match_distance 的 p90 统计
// ============================================================================
static double percentileOf(std::vector<double> vals, double p) {
    if (vals.empty()) return 0.0;
    std::sort(vals.begin(), vals.end());
    size_t n = vals.size();
    if (n == 1) return vals[0];
    double idx = p * (double)(n - 1);
    size_t lo = (size_t)std::floor(idx);
    size_t hi = (lo + 1 < n) ? (lo + 1) : (n - 1);
    double frac = idx - (double)lo;
    return vals[lo] * (1.0 - frac) + vals[hi] * frac;
}

// P12-001: 初始化 PhotometricDiag (全 0)
static void initDiag(PhotometricDiag* d) {
    if (!d) return;
    d->spectrum_rows_total = 0;
    d->valid_fsyn = 0;
    d->gaia_projected_in_frame = 0;
    d->psf_total = 0;
    d->psf_valid = 0;
    d->spatial_candidates = 0;
    d->unique_matches = 0;
    d->rejected_ambiguous = 0;
    d->rejected_distance = 0;
    d->rejected_quality = 0;
    d->fit_used = 0;
    d->robust_iterations = 0;
    d->scale_factor = 0.0;
    d->sigma_residual = 0.0;
    d->r_median = 0.0; d->r_p90 = 0.0; d->r_max = 0.0;
    d->match_distance_median = 0.0; d->match_distance_p90 = 0.0; d->match_distance_max = 0.0;
}

// ============================================================================
// 内部: 简单 2D KD-tree (替代 nanoflann, Gaia 星通常 <10000, 自实现足够)
// 仅支持最近邻查询 (within max_dist2)
// ============================================================================
class KdTree2D {
public:
    KdTree2D(const std::vector<double>& xs, const std::vector<double>& ys)
        : points_(xs.size()) {
        for (size_t i = 0; i < xs.size(); ++i) {
            points_[i] = std::make_pair(xs[i], ys[i]);
        }
        std::vector<int> indices(xs.size());
        std::iota(indices.begin(), indices.end(), 0);
        root_ = build(indices, 0);
    }

    ~KdTree2D() { destroy(root_); }

    // 在 max_dist2 范围内找最近邻, 返回索引或 -1
    int findNearest(double x, double y, double max_dist2) const {
        int best_idx = -1;
        double best_dist2 = max_dist2;
        findNearestRec(root_, x, y, best_idx, best_dist2);
        return best_idx;
    }

private:
    struct Node {
        int idx;     // 原始索引
        int axis;    // 0=x, 1=y
        Node* left;
        Node* right;
    };

    std::vector<std::pair<double, double>> points_;
    Node* root_;

    Node* build(std::vector<int>& indices, int axis) {
        if (indices.empty()) return nullptr;
        if (axis >= 2) axis = 0;  // 仅 2 维, 循环切分

        // 按当前轴排序, 取中位数作为根
        std::sort(indices.begin(), indices.end(),
                  [&](int a, int b) {
                      return axis == 0 ? points_[a].first < points_[b].first
                                       : points_[a].second < points_[b].second;
                  });
        size_t mid = indices.size() / 2;
        int midx = indices[mid];

        Node* node = new Node;
        node->idx = midx;
        node->axis = axis;
        std::vector<int> left_idx(indices.begin(), indices.begin() + mid);
        std::vector<int> right_idx(indices.begin() + mid + 1, indices.end());
        node->left = build(left_idx, axis + 1);
        node->right = build(right_idx, axis + 1);
        return node;
    }

    void findNearestRec(Node* node, double x, double y,
                        int& best_idx, double& best_dist2) const {
        if (node == nullptr) return;
        const auto& p = points_[node->idx];
        double dx = p.first - x;
        double dy = p.second - y;
        double dist2 = dx * dx + dy * dy;
        if (dist2 < best_dist2) {
            best_dist2 = dist2;
            best_idx = node->idx;
        }

        // 决定先访问哪个子树 (按当前轴的分裂方向)
        double diff = (node->axis == 0) ? dx : dy;
        Node* first = (diff < 0) ? node->left : node->right;
        Node* second = (diff < 0) ? node->right : node->left;

        findNearestRec(first, x, y, best_idx, best_dist2);

        // 检查另一侧是否可能有更近的点 (基于分裂平面的距离)
        double split_dist2 = diff * diff;
        if (split_dist2 < best_dist2) {
            findNearestRec(second, x, y, best_idx, best_dist2);
        }
    }

    void destroy(Node* node) {
        if (node == nullptr) return;
        destroy(node->left);
        destroy(node->right);
        delete node;
    }
};

// ============================================================================
// 内部工具: 计算中位数 (排序后取中间)
// ============================================================================
static double medianOf(std::vector<double> vals) {
    if (vals.empty()) return 0.0;
    std::sort(vals.begin(), vals.end());
    size_t n = vals.size();
    return (n % 2 == 1) ? vals[n / 2] : 0.5 * (vals[n / 2 - 1] + vals[n / 2]);
}

StarMatcher::StarMatcher() {
    LOG_INFO("[star_matcher] 初始化: KD-tree + IRLS/Tukey 清洗 (GAP-013)");
}

// ============================================================================
// KD-tree 最近邻匹配
// 1. WCS 投影所有 Gaia 星到像素坐标
// 2. 对 Gaia 像素坐标建 KD-tree
// 3. 对每颗 PSF 有效星 (status==0), 查询 KD-tree 找最近邻 Gaia 星 (距离 < match_radius_px)
// ============================================================================
std::vector<StarMatch> StarMatcher::matchWithKdTree(
    const WcsTransform& wcs,
    const double* gaia_ra, const double* gaia_dec,
    const double* gaia_mag, const double* gaia_fsyn, int n_gaia,
    const double* psf_cx, const double* psf_cy,
    const double* psf_flux, const int* psf_status, int n_psf,
    double match_radius_px,
    PhotometricDiag* out_diag,
    int frame_width, int frame_height) {

    std::vector<StarMatch> matches;

    // P12-001 阶段3: PSF 总数 (无论后续是否匹配成功)
    if (out_diag) {
        out_diag->psf_total = n_psf;
    }

    if (n_gaia <= 0 || n_psf <= 0) {
        LOG_INFO("[star_matcher] 警告: Gaia星=%d, PSF星=%d, 无匹配", n_gaia, n_psf);
        return matches;
    }

    // 1. WCS 投影所有 Gaia 星到像素坐标
    std::vector<double> gaia_px(n_gaia), gaia_py(n_gaia);
    int n_in_frame = 0;  // P12-001 阶段2: 投影后落在 [0,W) x [0,H) 的 Gaia 星数
    for (int i = 0; i < n_gaia; ++i) {
        wcs.skyToPixel(gaia_ra[i], gaia_dec[i], gaia_px[i], gaia_py[i]);
        if (frame_width > 0 && frame_height > 0) {
            if (gaia_px[i] >= 0.0 && gaia_px[i] < frame_width &&
                gaia_py[i] >= 0.0 && gaia_py[i] < frame_height) {
                ++n_in_frame;
            }
        } else {
            // 无 frame 尺寸, 无法判定, 计为全部
            ++n_in_frame;
        }
    }
    // P12-001 阶段2: 投影统计
    if (out_diag) {
        out_diag->gaia_projected_in_frame = n_in_frame;
        LOG_INFO("[star_matcher] P12-001 阶段2: gaia_projected_in_frame=%d / %d",
                 n_in_frame, n_gaia);
    }

    // 2. 对 Gaia 像素坐标建 KD-tree
    KdTree2D kdtree(gaia_px, gaia_py);

    // 3. 收集 PSF 有效星 (status==0)
    std::vector<int> valid_idx;
    valid_idx.reserve(n_psf);
    for (int i = 0; i < n_psf; ++i) {
        if (psf_status[i] == 0) {
            valid_idx.push_back(i);
        }
    }
    // P12-001 阶段3: PSF 有效星数
    if (out_diag) {
        out_diag->psf_valid = (int)valid_idx.size();
        LOG_INFO("[star_matcher] P12-001 阶段3: psf_total=%d, psf_valid=%d",
                 n_psf, (int)valid_idx.size());
    }
    if (valid_idx.empty()) {
        LOG_INFO("[star_matcher] 警告: 无有效PSF星 (status!=0全部)");
        return matches;
    }
    LOG_INFO("[star_matcher] PSF有效星: %d / %d, Gaia星: %d, 匹配半径: %.2f px",
             (int)valid_idx.size(), n_psf, n_gaia, match_radius_px);

    // 4. 对每颗 PSF 有效星, 查询 KD-tree 找最近邻 Gaia 星
    double max_dist2 = match_radius_px * match_radius_px;
    std::vector<double> match_distances;  // P12-001 阶段8: 记录每对匹配的像素距离
    match_distances.reserve(valid_idx.size());
    for (size_t k = 0; k < valid_idx.size(); ++k) {
        int j = valid_idx[k];
        int best_gaia = kdtree.findNearest(psf_cx[j], psf_cy[j], max_dist2);
        if (best_gaia < 0) {
            // 无匹配 (最近邻超出阈值)
            continue;
        }

        StarMatch m;
        m.x = psf_cx[j];
        m.y = psf_cy[j];
        m.f_instr = psf_flux[j];
        m.f_syn = gaia_fsyn[best_gaia];
        m.gaia_mag = gaia_mag[best_gaia];
        matches.push_back(m);

        // P12-001 阶段8: 记录匹配距离 (PSF 星到 Gaia 星的像素距离)
        double dx = psf_cx[j] - gaia_px[best_gaia];
        double dy = psf_cy[j] - gaia_py[best_gaia];
        match_distances.push_back(std::sqrt(dx * dx + dy * dy));

        // 循环内每颗匹配星的高频日志, 用 LOG_DEBUG 编译时禁用
#ifdef PC_ENABLE_DEBUG
        LOG_DEBUG("[star_matcher] 匹配#%d: PSF[%d] -> Gaia[%d] "
                  "(%.2f,%.2f) dist=%.3f F_syn=%.4e F_instr=%.2f mag_g=%.2f",
                  (int)matches.size(), j, best_gaia, m.x, m.y,
                  match_distances.back(), m.f_syn, m.f_instr, m.gaia_mag);
#endif
    }

    // P12-001 阶段4: 空间候选数 (KD-tree 命中)
    // P12-001 阶段6: rejected_distance = 有效PSF星 - 命中数 (距离超阈值)
    // P12-001 阶段8: 匹配距离统计
    if (out_diag) {
        out_diag->spatial_candidates = (int)matches.size();
        out_diag->unique_matches = (int)matches.size();  // 当前无双向过滤, 等于候选数
        out_diag->rejected_ambiguous = 0;  // 当前无双向匹配, 保持 0
        out_diag->rejected_distance = (int)valid_idx.size() - (int)matches.size();
        LOG_INFO("[star_matcher] P12-001 阶段4/6: spatial_candidates=%d, "
                 "unique_matches=%d, rejected_ambiguous=%d, rejected_distance=%d",
                 out_diag->spatial_candidates, out_diag->unique_matches,
                 out_diag->rejected_ambiguous, out_diag->rejected_distance);

        if (!match_distances.empty()) {
            out_diag->match_distance_median = percentileOf(match_distances, 0.5);
            out_diag->match_distance_p90 = percentileOf(match_distances, 0.9);
            out_diag->match_distance_max =
                *std::max_element(match_distances.begin(), match_distances.end());
            LOG_INFO("[star_matcher] P12-001 阶段8: match_distance "
                     "median=%.4f p90=%.4f max=%.4f px",
                     out_diag->match_distance_median,
                     out_diag->match_distance_p90,
                     out_diag->match_distance_max);
        }
    }

    LOG_INFO("[star_matcher] KD-tree 匹配完成: %d 对", (int)matches.size());
    return matches;
}

// ============================================================================
// 星等一致性预过滤 + IRLS+Tukey 稳健清洗
//   1) delta = -2.5*log10(F_instr) - gaia_mag (粗略零点差)
//   2) median_delta 作为粗略零点, 拒绝 |delta - median_delta| > mag_tolerance
//   3) r = log10(F_instr/F_syn) 上做 IRLS + Tukey biweight
//   4) scale = 10^(-location), sigma_residual = MAD(r_inliers)/0.6745
// ============================================================================
std::vector<StarMatch> StarMatcher::cleanAndScale(
    const std::vector<StarMatch>& matches, double mag_tolerance,
    double* out_scale_factor, double* out_sigma_residual,
    PhotometricDiag* out_diag) {

    int n_in = (int)matches.size();
    LOG_INFO("[star_matcher] cleanAndScale: 输入 %d 颗, 星等容忍 %.2f mag",
             n_in, mag_tolerance);

    if (out_scale_factor) *out_scale_factor = 1.0;
    if (out_sigma_residual) *out_sigma_residual = 0.0;

    if (n_in == 0) {
        return {};
    }

    // ---- 1. 计算有效 r = log10(F_instr/F_syn) 和 delta = -2.5*log10(F_instr) - gaia_mag ----
    std::vector<double> r_vals, delta_vals;
    std::vector<int> valid_idx;  // 在 matches 中的索引
    r_vals.reserve(n_in);
    delta_vals.reserve(n_in);
    valid_idx.reserve(n_in);

    for (int i = 0; i < n_in; ++i) {
        if (matches[i].f_instr <= 0.0 || matches[i].f_syn <= 0.0) continue;
        double r = std::log10(matches[i].f_instr / matches[i].f_syn);
        if (!std::isfinite(r)) continue;
        double mag_inst = -2.5 * std::log10(matches[i].f_instr);
        if (!std::isfinite(mag_inst)) continue;
        double delta = mag_inst - matches[i].gaia_mag;
        if (!std::isfinite(delta)) continue;
        r_vals.push_back(r);
        delta_vals.push_back(delta);
        valid_idx.push_back(i);
    }

    if (r_vals.empty()) {
        LOG_INFO("[star_matcher] 警告: 无有效匹配星 (F_instr/F_syn<=0 或无效值)");
        if (out_diag) {
            out_diag->rejected_quality = n_in;  // 全部因 F<=0/非有限 被拒绝
            out_diag->fit_used = 0;
            out_diag->robust_iterations = 0;
            out_diag->scale_factor = 1.0;
            out_diag->sigma_residual = 0.0;
            LOG_INFO("[star_matcher] P12-001 阶段6/7: rejected_quality=%d (全部无效), fit_used=0",
                     n_in);
        }
        return {};
    }

    // ---- 2. 星等一致性预过滤: |delta - median_delta| > mag_tolerance 拒绝 ----
    double median_delta = medianOf(delta_vals);
    std::vector<int> mag_consistent_idx;
    std::vector<double> r_consistent;
    mag_consistent_idx.reserve(r_vals.size());
    r_consistent.reserve(r_vals.size());
    int n_mag_rejected = 0;
    for (size_t k = 0; k < r_vals.size(); ++k) {
        if (std::fabs(delta_vals[k] - median_delta) <= mag_tolerance) {
            mag_consistent_idx.push_back(valid_idx[k]);
            r_consistent.push_back(r_vals[k]);
        } else {
            n_mag_rejected++;
        }
    }
    LOG_INFO("[star_matcher] 星等一致性: 通过 %d, 拒绝 %d (median_delta=%.4f, tol=%.2f mag)",
             (int)mag_consistent_idx.size(), n_mag_rejected, median_delta, mag_tolerance);

    if (r_consistent.empty()) {
        LOG_INFO("[star_matcher] 警告: 星等一致性过滤后无匹配星");
        if (out_diag) {
            int n_invalid = n_in - (int)valid_idx.size();
            out_diag->rejected_quality = n_invalid + n_mag_rejected;
            out_diag->fit_used = 0;
            out_diag->robust_iterations = 0;
            out_diag->scale_factor = 1.0;
            out_diag->sigma_residual = 0.0;
            LOG_INFO("[star_matcher] P12-001 阶段6/7: rejected_quality=%d (invalid=%d + mag=%d), fit_used=0",
                     out_diag->rejected_quality, n_invalid, n_mag_rejected);
        }
        return {};
    }

    // ---- 3. IRLS + Tukey biweight 稳健位置估计 ----
    // 初始: location = median(r), S = MAD(r)/0.6745
    double location = medianOf(r_consistent);
    std::vector<double> abs_dev;
    abs_dev.reserve(r_consistent.size());
    for (double r : r_consistent) abs_dev.push_back(std::fabs(r - location));
    double mad = medianOf(abs_dev);
    double S = (mad > 0.0) ? (mad / _MAD_SCALE) : 0.0;
    LOG_INFO("[star_matcher] IRLS 初始: location=%.6f, MAD=%.6f, S=%.6f", location, mad, S);

    int irls_iter_count = 0;  // P12-001 阶段7: IRLS 实际迭代次数
    if (S <= 0.0) {
        // 所有 r 相同 (S=0), 无法做 IRLS, 直接用 location 作为最终估计
        LOG_INFO("[star_matcher] IRLS: S=0 (所有 r 相同), 跳过迭代");
    } else {
        // IRLS 迭代
        double prev_location = location;
        for (int iter = 0; iter < _IRLS_MAX_ITER; ++iter) {
            irls_iter_count = iter + 1;  // P12-001: 记录迭代次数
            double sum_wr = 0.0, sum_w = 0.0;
            double cS = _TUKEY_C * S;
            for (double r : r_consistent) {
                double u = (r - location) / cS;
                double aU = std::fabs(u);
                double w;
                if (aU >= 1.0) {
                    w = 0.0;
                } else {
                    double tmp = 1.0 - u * u;
                    w = tmp * tmp;  // Tukey biweight 权重 (1-u^2)^2
                }
                sum_wr += w * r;
                sum_w += w;
            }
            if (sum_w <= 0.0) {
                LOG_INFO("[star_matcher] IRLS 迭代 %d: 所有权重为 0, 终止", iter);
                break;
            }
            double new_location = sum_wr / sum_w;
            double diff = std::fabs(new_location - prev_location);
            location = new_location;
            LOG_DEBUG("[star_matcher] IRLS 迭代 %d: location=%.6f, diff=%.2e", iter, location, diff);
            if (diff < _IRLS_CONVERGE) {
                LOG_INFO("[star_matcher] IRLS 收敛于迭代 %d: location=%.6f", iter, location);
                break;
            }
            prev_location = new_location;
        }
    }

    // ---- 4. 计算 scale = 10^(-location) 和 inliers (Tukey 权重 > 0) ----
    double scale = std::pow(10.0, -location);
    if (out_scale_factor) *out_scale_factor = scale;

    // 收集 inliers (Tukey 权重 > 0)
    std::vector<StarMatch> cleaned;
    cleaned.reserve(mag_consistent_idx.size());
    std::vector<double> r_inliers;
    r_inliers.reserve(mag_consistent_idx.size());
    double cS = (S > 0.0) ? (_TUKEY_C * S) : 1.0;
    int n_irls_outlier = 0;
    for (size_t k = 0; k < r_consistent.size(); ++k) {
        double r = r_consistent[k];
        double u = (S > 0.0) ? (r - location) / cS : 0.0;
        if (S <= 0.0 || std::fabs(u) < 1.0) {
            cleaned.push_back(matches[mag_consistent_idx[k]]);
            r_inliers.push_back(r);
        } else {
            n_irls_outlier++;
        }
    }

    // sigma_residual = MAD(r_inliers)/0.6745 (供 SNR 模块使用)
    double sigma_residual = 0.0;
    if (!r_inliers.empty()) {
        std::vector<double> dev;
        dev.reserve(r_inliers.size());
        for (double r : r_inliers) dev.push_back(std::fabs(r - location));
        double mad_in = medianOf(dev);
        sigma_residual = (mad_in > 0.0) ? (mad_in / _MAD_SCALE) : 0.0;
    }
    if (out_sigma_residual) *out_sigma_residual = sigma_residual;

    // ---- 5. 亮度比例一致性日志 ----
    LOG_INFO("[star_matcher] 亮度比例一致性:");
    LOG_INFO("  - 输入匹配: %d", n_in);
    LOG_INFO("  - 星等一致性通过: %d (拒绝 %d)", (int)mag_consistent_idx.size(), n_mag_rejected);
    LOG_INFO("  - IRLS inliers: %d (outliers: %d)", (int)cleaned.size(), n_irls_outlier);
    LOG_INFO("  - location(r) = %.6f (dex), scale = 10^(-location) = %.6e", location, scale);
    LOG_INFO("  - MAD(r) = %.6f, S = %.6f, sigma_residual = %.6f", mad, S, sigma_residual);
    if (!r_inliers.empty()) {
        double r_min = *std::min_element(r_inliers.begin(), r_inliers.end());
        double r_max = *std::max_element(r_inliers.begin(), r_inliers.end());
        LOG_INFO("  - r_inliers 范围: [%.6f, %.6f] (spread %.6f dex = %.3f mag)",
                 r_min, r_max, r_max - r_min, 2.5 * (r_max - r_min));
    }
    LOG_INFO("[star_matcher] 清洗完成: 保留 %d, 排除 %d (无效/星等不一致/IRLS离群)",
             (int)cleaned.size(), n_in - (int)cleaned.size());

    // P12-001 阶段6/7/8: 填充诊断结构体
    if (out_diag) {
        int n_invalid = n_in - (int)valid_idx.size();  // F<=0/非有限
        out_diag->rejected_quality = n_invalid + n_mag_rejected + n_irls_outlier;
        out_diag->fit_used = (int)cleaned.size();
        out_diag->robust_iterations = irls_iter_count;
        out_diag->scale_factor = scale;
        out_diag->sigma_residual = sigma_residual;
        if (!r_inliers.empty()) {
            out_diag->r_median = percentileOf(r_inliers, 0.5);
            out_diag->r_p90 = percentileOf(r_inliers, 0.9);
            out_diag->r_max = *std::max_element(r_inliers.begin(), r_inliers.end());
        }
        LOG_INFO("[star_matcher] P12-001 阶段6/7/8: rejected_quality=%d "
                 "(invalid=%d + mag=%d + irls=%d), fit_used=%d, "
                 "robust_iterations=%d, scale=%.6e, sigma=%.6f",
                 out_diag->rejected_quality, n_invalid, n_mag_rejected,
                 n_irls_outlier, out_diag->fit_used, out_diag->robust_iterations,
                 out_diag->scale_factor, out_diag->sigma_residual);
        if (!r_inliers.empty()) {
            LOG_INFO("[star_matcher] P12-001 阶段8: r_inliers "
                     "median=%.6f p90=%.6f max=%.6f dex",
                     out_diag->r_median, out_diag->r_p90, out_diag->r_max);
        }
    }

    return cleaned;
}

// ============================================================================
// 一站式: KD-tree 匹配 + IRLS/Tukey 清洗
// ============================================================================
std::vector<StarMatch> StarMatcher::matchAndClean(
    const WcsTransform& wcs,
    const double* gaia_ra, const double* gaia_dec,
    const double* gaia_mag, const double* gaia_fsyn, int n_gaia,
    const double* psf_cx, const double* psf_cy,
    const double* psf_flux, const int* psf_status, int n_psf,
    double match_radius_px, double mag_tolerance,
    double* out_scale_factor, double* out_sigma_residual,
    PhotometricDiag* out_diag,
    int frame_width, int frame_height) {

    // P12-001: 初始化诊断结构体 (全 0)
    initDiag(out_diag);

    std::vector<StarMatch> matches = matchWithKdTree(
        wcs, gaia_ra, gaia_dec, gaia_mag, gaia_fsyn, n_gaia,
        psf_cx, psf_cy, psf_flux, psf_status, n_psf, match_radius_px,
        out_diag, frame_width, frame_height);

    return cleanAndScale(matches, mag_tolerance, out_scale_factor, out_sigma_residual, out_diag);
}

} // namespace pc
