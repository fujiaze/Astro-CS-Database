// lib/phase2/include/astro/phase2/integrate.h
//
// Phase2 W8：SNR/support/quality 加权叠加 + HiPS mosaic tile 输出。
//
// 语义（V17 冻结）：
//   - 输入为同一输出像素的 UPM-calibrated **accepted** 样本栈；
//   - 权重策略（与 UPM observation weight 严格分开命名）：
//       stack.support_x_snr2.v1（weight_mode=0 + weights 提供；weights =
//         support × SNR²，调用方负责构造并先经 validate_candidate_weights）
//       stack.equal.v1（weight_mode=1 或 weights=nullptr → 等权）
//     UPM 控制点权重为 upm.robust_control_weight.v1（不同语义，禁止混名）。
//   - 资格（integration eligibility）：finite(value)、finite(weight) &&
//     weight>0、finite(support) && support>0、accepted==true；任一非 finite
//     输入 → INVALID_INPUT（绝不产生 OK+NaN）。
//   - output support 唯一 canonical reducer：**max(accepted support)**
//     （覆盖并集保守下界，V13/V11 冻结语义）；Stage2/ACR 只消费 pr.support，
//     不再自行第二次 max/mean。
//   - status 枚举（与 rejection status 分离）：
//       OK / NO_CANDIDATES / ALL_REJECTED / ZERO_VALID_WEIGHT / INVALID_INPUT。
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
    const double* weights;        // 可空=等权；提供时须为正有限
    const double* support;        // 可空=1.0；提供时须为正有限
    const std::uint8_t* accepted; // 可空=全接受
    std::uint32_t count;
    int weight_mode;              // 0=stack.support_x_snr2.v1, 1=stack.equal.v1
} P2PixelStack;

// V17：integration status（unambiguous）
enum P2IntegrateStatus {
    P2_INTEGRATE_OK = 0,
    P2_INTEGRATE_NO_CANDIDATES = 1,
    P2_INTEGRATE_ALL_REJECTED = 2,
    P2_INTEGRATE_ZERO_VALID_WEIGHT = 3,
    P2_INTEGRATE_INVALID_INPUT = 4
};

typedef struct {
    double signal;                // 加权均值（原始科学值域）
    double support;               // max(accepted support)（canonical reducer）
    std::uint32_t n_used;         // 实际参与积分样本数（正权重 accepted）
    // V17：显式计数器（不靠 n_used 猜原因）
    std::uint32_t n_candidates;   // 输入样本数
    std::uint32_t n_accepted;     // accepted 掩码通过数
    std::uint32_t n_finite;       // accepted 且 value/support/weight finite
    std::uint32_t n_positive_weight; // finite 且 weight>0
    int status;                   // P2IntegrateStatus
} P2PixelResult;

P2_API int p2_integrate_pixel(const P2PixelStack* in, P2PixelResult* out);

// V17：候选权重校验（integration eligibility 的一部分；Stage2 在 SNR
// lookup 后调用，防止漏检非 finite/非正权重）
P2_API int p2_validate_candidate_weights(const double* weights,
                                         std::uint32_t count);

#ifdef __cplusplus
}
#endif
