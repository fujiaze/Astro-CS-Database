// gradient_2d_api.cpp - C API 包装实现
// 串联 StarMatcher -> GradientFitter -> Gradient2DImageCorrector 完成单帧 2D 测光校准
// 对应 Python: lib/photometric_calib/archive/estimator.py 的 calibrate() 方法

#include "gradient_2d.h"
#include "star_matcher.h"
#include "gradient_fitter.h"
#include "image_corrector.h"
#include "wcs_transform.h"
#include "log_macros.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

// 构建质量报告 JSON 字符串
std::string build_quality_report(
    int n_matched, int n_excluded,
    int mult_order, int mult_n_used, int mult_n_rejected,
    double mult_loocv, double mult_r2, int mult_skipped,
    double scale, double sigma_residual) {
    char buf[4096];
    std::snprintf(buf, sizeof(buf),
        "{\"n_matched\":%d,\"n_excluded\":%d,"
        "\"mult_order\":%d,\"mult_n_used\":%d,\"mult_n_rejected\":%d,"
        "\"mult_loocv_error\":%.6e,\"mult_r_squared\":%.4f,\"mult_skipped\":%d,"
        "\"scale_factor\":%.6e,\"sigma_residual\":%.6e,"
        "\"additive_frozen\":true,\"sky_calibration_frozen\":true}",
        n_matched, n_excluded,
        mult_order, mult_n_used, mult_n_rejected,
        mult_loocv, mult_r2, mult_skipped,
        scale, sigma_residual);
    return std::string(buf);
}

} // anonymous namespace

// ============================================================================
// gradient_2d_calibrate - 单帧 2D 测光校准 C API
// ============================================================================

