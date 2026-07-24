// pc_api.cpp - C API包装层
// 功能: 将C++测光校准流程包装为C接口, 供Python ctypes调用
// 流程: WCS投影 -> 星匹配 -> MAD清洗 -> scale计算 -> 图像校正

#include "../include/photometric_calib.h"
#include "../include/log_macros.h"
#include "wcs_transform.h"
#include "star_matcher.h"
#include "image_corrector.h"
#include "spectrum_integrator.h"

// gaia_client C API (跨 DLL 调用)
extern "C" {
#include "gaia_client.h"
#include <cstdlib> // free, 用于释放 gaia_client 返回的 malloc 内存
}

#include <cmath>
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
    const double* qe_wl, const double* qe_trans, int qe_count,
    double crval1, double crval2, double crpix1, double crpix2,
    double cd11, double cd12, double cd21, double cd22,
    int sip_order,
    const double* sip_a, const double* sip_b,
    const double* sip_ap, const double* sip_bp,
    float* out_pixels, int* out_n_matched, double* out_scale_factor,
    double* out_sigma_residual) {

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
        if (out_sigma_residual) *out_sigma_residual = 0.0;
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
        if (out_sigma_residual) *out_sigma_residual = 0.0;
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < width * height; ++i) {
            out_pixels[i] = pixels[i];
        }
        std::fprintf(stderr, "[pc_api] 退化: scale=1.0, 无校正\n");
        return 0;
    }

    // 注: pc_calibrate_simple 不在 DLL 内部计算 F_syn (gaia_fsyn 由调用方外部传入)
    //     QE 参数仅为 API 一致性保留, 此处不做处理. 若需用 QE 计算 F_syn, 请使用
    //     pc_calibrate_simple_with_gaia 接口.
    (void)qe_wl; (void)qe_trans; (void)qe_count;

    std::fprintf(stderr, "[pc_api] 图像: %dx%d, Gaia星: %d, PSF星: %d, SIP阶数: %d\n",
                width, height, n_gaia, n_psf, sip_order);

    // ---- 1. 构造WCS转换器 ----
    pc::WcsTransform wcs(crval1, crval2, crpix1, crpix2,
                         cd11, cd12, cd21, cd22,
                         sip_order, sip_a, sip_b, sip_ap, sip_bp);

    // ---- 2. 星-图匹配 + IRLS/Tukey 清洗 (GAP-013: 输出 scale + sigma_residual) ----
    pc::StarMatcher matcher;
    double scale = 1.0;
    double sigma_residual = 0.0;
    std::vector<pc::StarMatch> matches = matcher.matchAndClean(
        wcs, gaia_ra, gaia_dec, gaia_mag, gaia_fsyn, n_gaia,
        psf_cx, psf_cy, psf_flux, psf_status, n_psf,
        2.0,  // match_radius_px (GAP-013: 收紧 3.0 -> 2.0)
        3.0,  // mag_tolerance (GAP-013: 星等一致性容忍度, mag)
        &scale,
        &sigma_residual);

    int n_matched = (int)matches.size();
    std::fprintf(stderr, "[pc_api] 匹配+清洗完成: %d 颗, scale=%.6e, sigma_residual=%.6f\n",
                n_matched, scale, sigma_residual);

    // ---- 3. 图像校正: I_cal = I * scale ----
    pc::ImageCorrector::correctImage(pixels, width, height, scale, out_pixels);

    // ---- 输出 ----
    *out_n_matched = n_matched;
    *out_scale_factor = scale;
    if (out_sigma_residual) *out_sigma_residual = sigma_residual;

    std::fprintf(stderr, "[pc_api] ====== 测光校准完成: n_matched=%d, scale=%.6e, sigma_residual=%.6f ======\n",
                n_matched, scale, sigma_residual);
    return 0;
}

