// lib/phase2/include/astro/phase2/rejection.h
//
// Phase2 Rejection Framework 公共接口（W2 冻结，控制包 34A532A2...B2EB308）。
//
// 语义（冻结）：
//   - 输入为同一输出 pixel 的 UPM-calibrated 样本栈
//     value[] / valid[] / support[] / weight[] / quality[]；
//   - 输出 accepted_mask 与 low/high 拒绝计数；
//   - CPU reference 优先；ACR 后端必须消费同一语义接口；
//   - Oracle：Astropy sigma_clip、NIST ESD、IRAF AVSIGCLIP、
//     Siril（GPL ORACLE ONLY）、RCR 论文独立实现（官方非商业源码 ORACLE ONLY）。
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

enum P2RejectionMethod {
    P2_REJECT_NONE = 0,
    P2_REJECT_SIGMA = 1,
    P2_REJECT_WINSORIZED_SIGMA = 2,
    P2_REJECT_AVERAGED_SIGMA = 3,
    P2_REJECT_LINEAR_FIT = 4,
    P2_REJECT_GENERALIZED_ESD = 5,
    P2_REJECT_RCR = 6,
    // V14 审核增补（WBPP 方法全集）：
    P2_REJECT_PERCENTILE = 7,     // 相对 median 百分比 clip（Siril 语义）
    P2_REJECT_MEDIAN_SIGMA = 8,   // median 位置 + SD 尺度迭代 clip（WBPP）
    P2_REJECT_MINMAX = 9,         // 每轮剔除最小/最大样本（WBPP Min/Max）
    P2_REJECT_AUTO = 10           // 按样本数自动选择（WBPP Auto 语义）
};

// 输入：value 为 UPM-calibrated 样本；valid/support/weight/quality 可空
typedef struct {
    const double* values;
    const std::uint8_t* valid;      // 可空（全部有效）
    const double* support;          // 可空
    const double* weights;          // 可空（等权）
    const std::uint32_t* quality;   // 可空
    const std::uint64_t* frame_ids; // 可空（稳定帧标识；LinearFit 顺序无关用）
    std::uint32_t count;
    int  data_type;                 // 0=fp32, 1=fp64
    int  method;                    // P2RejectionMethod
    double sigma_low;               // 默认 -4.0
    double sigma_high;              // 默认 +3.0
    int  max_iterations;            // 默认 8
    int  min_samples;               // 最少样本数（不足返回 0/status=MIN_SAMPLES）
} P2SampleStackView;

typedef struct {
    std::uint8_t* accepted;         // 输出掩码（count 字节，调用方分配）
    std::uint32_t accepted_count;
    std::uint32_t rejected_low;
    std::uint32_t rejected_high;
    std::uint32_t iterations;
    int status;                     // 0=ok, 1=min-samples, 2=all-rejected, 3=nan-input
} P2RejectionResult;

P2_API int p2_reject_stack(const P2SampleStackView* in, P2RejectionResult* out);

#ifdef __cplusplus
}
#endif
