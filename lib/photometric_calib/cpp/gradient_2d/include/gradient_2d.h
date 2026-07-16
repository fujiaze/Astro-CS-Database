#ifndef GRADIENT_2D_H
#define GRADIENT_2D_H

#include <cstdint>

#ifdef _WIN32
#define G2D_API __declspec(dllexport)
#else
#define G2D_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Gradient2D - 单帧 2D 测光校准 (STAGE_GRADIENT_2D)
//
// 功能: 星-图匹配 + 乘性梯度曲面拟合 (IRLS+Tukey biweight+Ridge+LOOCV 选阶)
//       + 图像校正 + 通量归一化到 Gaia 参考星系统
//
// 算法 (源自 lib/photometric_calib/archive/estimator.py 的 C++ 化):
//   1. WCS 投影 Gaia 星到像素坐标 (TAN + SIP)
//   2. 暴力最近邻匹配 PSF 星 <-> Gaia 星 (距离 < match_radius_px)
//   3. MAD 离群清洗: r = log10(F_instr / F_syn), 剔除 |r - median| > sigma * MAD/0.6745
//   4. 乘性梯度曲面拟合:
//      - 输入 r = log10(F_instr / F_syn) (log10(M_true), M_true = F_instr / F_syn)
//      - 2D 多项式基 (单项式 (j,k), j+k <= order), 坐标归一化 [-1,1]
//      - IRLS + Tukey biweight (c=4.685) 稳健回归
//      - Ridge L2 正则化 (alpha 网格 {0, 0.1, 1, 10, 100, 1000}, 常数项不正则)
//      - 帽子矩阵 LOOCV 自动选阶 (min_order=1, max_order=min(max_order,3))
//      - 10% 容差选最简模型 (最低阶 + 最小 alpha)
//   5. R² 信号检测: R² < 0.02 时跳过乘性校正 (M_map=1.0), 避免拟合噪声
//   6. 图像校正: I_cal(x,y) = (I(x,y) - S) / max(M(x,y), 0.01)
//      [封存 2026-07-12] 加性梯度 S_map 已封存 (S=0), 仅做乘性流量定标
//      原因: PSF 背景含 ISL+DGL+气辉+黄道光+星云信号, 稀疏采样下多项式易过拟合
//   7. 通量归一化:
//      - F_cal_i = F_instr_i / M(x_i, y_i)
//      - scale = median(F_syn_i / F_cal_i)
//      - I_final(x,y) = I_cal(x,y) * scale
//
// 输入参数:
//   pixels         - 输入图像 float32 [H*W] (行优先, 原地校正)
//   width, height  - 图像尺寸
//   gaia_ra/dec    - Gaia 星 RA/Dec 数组 [n_gaia] (度)
//   gaia_mag       - Gaia 星 magnitude 数组 [n_gaia]
//   gaia_fsyn      - Gaia 星合成流量数组 [n_gaia]
//   n_gaia         - Gaia 星数量
//   psf_cx/cy      - PSF 星 x/y 坐标数组 [n_psf] (0-based)
//   psf_flux       - PSF 星 flux 数组 [n_psf]
//   psf_status     - PSF 星状态数组 [n_psf] (0=成功)
//   n_psf          - PSF 星数量
//   WCS 参数: crval1/2, crpix1/2, cd11/12/21/22
//   SIP 参数: sip_order, sip_a/b/ap/bp 数组 (可为 nullptr, 长度 36, 按 i*6+j 索引)
//   match_radius_px - 星-图匹配半径 (像素), 默认 3.0
//   outlier_sigma   - MAD 离群清洗 sigma 阈值, 默认 3.0
//   max_order       - 梯度曲面最高阶数, 内部限制为 min(max_order, 3)
//
// 输出参数:
//   out_pixels      - 校正后像素 (调用者分配, float32 [H*W])
//   result          - Gradient2DResult 结构 (质量报告)
//
// 返回: 0=成功, <0=失败
//   -1: 空指针/参数无效
//   -2: 匹配星数 < 6 (退化路径, 返回 scale=1.0, 恒等校正)
//   -3: 数值异常 (M_map 非有限等)
// ============================================================================

typedef struct {
    double scale_factor;          // 乘性 scale (median(F_syn / F_cal))
    double additive_zero;         // 加性零点 (已封存, 恒为 0.0)
    int    n_matched;             // 匹配星数 (MAD 清洗后)
    int    n_excluded;            // 剔除离群星数
    double rms_residual;          // 残差 RMS (IRLS 内点)
    int    mult_order;            // 乘性曲面阶数
    int    mult_n_used;           // 乘性拟合使用点数 (IRLS weight>0)
    int    mult_n_rejected;       // 乘性拟合排除点数
    double mult_loocv_error;      // 乘性曲面 LOOCV 误差
    double mult_r_squared;        // 乘性 R² (决定系数)
    int    mult_skipped;          // 乘性是否跳过 (R² < 0.02): 0=未跳过, 1=跳过
    double sigma_residual;        // MAD/0.6745 (供 SNR 模块计算 SNR_phot)
    char   quality_report[4096];  // 质量报告 JSON
} Gradient2DResult;

G2D_API int gradient_2d_calibrate(
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
    float* out_pixels, Gradient2DResult* result);

#ifdef __cplusplus
}
#endif

#endif // GRADIENT_2D_H