// ============================================================================
// pc_calibrate_simple_with_gaia: 扩展接口
//   DLL 内部调用 gaia_client 锥形搜索 DR3SP 光谱 -> OpenMP 并行积分 F_syn
//   -> WCS 投影 + 星匹配 + MAD 清洗 + scale 计算 + 图像校正
//
// 返回: 0=成功, <0=失败
//   -1: 空指针/参数无效
//   -2: gaia_client_handle 为空
//   -3: 锥形搜索失败或无光谱星
// ============================================================================
int pc_calibrate_simple_with_gaia(
    void* gaia_client_handle,
    double ra_center, double dec_center, double radius_deg,
    double mag_min, double mag_max,
    const double* filter_wl, const double* filter_trans, int filter_count,
    const double* qe_wl, const double* qe_trans, int qe_count,
    const double* spectrum_wl, int spectrum_count,
    const float* pixels, int width, int height,
    const double* psf_cx, const double* psf_cy,
    const double* psf_flux, const int* psf_status, int n_psf,
    double crval1, double crval2, double crpix1, double crpix2,
    double cd11, double cd12, double cd21, double cd22,
    int sip_order,
    const double* sip_a, const double* sip_b,
    const double* sip_ap, const double* sip_bp,
    float* out_pixels, int* out_n_matched, double* out_scale_factor,
    double* out_sigma_residual) {

    std::fprintf(stderr, "[pc_api] ====== pc_calibrate_simple_with_gaia 开始 ======\n");
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
        return -1;
    }
    if (gaia_client_handle == nullptr) {
        std::fprintf(stderr, "[pc_api] 错误: gaia_client_handle 为空\n");
        return -2;
    }
    if (filter_wl == nullptr || filter_trans == nullptr || filter_count <= 0) {
        std::fprintf(stderr, "[pc_api] 错误: 滤光片参数无效\n");
        return -1;
    }
    if (spectrum_wl == nullptr || spectrum_count <= 0) {
        std::fprintf(stderr, "[pc_api] 错误: 光谱波长参数无效\n");
        return -1;
    }

    GaiaClient* client = static_cast<GaiaClient*>(gaia_client_handle);

    // 退化情形: 无 PSF 星 -> 恒等校正
    if (n_psf <= 0 || psf_cx == nullptr || psf_cy == nullptr ||
        psf_flux == nullptr || psf_status == nullptr) {
        std::fprintf(stderr, "[pc_api] 退化: 无PSF星, scale=1.0\n");
        *out_scale_factor = 1.0;
        *out_n_matched = 0;
        if (out_sigma_residual) *out_sigma_residual = 0.0;
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < width * height; ++i) {
            out_pixels[i] = pixels[i];
        }
        return 0;
    }

    LOG_INFO("锥形搜索: ra=%.4f dec=%.4f r=%.3f mag_min=%.1f (自适应迭代 mag_max, 不缩小半径)",
             ra_center, dec_center, radius_deg, mag_min);

    // ---- 1. gaia_client 锥形搜索带光谱 (自适应迭代星等) ----
    // 从 mag_max=12.0 开始, 若返回星数 < 2000 则增大 mag_max (13/14/15/16),
    // 直到星数在 2000-10000 范围或达 16.0 上限. 不缩小 radius_deg, mag_min 保持外部传入值.
    static const double mag_max_arr[] = {12.0, 13.0, 14.0, 15.0, 16.0};
    GaiaSpectrumStar* spec_stars = nullptr;
    uint8_t* spectra_buf = nullptr;
    int n_gaia = 0;
    int rc = 0;

    for (int i = 0; i < 5; ++i) {
        double mag_max_try = mag_max_arr[i];
        LOG_INFO("自适应星等查询: mag_max=%.1f", mag_max_try);

        rc = gaia_client_cone_search_with_spectrum(
            client, ra_center, dec_center, radius_deg, mag_min, mag_max_try,
            &spec_stars, &spectra_buf, &n_gaia);

        if (rc != 0) {
            LOG_ERROR("锥形搜索失败 rc=%d, mag_max=%.1f", rc, mag_max_try);
            if (spec_stars) { free(spec_stars); spec_stars = nullptr; }
            if (spectra_buf) { free(spectra_buf); spectra_buf = nullptr; }
            return -3;
        }

        LOG_INFO("自适应星等查询: mag_max=%.1f, n_gaia=%d", mag_max_try, n_gaia);

        // 停止条件: 星数 >= 2000 (含 > 10000 情况, 无需更小 mag_max) 或已达 16.0 上限
        if (n_gaia >= 2000 || i == 4) {
            LOG_INFO("自适应星等查询完成: mag_max=%.1f, n_gaia=%d", mag_max_try, n_gaia);
            break;
        }

        // 释放本次失败迭代的内存, 准备下一次 (成功迭代的结果由后续代码使用, 不释放)
        if (spec_stars) { free(spec_stars); spec_stars = nullptr; }
        if (spectra_buf) { free(spectra_buf); spectra_buf = nullptr; }
    }

    if (n_gaia <= 0 || spec_stars == nullptr) {
        LOG_INFO("无光谱星 (n_gaia=%d), 退化: scale=1.0", n_gaia);
        *out_scale_factor = 1.0;
        *out_n_matched = 0;
        if (out_sigma_residual) *out_sigma_residual = 0.0;
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < width * height; ++i) {
            out_pixels[i] = pixels[i];
        }
        if (spec_stars) { free(spec_stars); spec_stars = nullptr; }
        if (spectra_buf) { free(spectra_buf); spectra_buf = nullptr; }
        return 0;
    }

    // 获取光谱点数 (用于定位第 i 颗星光谱在 spectra_buf 中的偏移)
    int wl_start = 0, wl_step = 0, spec_count_from_db = 0;
    gaia_client_get_spectrum_params(client, &wl_start, &wl_step, &spec_count_from_db);
    int spec_stride = spec_count_from_db > 0 ? spec_count_from_db : spectrum_count;
    if (spec_count_from_db != spectrum_count) {
        std::fprintf(stderr, "[pc_api] 警告: DB光谱点数 %d != 传入 spectrum_count %d, 使用DB值\n",
                    spec_count_from_db, spectrum_count);
    }
    if (spec_count_from_db <= 0) {
        spec_count_from_db = spectrum_count; // 退化为传入值
        spec_stride = spectrum_count;
    }
    std::fprintf(stderr, "[pc_api] 搜索到 %d 颗光谱星, 光谱点数=%d (wl_start=%d, wl_step=%d)\n",
                n_gaia, spec_stride, wl_start, wl_step);

    // ---- 2. 预处理滤光片曲线 + QE 曲线 (GAP-012: F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ) ----
    // 排序 + Akima 插值到光谱网格, 缓存 weighted_wl = λ × T(λ) × Q(λ)
    // qe_wl 可为 nullptr (向后兼容, 此时 Q(λ)=1.0)
    photo_calib::SpectrumIntegratorCache filter_cache = photo_calib::prepare_filter_cache(
        filter_wl, filter_trans, filter_count,
        qe_wl, qe_trans, qe_count,
        spectrum_wl, spectrum_count);
    if (filter_cache.spectrum_wl.empty()) {
        LOG_ERROR("滤光片/QE 预处理失败, 退化: scale=1.0");
        *out_scale_factor = 1.0;
        *out_n_matched = 0;
        if (out_sigma_residual) *out_sigma_residual = 0.0;
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < width * height; ++i) {
            out_pixels[i] = pixels[i];
        }
        free(spec_stars);
        free(spectra_buf);
        return 0;
    }
    LOG_INFO("滤光片/QE 预处理完成: filter=%d 点, qe=%d 点 -> 光谱 %d 点",
             filter_count, qe_count, spectrum_count);

    // ---- 3. OpenMP 16 线程并行计算 F_syn (复用缓存) ----
    std::vector<double> f_syn_values(n_gaia, 0.0);
    int n_valid_fsyn = 0;

    #pragma omp parallel for num_threads(16) schedule(dynamic, 64) reduction(+:n_valid_fsyn)
    for (int i = 0; i < n_gaia; ++i) {
        const uint8_t* spec_i = spectra_buf + (size_t)i * spec_stride;
        f_syn_values[i] = photo_calib::compute_f_syn_cached(
            filter_cache, spec_i, spec_stride, spec_stars[i].magG);
        if (f_syn_values[i] > 0.0 && std::isfinite(f_syn_values[i])) {
            ++n_valid_fsyn;
        }
    }
    std::fprintf(stderr, "[pc_api] F_syn 并行计算完成: %d/%d 颗有效\n", n_valid_fsyn, n_gaia);

    // ---- 4. 构造 Gaia ra/dec/mag/fsyn 数组, 调用现有 StarMatcher + ImageCorrector ----
    std::vector<double> gaia_ra(n_gaia), gaia_dec(n_gaia), gaia_mag(n_gaia), gaia_fsyn(n_gaia);
    for (int i = 0; i < n_gaia; ++i) {
        gaia_ra[i]   = spec_stars[i].ra;
        gaia_dec[i]  = spec_stars[i].dec;
        gaia_mag[i]  = spec_stars[i].magG;
        gaia_fsyn[i] = f_syn_values[i];
    }

    // ---- 5. 构造 WCS 转换器 ----
    pc::WcsTransform wcs(crval1, crval2, crpix1, crpix2,
                         cd11, cd12, cd21, cd22,
                         sip_order, sip_a, sip_b, sip_ap, sip_bp);

    // ---- 6. 星-图匹配 + IRLS/Tukey 清洗 (GAP-013: 输出 scale + sigma_residual) ----
    pc::StarMatcher matcher;
    double scale = 1.0;
    double sigma_residual = 0.0;
    std::vector<pc::StarMatch> matches = matcher.matchAndClean(
        wcs,
        gaia_ra.data(), gaia_dec.data(), gaia_mag.data(), gaia_fsyn.data(), n_gaia,
        psf_cx, psf_cy, psf_flux, psf_status, n_psf,
        2.0,   // match_radius_px (GAP-013: 收紧 3.0 -> 2.0)
        3.0,   // mag_tolerance (GAP-013: 星等一致性容忍度, mag)
        &scale,
        &sigma_residual);

    int n_matched = (int)matches.size();
    std::fprintf(stderr, "[pc_api] 匹配+清洗完成: %d 颗, scale=%.6e, sigma_residual=%.6f\n",
                n_matched, scale, sigma_residual);

    // ---- 7. 图像校正: I_cal = I * scale (scale 来自 IRLS 稳健估计) ----
    pc::ImageCorrector::correctImage(pixels, width, height, scale, out_pixels);

    *out_n_matched = n_matched;
    *out_scale_factor = scale;
    if (out_sigma_residual) *out_sigma_residual = sigma_residual;

    // ---- 8. 释放 gaia_client 返回的内存 ----
    // 注: MinGW 下两 DLL 共用 msvcrt, free 跨边界安全
    free(spec_stars);
    free(spectra_buf);

    std::fprintf(stderr, "[pc_api] ====== pc_calibrate_simple_with_gaia 完成: n_matched=%d, scale=%.6e, sigma_residual=%.6f ======\n",
                n_matched, scale, sigma_residual);
    return 0;
}
