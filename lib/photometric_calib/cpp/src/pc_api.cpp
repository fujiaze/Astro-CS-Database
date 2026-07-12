// pc_api.cpp - C API包装层
// 功能: 将C++测光校准流程包装为C接口, 供Python ctypes调用
// 流程: WCS投影 -> 星匹配 -> MAD清洗 -> scale计算 -> 图像校正

#include "../include/photometric_calib.h"
#include "wcs_transform.h"
#include "star_matcher.h"
#include "image_corrector.h"

#include <cstdio>
#include <cstring>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

// ============================================================================
// pc_calibrate_simple: 简化版测光校准主入口
//
// 返回: 0=成功, <0=失败
//   -1: 空指针参数
//   -2: 尺寸无效
//   -3: 无Gaia星或无PSF星
// ============================================================================
int pc_calibrate_simple(
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
    float* out_pixels, int* out_n_matched, double* out_scale_factor) {

    std::fprintf(stderr, "[pc_api] ====== 简化版测光校准开始 ======\n");
#ifdef _OPENMP
    std::fprintf(stderr, "[pc_api] OpenMP线程数: %d\n", omp_get_max_threads());
#endif

    // ---- 参数校验 ----
    if (pixels == nullptr || out_pixels == nullptr ||
        out_n_matched == nullptr || out_scale_factor == nullptr) {
        std::fprintf(stderr, "[pc_api] 错误: 输出指针为空\n");
        return -1;
    }
    if (width <= 0 || height <= 0) {
        std::fprintf(stderr, "[pc_api] 错误: 图像尺寸无效 (%dx%d)\n", width, height);
        return -2;
    }
    if (n_gaia <= 0 || gaia_ra == nullptr || gaia_dec == nullptr ||
        gaia_mag == nullptr || gaia_fsyn == nullptr) {
        std::fprintf(stderr, "[pc_api] 错误: 无Gaia星 (n_gaia=%d)\n", n_gaia);
        // 仍然做恒等校正 (scale=1.0)
        *out_scale_factor = 1.0;
        *out_n_matched = 0;
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < width * height; ++i) {
            out_pixels[i] = pixels[i];
        }
        std::fprintf(stderr, "[pc_api] 退化: scale=1.0, 无校正\n");
        return 0;
    }
    if (n_psf <= 0 || psf_cx == nullptr || psf_cy == nullptr ||
        psf_flux == nullptr || psf_status == nullptr) {
        std::fprintf(stderr, "[pc_api] 错误: 无PSF星 (n_psf=%d)\n", n_psf);
        *out_scale_factor = 1.0;
        *out_n_matched = 0;
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < width * height; ++i) {
            out_pixels[i] = pixels[i];
        }
        std::fprintf(stderr, "[pc_api] 退化: scale=1.0, 无校正\n");
        return 0;
    }

    std::fprintf(stderr, "[pc_api] 图像: %dx%d, Gaia星: %d, PSF星: %d, SIP阶数: %d\n",
                width, height, n_gaia, n_psf, sip_order);

    // ---- 1. 构造WCS转换器 ----
    pc::WcsTransform wcs(crval1, crval2, crpix1, crpix2,
                         cd11, cd12, cd21, cd22,
                         sip_order, sip_a, sip_b, sip_ap, sip_bp);

    // ---- 2. 星-图匹配 + MAD清洗 ----
    pc::StarMatcher matcher;
    std::vector<pc::StarMatch> matches = matcher.matchAndClean(
        wcs, gaia_ra, gaia_dec, gaia_mag, gaia_fsyn, n_gaia,
        psf_cx, psf_cy, psf_flux, psf_status, n_psf,
        3.0,  // match_radius_px
        3.0); // outlier_sigma

    int n_matched = (int)matches.size();
    std::fprintf(stderr, "[pc_api] 匹配+清洗完成: %d 颗\n", n_matched);

    // ---- 3. 计算scale因子 ----
    double scale = pc::ImageCorrector::computeScale(matches);

    // ---- 4. 图像校正: I_cal = I * scale ----
    pc::ImageCorrector::correctImage(pixels, width, height, scale, out_pixels);

    // ---- 输出 ----
    *out_n_matched = n_matched;
    *out_scale_factor = scale;

    std::fprintf(stderr, "[pc_api] ====== 测光校准完成: n_matched=%d, scale=%.6e ======\n",
                n_matched, scale);
    return 0;
}
