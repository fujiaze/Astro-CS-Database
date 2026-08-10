// lib/phase2/include/astro/phase2/integrate.h
//
// Phase2 W8：SNR/support/quality 加权叠加 + HiPS mosaic tile 输出。
//
// 语义（冻结）：
//   - 输入为同一输出像素的 UPM-calibrated accepted 样本栈；
//   - 权重 = SNR/quality/support 组合（首版 weight_mode=auto → snr2 归一化）；
//   - 输出 signal = Σ(w_i·v_i)/Σ(w_i)，support = 覆盖率；全拒/零权重有明确定义；
//   - HiPS tile 复用 AIO writer（aio_hips_*），Phase2 不新建 I/O DLL。
#pragma once

#include <cstdint>

#ifdef _WIN32
#define P2_API __declspec(dllexport)
#else
#define P2_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const double* values;         // UPM-calibrated accepted 样本
    const double* weights;        // 权重（可空=等权）
    const double* support;        // 可空
    const std::uint8_t* accepted; // 可空（全接受）
    std::uint32_t count;
    int weight_mode;              // 0=snr2_normalized, 1=equal
} P2PixelStack;

typedef struct {
    double signal;                // 加权均值
    double support;               // 覆盖率均值（可空输入=1）
    std::uint32_t n_used;
    int status;                   // 0=ok, 1=all-rejected, 2=zero-weight
} P2PixelResult;

P2_API int p2_integrate_pixel(const P2PixelStack* in, P2PixelResult* out);

#ifdef __cplusplus
}
#endif