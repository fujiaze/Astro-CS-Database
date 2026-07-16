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
//
// 注意: 此接口保留用于测试/调试, 管线中不再调用 (改用 snr_extract_model)
// ============================================================================
SNR_API int snr_estimate(const float* data, int h, int w,
                         const double* psf, int n_stars,
                         double sigma_residual,
                         float* out_snr);

// ============================================================================
// WCS 参数 (简化版, 不含 SIP, 用于像素→球面坐标转换)
// CRPIX 是 1-based (FITS 标准)
// ============================================================================
typedef struct {
    double crval1;  // 参考点赤经 (度)
    double crval2;  // 参考点赤纬 (度)
    double crpix1;  // 参考点像素 X (1-based)
    double crpix2;  // 参考点像素 Y (1-based)
    double cd[4];   // CD 矩阵 [cd11, cd12, cd21, cd22] (度/像素)
} SnrWcsParams;

// ============================================================================
// SNR 控制点 (球面坐标 + snr_psf 值)
// ============================================================================
typedef struct {
    double ra;       // 球面赤经 (度)
    double dec;      // 球面赤纬 (度)
    float  snr_psf;  // (A-B)/mad (无量纲)
} SnrControlPoint;

// ============================================================================
// SNR 模型 (稀疏控制点 + 全局参数)
//
// SNR(ra,dec) = snr_phot × (IDW_spherical(points, query) / median_snr)
// IDW: weight = 1/γ^idw_power, γ=球面大圆弧角距离
// ============================================================================
typedef struct {
    uint32_t n_points;          // 控制点数
    SnrControlPoint* points;    // 控制点数组 (调用者负责释放, 用 snr_free_model)
    double   snr_phot;          // 1/(ln10×sigma_residual) 全局标量
    double   median_snr;        // median(snr_psf) 归一化基准
    double   idw_power;         // IDW 幂次 (默认 2.0)
} SnrModel;

// ============================================================================
// snr_extract_model - 从 PSF 块提取稀疏 SNR 控制点模型
//
// 输入:
//   psf            - PSF 拟合结果 double [n_stars*9] (同 snr_estimate)
//   n_stars        - PSF 星数量
//   sigma_residual - 测光残差 sigma
//   wcs            - WCS 参数 (用于像素坐标→球面坐标转换)
//   out_model      - 输出 SNR 模型 (调用者负责用 snr_free_model 释放)
//
// 返回: 0=成功, 1=n_stars<=0或无有效星(退化), 2=sigma_residual<=0(退化), 3=nullptr
//
// 有效星条件: status==0, A>B, mad>0
// 控制点坐标: PSF 星位置 (cx,cy) 经 WCS 转球面 (ra,dec)
// ============================================================================
SNR_API int snr_extract_model(const double* psf, int n_stars,
                               double sigma_residual,
                               const SnrWcsParams* wcs,
                               SnrModel* out_model);

// ============================================================================
// snr_free_model - 释放 SnrModel 内部资源
// ============================================================================
SNR_API void snr_free_model(SnrModel* model);

#ifdef __cplusplus
}
#endif

#endif // SNR_ESTIMATOR_H
