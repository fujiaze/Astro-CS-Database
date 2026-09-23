// lib/algorithms/coverage/include/astro/phase2/integrate.h
//
// Phase2 W8：SNR/support/quality 加权叠加 + HiPS mosaic tile 输出。
//
// 语义（W8 冻结 + 零权重合同，INTEGRATION_ZERO_WEIGHT_CONTRACT）：
// - 输入为同一输出像素的 UPM-calibrated **accepted** 样本栈；
// - 权重策略（与 UPM observation weight 严格分开命名）：
// **单一权重口径** —— 唯一生产策略 = 调用方构造的**逐样本逆方差**权重
// w = SNR²/F_ref² = 1/σ_F²（ivar 产品，或 ivar 缺失时的帧级 SNR 逆方差链），
// 构造后先经 p2_validate_candidate_weights。
// §9.73 裁决 A44（同批清理）：原 stack.support_x_snr2.v1（weight_mode=0，
// weights = support × SNR²）与 stack.equal.v1（weight_mode=1 → 等权）两个
// **可选口径**及其 weight_mode 选择键**已删除** —— support 是无量纲几何量、
// equal 是等权，二者都不是信号/噪声之比（ASTROCS_DESIGN.md §3.1:173/175；
// docs/science/PSF_SIGNAL_WEIGHT.md §4:72「不存在口径选择键、口径枚举、
// 口径配置项或口径产物」）。
// UPM 控制点权重为 upm.robust_control_weight.v1（不同语义，禁止混名）。
// -：reducer 只消费 values / 外部 numeric weights / support / accepted；
// 不编码 ivar/SNR 科学策略（policy 在调用方）。weights=nullptr 是**本 C API
// 的输入合同**（无权重数组 ⇒ 等权），不是可选择口径：生产唯一调用方
// （module_adapters.cpp p2_op_integrate）恒传 weights（与 values 同步填充），
// 故 nullptr 分支在生产不可达（count=0 → NO_CANDIDATES）。
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