extern "C" G2D_API int gradient_2d_calibrate(
    const float* pixels, int width, int height,
    const double* gaia_ra, const double* gaia_dec,
    const double* gaia_mag, const double* gaia_fsyn, int n_gaia,
    const double* psf_cx, const double* psf_cy,
    const double* psf_flux, const int* psf_status, int n_psf,
    double crval1, double crval2, double crpix1, double crpix2,
    double cd11, double cd12, double cd21, double cd22,
    int sip_order,
    const double* sip_a, const double* sip_b,
    const double* sip_ap, const double* sip_bp,
    double match_radius_px, double outlier_sigma,
    int max_order,
    float* out_pixels, Gradient2DResult* result) {

    // ---- 1. 参数校验 ----
    if (!pixels || !out_pixels || !result) {
        LOG_ERROR("gradient_2d_calibrate: null pointer (pixels=%p out=%p result=%p)",
                  pixels, out_pixels, result);
        return -1;
    }
    if (width <= 0 || height <= 0) {
        LOG_ERROR("gradient_2d_calibrate: invalid image size %dx%d", width, height);
        return -1;
    }
    if (n_gaia <= 0 || !gaia_ra || !gaia_dec || !gaia_mag || !gaia_fsyn) {
        LOG_ERROR("gradient_2d_calibrate: invalid gaia arrays (n_gaia=%d)", n_gaia);
        return -1;
    }
    if (n_psf <= 0 || !psf_cx || !psf_cy || !psf_flux || !psf_status) {
        LOG_ERROR("gradient_2d_calibrate: invalid psf arrays (n_psf=%d)", n_psf);
        return -1;
    }

    // 初始化 result 为退化状态
    std::memset(result, 0, sizeof(Gradient2DResult));
    result->scale_factor = 1.0;
    result->additive_zero = 0.0;
    result->mult_order = 1;
    result->mult_skipped = 1;

    LOG_INFO("gradient_2d_calibrate start: img=%dx%d, gaia=%d, psf=%d, match_r=%.2f, sigma=%.2f, max_order=%d",
             width, height, n_gaia, n_psf, match_radius_px, outlier_sigma, max_order);

    // ---- 2. WCS 变换器 ----
    pc::WcsTransform wcs(crval1, crval2, crpix1, crpix2,
                         cd11, cd12, cd21, cd22, sip_order,
                         sip_a, sip_b, sip_ap, sip_bp);

    // ---- 3. 星-图匹配 + MAD 清洗 ----
    pc::StarMatcher matcher;
    double sigma_residual = 0.0;
    auto matches = matcher.matchAndClean(
        wcs, gaia_ra, gaia_dec, gaia_mag, gaia_fsyn, n_gaia,
        psf_cx, psf_cy, psf_flux, psf_status, n_psf,
        match_radius_px, outlier_sigma, &sigma_residual);

    int n_matched = (int)matches.size();
    // n_excluded = 暴力匹配数 - 清洗后数 (matcher 内部不暴露 n_excluded, 估算为 0)
    // 注: StarMatcher::matchAndClean 内部已做清洗, n_excluded 通过
    //     (n_raw_matches - n_matched) 计算, 但 matchAndClean 不返回 n_raw。
    //     这里以 sigma_residual 为质量指标, n_excluded 置 0 (保守值)
    int n_excluded = 0;
    result->sigma_residual = sigma_residual;
    LOG_INFO("match: n_matched=%d, sigma_residual=%.6e", n_matched, sigma_residual);

    if (n_matched < pc::GradientFitter::MIN_MATCHED_FOR_FIT) {
        LOG_INFO("n_matched=%d < %d, degenerate path (scale=1.0, identity surface)",
                 n_matched, pc::GradientFitter::MIN_MATCHED_FOR_FIT);
        // 恒等校正: 直接复制原图
        std::memcpy(out_pixels, pixels, sizeof(float) * width * height);
        std::string qr = build_quality_report(
            n_matched, n_excluded, 1, 0, 0,
            std::numeric_limits<double>::infinity(), 0.0, 1, 1.0, sigma_residual);
        std::strncpy(result->quality_report, qr.c_str(), sizeof(result->quality_report) - 1);
        result->quality_report[sizeof(result->quality_report) - 1] = '\0';
        return -2;
    }

    // ---- 4. 提取数组 + 过滤无效 ----
    std::vector<double> x_arr, y_arr, f_instr, f_syn, r_arr;
    x_arr.reserve(n_matched);
    y_arr.reserve(n_matched);
    f_instr.reserve(n_matched);
    f_syn.reserve(n_matched);
    r_arr.reserve(n_matched);
    int n_invalid = 0;
    for (const auto& m : matches) {
        if (m.f_instr <= 0.0 || m.f_syn <= 0.0) {
            ++n_invalid;
            continue;
        }
        x_arr.push_back(m.x);
        y_arr.push_back(m.y);
        f_instr.push_back(m.f_instr);
        f_syn.push_back(m.f_syn);
        r_arr.push_back(std::log10(m.f_instr / m.f_syn));
    }
    if (n_invalid > 0) {
        LOG_INFO("filtered %d invalid matches (F_instr<=0 or F_syn<=0)", n_invalid);
    }
    if (x_arr.size() < pc::GradientFitter::MIN_MATCHED_FOR_FIT) {
        LOG_INFO("after filter: n=%zu < %d, degenerate path",
                 x_arr.size(), pc::GradientFitter::MIN_MATCHED_FOR_FIT);
        std::memcpy(out_pixels, pixels, sizeof(float) * width * height);
        std::string qr = build_quality_report(
            n_matched, n_excluded, 1, 0, 0,
            std::numeric_limits<double>::infinity(), 0.0, 1, 1.0, sigma_residual);
        std::strncpy(result->quality_report, qr.c_str(), sizeof(result->quality_report) - 1);
        result->quality_report[sizeof(result->quality_report) - 1] = '\0';
        return -2;
    }

    // ---- 5. 乘性梯度曲面拟合 ----
    pc::GradientFitter fitter;
    pc::GradientSurface mult_surface = fitter.fit_multiplicative(
        x_arr, y_arr, r_arr, (double)width, (double)height, max_order);
    LOG_INFO("mult_surface: order=%d, alpha=%.2f, cv=%.6e, used=%d, rejected=%d",
             mult_surface.order, mult_surface.ridge_alpha, mult_surface.loocv_error,
             mult_surface.n_used, mult_surface.n_rejected);

    // ---- 6. R² 信号检测: < 0.02 则跳过乘性校正 ----
    double mult_r2 = fitter.compute_r_squared(
        mult_surface, x_arr, y_arr, r_arr, (double)width, (double)height);
    int mult_skipped = 0;
    if (mult_r2 < pc::GradientFitter::MIN_R_SQUARED) {
        LOG_INFO("mult R²=%.4f < %.2f, skip multiplicative correction (M_map=1.0)",
                 mult_r2, pc::GradientFitter::MIN_R_SQUARED);
        mult_surface = pc::GradientFitter::identity_surface();
        mult_skipped = 1;
    }

    // ---- 7. 图像校正 + 通量归一化 ----
    double scale = pc::Gradient2DImageCorrector::correct_and_normalize(
        pixels, width, height, mult_surface,
        x_arr, y_arr, f_syn, f_instr, out_pixels);
    if (!std::isfinite(scale) || scale <= 0.0) {
        LOG_ERROR("scale invalid (%.6e), fallback to 1.0", scale);
        scale = 1.0;
        std::memcpy(out_pixels, pixels, sizeof(float) * width * height);
    }

    // ---- 8. 残差 RMS (内点) ----
    double rms = 0.0;
    int n_used = 0;
    for (size_t i = 0; i < x_arr.size(); ++i) {
        // 计算拟合残差: r_obs - r_fit
        double sx = (width > 0) ? (2.0 / width) : 0.0;
        double sy = (height > 0) ? (2.0 / height) : 0.0;
        double x_norm = x_arr[i] * sx - 1.0;
        double y_norm = y_arr[i] * sy - 1.0;
        double r_fit = pc::GradientFitter::eval_surface(mult_surface, x_norm, y_norm);
        double res = r_arr[i] - r_fit;
        rms += res * res;
        ++n_used;
    }
    if (n_used > 0) rms = std::sqrt(rms / n_used);

    // ---- 9. 填充 result ----
    result->scale_factor = scale;
    result->additive_zero = 0.0;  // 已封存
    result->n_matched = n_matched;
    result->n_excluded = n_excluded;
    result->rms_residual = rms;
    result->mult_order = mult_surface.order;
    result->mult_n_used = mult_surface.n_used;
    result->mult_n_rejected = mult_surface.n_rejected;
    result->mult_loocv_error = mult_surface.loocv_error;
    result->mult_r_squared = mult_r2;
    result->mult_skipped = mult_skipped;
    result->sigma_residual = sigma_residual;

    std::string qr = build_quality_report(
        n_matched, n_excluded,
        mult_surface.order, mult_surface.n_used, mult_surface.n_rejected,
        mult_surface.loocv_error, mult_r2, mult_skipped,
        scale, sigma_residual);
    std::strncpy(result->quality_report, qr.c_str(), sizeof(result->quality_report) - 1);
    result->quality_report[sizeof(result->quality_report) - 1] = '\0';

    LOG_INFO("gradient_2d_calibrate done: scale=%.6e, n_matched=%d, mult_order=%d, "
             "R²=%.4f, skipped=%d, rms=%.4e, sigma_resid=%.4e",
             scale, n_matched, mult_surface.order, mult_r2, mult_skipped, rms, sigma_residual);
    return 0;
}
