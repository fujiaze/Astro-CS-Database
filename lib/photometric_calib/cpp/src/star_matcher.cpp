// star_matcher.cpp - 星-图匹配器
// 功能: 将Gaia参考星与图像PSF拟合星进行空间匹配, MAD离群清洗
// 算法: 暴力最近邻搜索(Gaia星数量通常<10000) + MAD稳健sigma裁剪
// 参考: lib/photometric_calib/flux_calibrator/python/star_matcher.py

#include "star_matcher.h"
#include "log_macros.h"

#include <cstdio>
#include <cmath>
#include <algorithm>

namespace pc {

static constexpr double _MAD_SCALE = 0.6745;

StarMatcher::StarMatcher() {
    std::fprintf(stderr, "[star_matcher] 初始化: 暴力最近邻 + MAD清洗\n");
}

// ============================================================================
// 暴力最近邻匹配
// 对每颗Gaia星: WCS投影到像素坐标, 在PSF有效星中找最近邻
// ============================================================================
std::vector<StarMatch> StarMatcher::matchBruteForce(
    const WcsTransform& wcs,
    const double* gaia_ra, const double* gaia_dec,
    const double* gaia_mag, const double* gaia_fsyn, int n_gaia,
    const double* psf_cx, const double* psf_cy,
    const double* psf_flux, const int* psf_status, int n_psf,
    double match_radius_px) {

    std::vector<StarMatch> matches;

    if (n_gaia <= 0 || n_psf <= 0) {
        std::fprintf(stderr, "[star_matcher] 警告: Gaia星=%d, PSF星=%d, 无匹配\n",
                    n_gaia, n_psf);
        return matches;
    }

    // 收集PSF有效星 (status==0)
    std::vector<int> valid_idx;
    valid_idx.reserve(n_psf);
    for (int i = 0; i < n_psf; ++i) {
        if (psf_status[i] == 0) {
            valid_idx.push_back(i);
        }
    }
    if (valid_idx.empty()) {
        std::fprintf(stderr, "[star_matcher] 警告: 无有效PSF星 (status!=0全部)\n");
        return matches;
    }
    std::fprintf(stderr, "[star_matcher] PSF有效星: %d / %d\n",
                (int)valid_idx.size(), n_psf);

    // 对每颗Gaia星: WCS投影 + 暴力最近邻
    for (int i = 0; i < n_gaia; ++i) {
        double gx, gy;
        wcs.skyToPixel(gaia_ra[i], gaia_dec[i], gx, gy);

        // 暴力搜索最近邻PSF有效星
        double best_dist2 = match_radius_px * match_radius_px;
        int best_psf = -1;
        for (size_t k = 0; k < valid_idx.size(); ++k) {
            int j = valid_idx[k];
            double ddx = psf_cx[j] - gx;
            double ddy = psf_cy[j] - gy;
            double dist2 = ddx * ddx + ddy * ddy;
            if (dist2 < best_dist2) {
                best_dist2 = dist2;
                best_psf = j;
            }
        }

        if (best_psf < 0) {
            // 无匹配 (最近邻超出阈值)
            continue;
        }

        StarMatch m;
        m.x = psf_cx[best_psf];
        m.y = psf_cy[best_psf];
        m.f_instr = psf_flux[best_psf];
        m.f_syn = gaia_fsyn[i];
        matches.push_back(m);

        // 循环内每颗匹配星的高频日志, 用 LOG_DEBUG 编译时禁用
        LOG_DEBUG("[star_matcher] 匹配#%d: Gaia[%d] -> PSF[%d] "
                  "(%.2f,%.2f) dist=%.3f F_syn=%.4e F_instr=%.2f",
                  (int)matches.size(), i, best_psf, m.x, m.y,
                  std::sqrt(best_dist2), m.f_syn, m.f_instr);
    }

    std::fprintf(stderr, "[star_matcher] 匹配完成: %d 对\n", (int)matches.size());
    return matches;
}

// ============================================================================
// MAD离群清洗
// r = log10(F_instr / F_syn)
// 排除 F_instr<=0 或 F_syn<=0
// median = median(r), MAD = median(|r - median|)
// sigma = MAD / 0.6745
// 剔除 |r - median| > outlier_sigma * sigma
// ============================================================================
std::vector<StarMatch> StarMatcher::cleanOutliers(
    const std::vector<StarMatch>& matches, double outlier_sigma) {

    int n_in = (int)matches.size();
    std::fprintf(stderr, "[star_matcher] clean_outliers: 输入 %d 颗, sigma阈值 %.2f\n",
                n_in, outlier_sigma);
    if (n_in == 0) {
        return {};
    }

    // 计算r = log10(F_instr/F_syn), 排除无效值
    std::vector<double> r_vals;
    r_vals.reserve(n_in);
    std::vector<bool> valid(n_in, false);
    for (int i = 0; i < n_in; ++i) {
        if (matches[i].f_instr > 0.0 && matches[i].f_syn > 0.0) {
            double r = std::log10(matches[i].f_instr / matches[i].f_syn);
            if (std::isfinite(r)) {
                r_vals.push_back(r);
                valid[i] = true;
            }
        }
    }

    if (r_vals.empty()) {
        std::fprintf(stderr, "[star_matcher] 警告: 无有效匹配星(F_instr/F_syn<=0), 全部排除\n");
        return {};
    }

    // 中位数
    std::vector<double> r_sorted = r_vals;
    std::sort(r_sorted.begin(), r_sorted.end());
    double med = r_sorted[r_sorted.size() / 2];

    // MAD = median(|r - median|)
    std::vector<double> abs_dev;
    abs_dev.reserve(r_vals.size());
    for (double r : r_vals) {
        abs_dev.push_back(std::fabs(r - med));
    }
    std::sort(abs_dev.begin(), abs_dev.end());
    double mad = abs_dev[abs_dev.size() / 2];
    double sigma = (mad > 0.0) ? (mad / _MAD_SCALE) : 0.0;

    std::fprintf(stderr, "[star_matcher] r中位数=%.6f, MAD=%.6f, sigma=%.6f\n",
                med, mad, sigma);

    // 清洗: 保留有效且未离群的星
    std::vector<StarMatch> cleaned;
    cleaned.reserve(n_in);
    int n_outlier = 0;
    int n_invalid = 0;
    for (int i = 0; i < n_in; ++i) {
        if (!valid[i]) {
            n_invalid++;
            continue;
        }
        double r = std::log10(matches[i].f_instr / matches[i].f_syn);
        if (sigma > 0.0 && std::fabs(r - med) > outlier_sigma * sigma) {
            n_outlier++;
            continue;
        }
        cleaned.push_back(matches[i]);
    }

    std::fprintf(stderr, "[star_matcher] 清洗完成: 保留 %d, 排除 %d (无效 %d, 离群 %d)\n",
                (int)cleaned.size(), n_in - (int)cleaned.size(), n_invalid, n_outlier);
    return cleaned;
}

// ============================================================================
// 一站式: 匹配 + 清洗
// ============================================================================
std::vector<StarMatch> StarMatcher::matchAndClean(
    const WcsTransform& wcs,
    const double* gaia_ra, const double* gaia_dec,
    const double* gaia_mag, const double* gaia_fsyn, int n_gaia,
    const double* psf_cx, const double* psf_cy,
    const double* psf_flux, const int* psf_status, int n_psf,
    double match_radius_px, double outlier_sigma) {

    std::vector<StarMatch> matches = matchBruteForce(
        wcs, gaia_ra, gaia_dec, gaia_mag, gaia_fsyn, n_gaia,
        psf_cx, psf_cy, psf_flux, psf_status, n_psf, match_radius_px);

    return cleanOutliers(matches, outlier_sigma);
}

} // namespace pc
