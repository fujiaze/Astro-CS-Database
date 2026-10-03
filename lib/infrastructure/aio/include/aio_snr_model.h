#ifndef AIO_SNR_MODEL_H
#define AIO_SNR_MODEL_H

// ============================================================================
// aio_snr_model.h - 稀疏 SNR 控制点模型 (P2 跨帧绝对信噪比的数据载体)
//
// 本头文件从已退场的 aio_healpix_io.h 原样迁出。迁出的理由是结构性的:
//   aio_healpix_io.h 原本把「稀疏 SNR 控制点模型」与「.hiss / .hcsd 遗留容器
//   文件 I/O」混在同一个 extern "C" 块里。容器面退场后，模型本身仍被生产
//   drizzle 链消费 (hp_drizzle_api.cpp 构造、HISS_VERIFY 之外的稀疏 SNR 重建)，
//   它与容器格式无任何关系，故独立成头。
//
// 字段语义 (P2/P4 判据参数, 权威 = docs/science/DATA_SEMANTICS.md 的 SNR
// 控制点条目):
//   snr_psf     单点 SNR, 无量纲
//   snr_phot    1/(ln10×sigma_residual) 全局标量
//   median_snr  median(snr_psf) 归一化基准
//   idw_power   IDW 幂次 (默认 2.0)
// ============================================================================

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// SNR 控制点 (球面坐标 + snr_psf 值, 20 字节, 用于序列化)
#pragma pack(push, 1)
typedef struct {
    double ra;       // 球面赤经 (度)
    double dec;      // 球面赤纬 (度)
    float  snr_psf;  // (A-B)/mad (无量纲)
} HioSnrControlPoint;
#pragma pack(pop)
static_assert(sizeof(HioSnrControlPoint) == 20, "HioSnrControlPoint must be 20 bytes");

// FP64 SNR 控制点 (snr_psf 保留 double 精度)
#pragma pack(push, 1)
typedef struct {
    double ra;       // 球面赤经 (度)
    double dec;      // 球面赤纬 (度)
    double snr_psf;  // (A-B)/mad (double)
} HioSnrControlPointF64;
#pragma pack(pop)
static_assert(sizeof(HioSnrControlPointF64) == 24, "HioSnrControlPointF64 must be 24 bytes");

// SNR 模型 (稀疏控制点 + 全局参数)
// 对应 snr_format=1 的二进制布局:
// [n_points: uint32]
// [points: n_points * 20B]
// [snr_phot: f64][median_snr: f64][idw_power: f64]
typedef struct {
    uint32_t n_points;              // 控制点数
    HioSnrControlPoint* points;     // 控制点数组 (由调用方持有生命周期)
    double   snr_phot;              // 1/(ln10×sigma_residual) 全局标量
    double   median_snr;            // median(snr_psf) 归一化基准
    double   idw_power;             // IDW 幂次 (默认 2.0)
} HioSnrModel;

// FP64 SNR 模型 (value_dtype=1, 控制点为 HioSnrControlPointF64)
typedef struct {
    uint32_t n_points;
    HioSnrControlPointF64* points;
    double   snr_phot;
    double   median_snr;
    double   idw_power;
} HioSnrModelF64;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AIO_SNR_MODEL_H
