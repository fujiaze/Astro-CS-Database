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
//   out_sigma_residual - sigma_residual = MAD(log10(F_instr/F_syn))/0.6745
//                        (可为 nullptr, 向后兼容; 供 SNR 模块 §14 计算 SNR_phot)
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
    float* out_pixels, int* out_n_matched, double* out_scale_factor,
    double* out_sigma_residual);

// ============================================================================
// 扩展接口: 接受 gaia_client handle, DLL 内部查询 DR3SP 光谱并积分得 F_syn
//
// 功能: 锥形搜索 Gaia DR3SP -> 取 BP/RP uint8 光谱 -> Akima+Simpson 积分 F_syn
//       -> WCS 投影 -> KDTree 匹配 PSF 星 -> MAD 清洗 -> 全局 scale 校正
//
// 输入参数:
//   gaia_client_handle - gaia_client_create 返回的 handle (NULL 时返回错误)
//   ra_center, dec_center, radius_deg - 锥形搜索中心与半径(度)
//   mag_min, mag_max    - 星等范围
//   filter_wl/trans/count - 滤光片波长(nm)与透过率[0,1]
//   spectrum_wl, spectrum_count - 光谱波长数组[336,338,...,1020]nm (长度通常343)
//   pixels, width, height - 输入图像 float32 [H*W]
//   psf_cx/cy/flux/status, n_psf - PSF 测光星 (status 0=有效)
//   WCS 参数: crval1/2, crpix1/2, cd11/12/21/22
//   SIP 参数: sip_order, sip_a/b/ap/bp (可为 nullptr, 长度36)
//
// 输出参数:
//   out_pixels      - 校正后像素 (调用者分配, float32 [H*W])
//   out_n_matched   - 匹配星数 (MAD清洗后)
//   out_scale_factor- scale因子
//   out_sigma_residual - sigma_residual = MAD(log10(F_instr/F_syn))/0.6745
//                        (可为 nullptr, 向后兼容; 供 SNR 模块 §14 计算 SNR_phot)
//
// 返回: 0=成功, <0=失败
//   -1: 空指针/参数无效
//   -2: gaia_client_handle 为空
//   -3: 锥形搜索失败或无光谱星
// ============================================================================
PC_API int pc_calibrate_simple_with_gaia(
    void* gaia_client_handle,
    double ra_center, double dec_center, double radius_deg,
    double mag_min, double mag_max,
    const double* filter_wl, const double* filter_trans, int filter_count,
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
    double* out_sigma_residual);

#ifdef __cplusplus
}
#endif

#endif // PHOTOMETRIC_CALIB_H
