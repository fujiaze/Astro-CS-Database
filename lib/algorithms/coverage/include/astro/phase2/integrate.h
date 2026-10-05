// lib/algorithms/coverage/include/astro/phase2/integrate.h
//
// Phase2 W8：SNR/support/quality 加权叠加 + HiPS mosaic tile 输出。
//
// 语义（冻结 + 零权重合同，INTEGRATION_ZERO_WEIGHT_CONTRACT）：
// - 输入为同一输出像素的 UPM-calibrated **accepted** 样本栈；
// - 权重策略（与 UPM observation weight 严格分开命名）：
// **单一权重口径** —— 唯一生产策略 = 调用方构造的**逐样本逆方差**权重
// w = SNR²/F_ref² = 1/σ_F²（ivar 产品，或 ivar 缺失时的帧级 SNR 逆方差链），
// 构造后先经 p2_validate_candidate_weights。
// 原 stack.support_x_snr2.v1（weight_mode=0，
// weights = support × SNR²）与 stack.equal.v1（weight_mode=1 → 等权）两个
// **可选口径**及其 weight_mode 选择键**已删除** —— support 是无量纲几何量、
// equal 是等权，二者都不是信号/噪声之比（docs/ACSD_DESIGN.md §3.1:173/175；
// docs/science/unified/UNIFIED_SCIENCE_MODEL.md §4:72「不存在口径选择键、口径枚举、
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
// OK / NO_CANDIDATES / ALL_REJECTED / ZERO_VALID_WEIGHT / INVALID_INPUT /
// NON_FINITE_OUTPUT。前五态是冻结面；NON_FINITE_OUTPUT = 归约溢出（输入合法、
// 输出非有限）——它带**非 OK 码**，调用方按「本像素无有效统计贡献」处置
// （signal 为 NaN、该像素不计入有效像素），**不得**当 OK 读。
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
// 数值取值 0..4 是冻结面（SCIENCE_FREEZE.md「INTEGRATION_CONTRACT = FROZEN
// 显式状态 OK/NO_CANDIDATES/ALL_REJECTED/ZERO_VALID_WEIGHT/INVALID_INPUT」、
// ERROR_HANDLING_STANDARD.md 错误模型、docs/detail/registry/acsd.phase2.integrate.md
// 「五态显式 status」）：0..4 的**取值与语义都不动**。
// NON_FINITE_OUTPUT 是为「五态无法如实表达的一个既有物理态」追加的第 6 态，
// 追加它**不改变**任何既有态的含义，也不改变任何既有取值（下游全部按具名
// 等值比较，见 src/integrate.cpp 的发布面说明）。
enum P2IntegrateStatus {
    P2_INTEGRATE_OK = 0,
    P2_INTEGRATE_NO_CANDIDATES = 1,
    P2_INTEGRATE_ALL_REJECTED = 2,
    P2_INTEGRATE_ZERO_VALID_WEIGHT = 3,
    P2_INTEGRATE_INVALID_INPUT = 4,
    // 资格门**全部通过**（输入全 finite、权重全正），但归约累加溢出：
    // Σw·v 或 Σw 溢出到 ±Inf，或二者相除得 NaN。此时 signal 按
    // docs/engineering/standards/NUMERIC.md「MUST」一节「无效的唯一表示 = NaN」发 NaN，
    // **不得**发 0/±Inf/哨兵值伪装成有效。既有五态没有一态能如实表达它：
    // INVALID_INPUT 会谎称「输入非法」（输入全合法），ZERO_VALID_WEIGHT 会谎称
    // 「权重全 0」（权重全正）。
    P2_INTEGRATE_NON_FINITE_OUTPUT = 5
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
