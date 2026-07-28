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
// P12-001: Photometric 分阶段诊断结构体
// 各阶段计数埋点, 供 Python 侧分析匹配失败原因. 所有可能为 nullptr 的出参均向后兼容.
// ============================================================================
struct PhotometricDiag {
    // 阶段1: Fsyn
    int spectrum_rows_total;      // n_gaia (锥形搜索返回的行数)
    int valid_fsyn;               // f_syn > 0 且有限 的星数
    // 阶段2: 投影
    int gaia_projected_in_frame;  // 投影后落在 [0,W) x [0,H) 的 Gaia 星数
    // 阶段3: PSF
    int psf_total;                // n_psf (输入 PSF 星总数)
    int psf_valid;                // status==0 的 PSF 星数
    // 阶段4/5: 匹配
    int spatial_candidates;       // KD-tree 查询命中数 (距离<阈值的初始匹配)
    int unique_matches;           // 唯一配对后数 (当前实现无双向过滤, 等于 spatial_candidates)
    // 阶段6: 拒绝原因
    int rejected_ambiguous;       // 双向匹配冲突 (当前无双向, 保持 0)
    int rejected_distance;        // 距离超阈值 (KD-tree 最近邻仍 > match_radius_px)
    int rejected_quality;         // F<=0/非有限 + 星等不一致 + IRLS 离群 的总和
    // 阶段7: 拟合
    int fit_used;                 // IRLS inliers (Tukey 权重 > 0)
    int robust_iterations;        // IRLS 实际迭代次数
    double scale_factor;          // 10^(-location(r)), 与 out_scale_factor 一致
    double sigma_residual;        // MAD(r_inliers)/0.6745, 与 out_sigma_residual 一致
    // 阶段8: 残差/距离统计
    double r_median, r_p90, r_max;                       // r = log10(F_instr/F_syn) 的 inliers 统计
    double match_distance_median, match_distance_p90, match_distance_max;  // PSF-Gaia 像素距离统计
};

// ============================================================================
// 简化版测光校准 C API (GAP-012 + GAP-013 改进版)
//
// 功能: WCS投影Gaia星 -> KD-tree匹配PSF星 -> 星等一致性过滤
//       -> IRLS+Tukey 稳健清洗 -> 全局 scale 校正
// 算法:
//   1. WCS投影Gaia星到像素坐标 (TAN+SIP投影)
//   2. 对 Gaia 像素坐标建 KD-tree, 对每颗 PSF 星找最近邻 Gaia 星 (距离<2px)
//   3. 星等一致性预过滤 (|delta - median_delta| > 3 mag 拒绝)
//   4. IRLS + Tukey biweight 稳健位置估计 (c=4.685, 50 次迭代, 收敛 1e-6)
//   5. scale = 10^(-location), I_cal = I * scale
//   6. sigma_residual = MAD(r_inliers)/0.6745 供 SNR 模块使用
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
//   qe_wl         - CCD QE 波长数组 [qe_count] (nm), 可为 nullptr (此时 Q(λ)=1.0)
//                   注: pc_calibrate_simple 不在 DLL 内部计算 F_syn, QE 参数仅作 API 一致性保留
//   qe_trans      - CCD QE 透过率数组 [qe_count] [0,1], qe_wl 为 nullptr 时忽略
//   qe_count      - QE 点数, qe_wl 为 nullptr 时可为 0
//   WCS参数: crval1/2, crpix1/2, cd11/12/21/22
//   SIP参数: sip_order, sip_a/b/ap/bp数组 (可为nullptr, 长度36)
//
// 输出参数:
//   out_pixels      - 校正后像素 (调用者分配, float32 [H*W])
//   out_n_matched   - 匹配星数 (IRLS+Tukey 清洗后)
//   out_scale_factor- scale因子 (IRLS 稳健估计, 10^(-location(r)))
//   out_sigma_residual - sigma_residual = MAD(log10(F_instr/F_syn)_inliers)/0.6745
//                        (可为 nullptr, 向后兼容; 供 SNR 模块 §14 计算 SNR_phot)
//   out_diag        - P12-001 分阶段诊断结构体 (可为 nullptr, 向后兼容)
//                     旧接口仅填充部分字段; 完整诊断请使用 pc_calibrate_simple_with_gaia
//
// 返回: 0=成功, <0=失败
// ============================================================================
PC_API int pc_calibrate_simple(
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
    double* out_sigma_residual,
    PhotometricDiag* out_diag);

// ============================================================================
// 扩展接口: 接受 gaia_client handle, DLL 内部查询 DR3SP 光谱并积分得 F_syn
//
// 功能: 锥形搜索 Gaia DR3SP -> 取 BP/RP uint8 光谱
//       -> Akima+Simpson 积分 F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ × 10^(-0.4*mag_g)
//       -> WCS 投影 -> KD-tree 匹配 PSF 星 -> 星等一致性 + IRLS/Tukey 清洗
//       -> 全局 scale 校正
//
// 输入参数:
//   gaia_client_handle - gaia_client_create 返回的 handle (NULL 时返回错误)
//   ra_center, dec_center, radius_deg - 锥形搜索中心与半径(度)
//   mag_min, mag_max    - 星等范围
//   filter_wl/trans/count - 滤光片波长(nm)与透过率[0,1]
//   qe_wl/trans/count   - CCD QE 波长(nm)与透过率[0,1] (GAP-012; 可为 nullptr, Q(λ)=1.0)
//   spectrum_wl, spectrum_count - 光谱波长数组[336,338,...,1020]nm (长度通常343)
//   pixels, width, height - 输入图像 float32 [H*W]
//   psf_cx/cy/flux/status, n_psf - PSF 测光星 (status 0=有效)
//   WCS 参数: crval1/2, crpix1/2, cd11/12/21/22
//   SIP 参数: sip_order, sip_a/b/ap/bp (可为 nullptr, 长度36)
//
// 输出参数:
//   out_pixels      - 校正后像素 (调用者分配, float32 [H*W])
//   out_n_matched   - 匹配星数 (IRLS+Tukey 清洗后)
//   out_scale_factor- scale因子 (IRLS 稳健估计, 10^(-location(r)))
//   out_sigma_residual - sigma_residual = MAD(log10(F_instr/F_syn)_inliers)/0.6745
//                        (可为 nullptr, 向后兼容; 供 SNR 模块 §14 计算 SNR_phot)
//   out_diag        - P12-001 分阶段诊断结构体 (可为 nullptr, 向后兼容)
//                     完整填充所有 8 个阶段字段
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
    double* out_sigma_residual,
    PhotometricDiag* out_diag);

#ifdef __cplusplus
}
#endif

#endif // PHOTOMETRIC_CALIB_H
