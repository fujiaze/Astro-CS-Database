#ifndef PHOTOMETRIC_CALIB_H
#define PHOTOMETRIC_CALIB_H

#include <cstdint>

#ifdef _WIN32
#define PC_API __declspec(dllexport)
#else
#define PC_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 简化版测光校准 C API
//
// 功能: WCS投影Gaia星 -> KDTree匹配PSF星 -> MAD离群清洗 -> 全局scale校正
// 算法:
//   1. WCS投影Gaia星到像素坐标 (TAN+SIP投影)
//   2. 暴力最近邻匹配PSF星和Gaia星 (距离<3px)
//   3. MAD离群清洗: r=log10(F_instr/F_syn), sigma=3.0
//   4. scale = median(F_syn/F_instr)
//   5. I_cal = I * scale
//
// 输入参数:
//   pixels        - 图像像素 float32 [H*W] (行优先)
//   width,height  - 图像尺寸
//   gaia_ra/dec   - Gaia星RA/Dec数组 [n_gaia] (度)
//   gaia_mag      - Gaia星magnitude数组 [n_gaia]
//   gaia_fsyn     - Gaia星合成流量数组 [n_gaia]
//   n_gaia        - Gaia星数量
//   psf_cx/cy     - PSF星x/y坐标数组 [n_psf] (0-based)
//   psf_flux      - PSF星flux数组 [n_psf]
//   psf_status    - PSF星状态数组 [n_psf] (0=成功)
//   n_psf         - PSF星数量
//   WCS参数: crval1/2, crpix1/2, cd11/12/21/22
//   SIP参数: sip_order, sip_a/b/ap/bp数组 (可为nullptr, 长度36)
//
// 输出参数:
//   out_pixels      - 校正后像素 (调用者分配, float32 [H*W])
//   out_n_matched   - 匹配星数 (MAD清洗后)
//   out_scale_factor- scale因子
//
// 返回: 0=成功, <0=失败
// ============================================================================
PC_API int pc_calibrate_simple(
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
    float* out_pixels, int* out_n_matched, double* out_scale_factor);

#ifdef __cplusplus
}
#endif

#endif // PHOTOMETRIC_CALIB_H
