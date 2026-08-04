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
// FP64 版本 (R10 双精度 ABI 改造)
//
// 与 snr_estimate 逻辑一致, 仅 data 类型由 float 改为 double.
// 输出 out_snr 仍为 float32 (HISS SNR 子块格式已冻结为 float32, 见 02_FROZEN §17;
// SNR 是诊断值不是科学累加值, 精度损失可接受).
//
// 注意: 本接口保留用于测试/调试, 管线中不再调用 (改用 snr_extract_model, 后者
//       仅依赖 PSF double 参数, 与图像精度无关, 无需 f64 变体).
// ============================================================================
SNR_API int snr_estimate_f64(const double* data, int h, int w,
                             const double* psf, int n_stars,
                             double sigma_residual,
                             float* out_snr);

// ============================================================================
// SIP 前向系数 (A/B 多项式, 用于 pixel→sky 方向)
// 系数按 a[i*6+j] 存储, 对应 dx^i * dy^j, 下三角 i+j<=order
// 阶数上限 5 (36 元素), 与 FITS SIP 标准和 healpix_drizzle/wcs_sip.h 一致
// a_order<=0 表示无 SIP 修正 (仅用 CD+TAN)
// ============================================================================
#define SNR_SIP_MAX_ORDER 5
#define SNR_SIP_COEFF_SIZE 36  // (ORDER+1)*(ORDER+2)/2 上限, 用 6x6 数组存储

typedef struct {
    int a_order;                  // 前向 A 多项式阶数 (0=无)
    int b_order;                  // 前向 B 多项式阶数 (0=无)
    double a[SNR_SIP_COEFF_SIZE]; // A_i_j 系数 (前向, 像素→中间坐标)
    double b[SNR_SIP_COEFF_SIZE]; // B_i_j 系数
} SnrSipCoeffs;

// ============================================================================
// WCS 参数 (完整版, 含前向 SIP A/B, 用于像素→球面坐标转换)
// CRPIX 是 1-based (FITS 标准)
// ============================================================================
typedef struct {
    double crval1;  // 参考点赤经 (度)
    double crval2;  // 参考点赤纬 (度)
    double crpix1;  // 参考点像素 X (1-based)
    double crpix2;  // 参考点像素 Y (1-based)
    double cd[4];   // CD 矩阵 [cd11, cd12, cd21, cd22] (度/像素)
    SnrSipCoeffs sip;  // 前向 SIP 系数 (a_order=0 时无修正)
} SnrWcsParams;

// ============================================================================
// SNR-001: SNR 丢弃原因枚举 (SNR 不得静默丢点)
// 每个未写入 HISS 的 SNR 控制点必须有唯一原因分类。
// 值 0=未丢弃(已写入); 1-127=SNR 阶段丢弃(snr_extract_model 内部过滤);
// 128-254=Drizzle 阶段丢弃(写入 HISS 时); 255=其他未知原因。
// ============================================================================
typedef enum {
    SNR_DROP_NOT_DROPPED      = 0,    // 已写入 (有效控制点)
    // SNR 阶段丢弃 (snr_extract_model 内部过滤)
    SNR_DROP_OUTSIDE_TILE     = 1,    // 点不在任何 Tile 范围内 (drizzle 阶段判断)
    SNR_DROP_NO_OVERLAP       = 2,    // 点所在 Tile 无 signal 覆盖 (drizzle 阶段判断)
    SNR_DROP_INVALID_PSF      = 3,    // PSF 拟合失败: status != 0 或 mad <= 0
    SNR_DROP_INVALID_WCS      = 4,    // WCS 转换失败
    SNR_DROP_ZERO_FLUX        = 5,    // 通量为零 / 振幅不足 (A <= B)
    SNR_DROP_DUPLICATE_IPIX   = 6,    // 重复 local_ipix (保留首次, drizzle 阶段)
    SNR_DROP_OTHER            = 255   // 其他未知原因
} SnrDropReason;

// SNR 丢弃原因计数数组大小 (覆盖 0-255 全部枚举值)
#define SNR_DROP_REASON_COUNT 256

// ============================================================================
// SNR 控制点 (球面坐标 + snr_psf 值)
// R10 修复: 添加 #pragma pack(1) 确保 sizeof==20, 与 HioSnrControlPoint 二进制布局一致
//   根因: 未打包时 sizeof(SnrControlPoint)=24 (4字节尾部填充),
//         但 orchestrator 序列化用 memcpy(dst, points, n*20) 按 20 字节连续拷贝,
//         导致从第 2 个点起 ra/dec 错位, 产生 1609 个"越界"点.
// ============================================================================
#pragma pack(push, 1)
typedef struct {
    double ra;       // 球面赤经 (度)
    double dec;      // 球面赤纬 (度)
    float  snr_psf;  // (A-B)/mad (无量纲)
} SnrControlPoint;
#pragma pack(pop)
static_assert(sizeof(SnrControlPoint) == 20, "SnrControlPoint must be 20 bytes (packed, matches HioSnrControlPoint)");

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
