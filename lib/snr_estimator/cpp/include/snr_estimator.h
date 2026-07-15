#ifndef SNR_ESTIMATOR_H
#define SNR_ESTIMATOR_H

#include <cstdint>

#ifdef _WIN32
#define SNR_API __declspec(dllexport)
#else
#define SNR_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// SNR 估算 - 乘法模型
// SNR(pixel) = SNR_phot × (SNR_psf(pixel) / median(SNR_psf))
//
// 输入:
//   data          - 图像像素 float32 [h*w] (行优先, 来自 CALIBRATE 阶段)
//   h, w          - 图像尺寸
//   psf           - PSF 拟合结果 double [n_stars*9]
//                   每行: [status, B, flux, cx, cy, fwhm, A, mad, eccentricity]
//                   列索引: status=0, B=1, flux=2, cx=3, cy=4, fwhm=5, A=6, mad=7, eccentricity=8
//   n_stars       - PSF 星数量
//   sigma_residual - 测光残差 sigma (来自 photo_stats 块 SIGMA_RESIDUAL)
//   out_snr       - 输出 SNR 图 float32 [h*w] (调用者分配)
//
// 返回: 0=成功, 1=n_stars<=0(退化,全填SNR_phot), 2=sigma_residual<=0(退化,全填1.0), 3=nullptr
// ============================================================================
SNR_API int snr_estimate(const float* data, int h, int w,
                         const double* psf, int n_stars,
                         double sigma_residual,
                         float* out_snr);

#ifdef __cplusplus
}
#endif

#endif // SNR_ESTIMATOR_H
