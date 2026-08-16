// lib/phase2/include/astro/phase2/integrate.h
//
// Phase2 W8：SNR/support/quality 加权叠加 + HiPS mosaic tile 输出。
//
// 语义（W8 冻结 + 零权重合同，INTEGRATION_ZERO_WEIGHT_CONTRACT）：
// - 输入为同一输出像素的 UPM-calibrated **accepted** 样本栈；
// - 权重策略（与 UPM observation weight 严格分开命名）：
// stack.support_x_snr2.v1（weight_mode=0 + weights 提供；weights =
// support × SNR²，调用方负责构造并先经 validate_candidate_weights）
// stack.equal.v1（weight_mode=1 或 weights=nullptr → 等权）
// UPM 控制点权重为 upm.robust_control_weight.v1（不同语义，禁止混名）。
// -：reducer 只消费 values / 外部 numeric weights（可空=等权）/
// support / accepted；不编码 ivar/SNR 科学策略（policy 在调用方）。
// - 权重资格（冻结）：NaN/Inf/负权重 → INVALID_INPUT；weight==0 → 合法
// 但不贡献（ZERO_VALID_WEIGHT）；weight>0 → 可用。value 非 finite、
// support 非 finite 或 <=0、accepted 掩码之外的样本按同样 INVALID 规则。
// - output support 唯一 canonical reducer：**max(accepted support)**
// （覆盖并集保守下界， 冻结语义）；Stage2/ACR 只消费 pr.support，
// 不再自行第二次 max/mean。
// - status 枚举（与 rejection status 分离）：
// OK / NO_CANDIDATES / ALL_REJECTED / ZERO_VALID_WEIGHT / INVALID_INPUT。
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
    const double* weights;        // 可空=等权；提供时须为 nonnegative finite
    const double* support;        // 可空=1.0；提供时须为正有限
    const std::uint8_t* accepted; // 可空=全接受
    std::uint32_t count;
} P2PixelStack;

// integration status（unambiguous）
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
    // 显式计数器（不靠 n_used 猜原因）
    std::uint32_t n_candidates;   // 输入样本数
    std::uint32_t n_accepted;     // accepted 掩码通过数
    std::uint32_t n_finite;       // accepted 且 value/support/weight finite
    std::uint32_t n_positive_weight; // finite 且 weight>0
    int status;                   // P2IntegrateStatus
} P2PixelResult;

P2_API int p2_integrate_pixel(const P2PixelStack* in, P2PixelResult* out);

// 候选权重校验（integration eligibility 的一部分；Stage2 在权重构造后
// 调用）。 合同：NaN/Inf/负权重 → failure；零权重合法（不贡献）。
P2_API int p2_validate_candidate_weights(const double* weights,
                                         std::uint32_t count);

#ifdef __cplusplus
}
#endif
