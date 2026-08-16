// lib/phase2/include/astro/phase2/rejection.h
//
// Phase2 Rejection Framework 公共接口。
//
// 语义：
// - 输入分三层：
// 1) EligibilityPolicy（p2_eligibility_filter / 生产 strided
// p2_collect_candidate_stack）：finite/valid/support/quality →
// CandidateStack；rejection kernel 不再知道 support/quality；
// 2) RejectionPlan：auto 在 **planning 层**解析为显式方法 +
// method-specific typed params；profile 语义见
// p2_reject_plan_resolve（wbpp_current = group-level 一次解析；
// astrocs_adaptive = tile nominal-depth 自适应，独立命名）；
// 3) RejectionNormalizationPolicy（plan.normalization）：判定工作域
// （working stack）与科学积分域（原始 calibrated values）分离；
// decision 作用于 working stack，accepted mask 应用于原始值。
// - 输出 RejectionDecision：每样本 reason（ACCEPTED / REJECTED_LOW /
// REJECTED_HIGH / UNDERDETERMINED）+ stack-level status。
// - 统计语义：rejected_low = 低于 lower threshold；rejected_high = 高于
// upper threshold（禁止用原始值正负号）。
// - n <= underdetermined_n 或 n < 方法 minimum N → REJECTION_UNDERDETERMINED
// （可全接受但必须记录，禁止偷偷切换另一套算法）。
// - CPU reference 优先；ACR 后端消费同一语义接口（同一 contract）。
// - Oracle：Astropy sigma_clip(median+mad_std)、NIST ESD、AveragedSigma
// （公式定义；IRAF exact = NOT_CLAIMED）、Siril 1.4.3（GPL ORACLE
// ONLY）、RCR 2.4.7 官方固定版本（ORACLE ONLY）。
// - PIXINSIGHT_EXACT_COMPATIBILITY = NOT_CLAIMED（WBPP profile 只提供
// Auto routing/参数映射政策）。
#pragma once

#include <cstddef>
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
    P2_REJECT_NONE = 0,           // astrocs.none.v1
    P2_REJECT_SIGMA = 1,          // alias → astrocs.robust_mad_clip.v1
    P2_REJECT_WINSORIZED_SIGMA = 2, // astrocs.winsorized_sigma_siril_1_4_3.v1
    P2_REJECT_AVERAGED_SIGMA = 3,   // astrocs.averaged_sigma.v1
    P2_REJECT_LINEAR_FIT = 4,       // astrocs.linear_fit_siril_1_4_3.v1
    P2_REJECT_GENERALIZED_ESD = 5,  // astrocs.generalized_esd_nist.v1
    P2_REJECT_RCR = 6,              // astrocs.rcr_2_4_7_ss_median_dl.v1
    P2_REJECT_PERCENTILE = 7,       // astrocs.percentile_siril.v1
    P2_REJECT_MEDIAN_SIGMA = 8,     // astrocs.median_std_clip.v1
    P2_REJECT_MINMAX = 9,           // astrocs.minmax.v1
    P2_REJECT_AUTO = 10             // 只在 planning 层解析，永不进入 kernel
};

// canonical semantic id 常量（runtime 不依赖模糊字符串）
#define P2_SEMANTIC_NONE                  "astrocs.none.v1"
#define P2_SEMANTIC_ROBUST_MAD_CLIP       "astrocs.robust_mad_clip.v1"
#define P2_SEMANTIC_WINSORIZED_SIRIL      "astrocs.winsorized_sigma_siril_1_4_3.v1"
#define P2_SEMANTIC_AVERAGED_SIGMA        "astrocs.averaged_sigma.v1"
#define P2_SEMANTIC_LINEAR_FIT_SIRIL      "astrocs.linear_fit_siril_1_4_3.v1"
#define P2_SEMANTIC_GENERALIZED_ESD_NIST  "astrocs.generalized_esd_nist.v1"
#define P2_SEMANTIC_RCR_2_4_7_SS_MEDIAN_DL "astrocs.rcr_2_4_7_ss_median_dl.v1"
#define P2_SEMANTIC_PERCENTILE_SIRIL      "astrocs.percentile_siril.v1"
#define P2_SEMANTIC_MEDIAN_STD_CLIP       "astrocs.median_std_clip.v1"
#define P2_SEMANTIC_MINMAX                "astrocs.minmax.v1"
#define P2_SEMANTIC_LARGE_SCALE           "astrocs.large_scale_rejection.v1"

// per-sample reason（RejectionDecision.reasons[]）
enum P2RejectReason {
    P2_REASON_ACCEPTED = 0,
    P2_REASON_REJECTED_LOW = 1,    // 低于 lower rejection threshold
    P2_REASON_REJECTED_HIGH = 2,   // 高于 upper rejection threshold
    P2_REASON_UNDERDETERMINED = 3  // 样本数不足，未做拒绝判定（全接受）
};

// stack-level status（与 per-sample reason 分离）
enum P2RejectStatus {
    P2_STATUS_OK = 0,
    P2_STATUS_MIN_SAMPLES = 1,       // 兼容旧语义：候选数 < 显式 min_samples
    P2_STATUS_ALL_REJECTED = 2,
    P2_STATUS_INVALID_INPUT = 3,     // 候选栈含非 finite（资格层后不应出现）
    P2_STATUS_UNDERDETERMINED = 4,   // n <= underdetermined_n 或 n < min N
    P2_STATUS_INVALID_CONFIGURATION = 5, // 方法×normalization 组合不合法
    P2_STATUS_INVALID_METHOD = 6,        // AUTO 等非法方法进入 kernel
    P2_STATUS_INTERNAL_ERROR = 7         // kernel 内部不变量破坏
};

// RejectionNormalizationPolicy（判定工作域；mask 应用回原始科学值）
enum P2RejectionNormalization {
    P2_NORMALIZE_NONE = 0,          // identity 工作域
    P2_NORMALIZE_MEDIAN_CENTER = 1, // working = value - median（per-pixel）
    P2_NORMALIZE_MEDIAN_SCALE = 2   // working = value / max(|median|, floor)
};

// ---- method-specific typed parameters（禁止跨方法共享 low/high/max_iter） ----
typedef struct {
    double lower_sigma;    // 低侧 σ 阈值（正数；默认 4.0）
    double upper_sigma;    // 高侧 σ 阈值（正数；默认 3.0）
    int max_iterations;    // 默认 8
} P2SigmaParams;           // robust_mad_clip / winsorized / averaged / median_sigma

typedef struct {
    double lower;          // 低侧因子（默认 4.0）
    double upper;          // 高侧因子（默认 3.0）
    int max_iterations;    // 默认 8
} P2LinearFitParams;

typedef struct {
    double alpha;          // 显著性水平（默认 0.05）
    int max_outliers;      // 最大离群数（默认 10）
} P2EsdParams;

typedef struct {
    double low_fraction;   // 相对 median 低侧小数（默认 0.1 = 10%）
    double high_fraction;  // 相对 median 高侧小数（默认 0.1）
} P2PercentileParams;

typedef struct {
    int reject_low_count;  // 一次性剔除最低样本数（默认 1）
    int reject_high_count; // 一次性剔除最高样本数（默认 1）
    int min_kept;          // 剔除后至少保留样本数（默认 4）
} P2MinmaxParams;

typedef struct {
    int technique;         // 0 = SS_MEDIAN_DL（ 冻结，唯一支持）
} P2RcrParams;

// astrocs.large_scale_rejection.v1 —— 大尺度结构拒绝（WBPP
// Large-Scale Pixel Rejection 的 AstroCS 自有实现，PIXINSIGHT_EXACT=
// NOT_CLAIMED）。语义：对每帧 pixel-level rejection mask 做
// connected-component grow：
// - 8-连通分量中，只有分量大小 >= min_structure_pixels 的结构才被
// 视为大尺度（compact cosmic / 星点噪声不会无限生长）；
// - 合格结构按 Chebyshev 邻域扩张 grow_radius 像素，新增像素同样
// 标记 rejected（低/高侧独立半径）；
// - 扩张后 mask 应用回原始 calibrated 科学值（与 pixel rejection
// 同一 accepted mask 语义）。
// 默认值（WBPP 2.9.1 largeScaleClipLow/High 默认关闭 → 默认 enabled=0）：
// min_structure_pixels=8；low/high grow radius=2。
typedef struct {
    int enabled;                  // 0/1（默认 0）
    int min_structure_pixels;     // 结构最小像素数（>=1；默认 8）
    int low_grow_radius_pixels;   // 低侧扩张半径（>=0；默认 2）
    int high_grow_radius_pixels;  // 高侧扩张半径（>=0；默认 2）
} P2LargeScaleParams;

// 显式 RejectionPlan（kernel 只执行 explicit method，永不为 AUTO）
typedef struct {
    int method;                // P2RejectionMethod（explicit）
    int minimum_n;             // 方法注册表 minimum N（不足 → UNDERDETERMINED）
    std::uint32_t underdetermined_n; // n <= 该值 → UNDERDETERMINED（默认 2）
    int normalization;         // P2RejectionNormalization（默认 MEDIAN_CENTER）
    double normalization_floor; // MEDIAN_SCALE 的最小 |median|（默认 1e-12）
    // typed params（仅对应 method 的成员有意义）
    P2SigmaParams sigma;       // P2_REJECT_SIGMA
    P2SigmaParams winsorized;  // P2_REJECT_WINSORIZED_SIGMA
    P2SigmaParams averaged;    // P2_REJECT_AVERAGED_SIGMA
    P2LinearFitParams linear_fit; // P2_REJECT_LINEAR_FIT
    P2EsdParams esd;           // P2_REJECT_GENERALIZED_ESD
    P2PercentileParams percentile; // P2_REJECT_PERCENTILE
    P2SigmaParams median_sigma;    // P2_REJECT_MEDIAN_SIGMA
    P2MinmaxParams minmax;     // P2_REJECT_MINMAX
    P2RcrParams rcr;           // P2_REJECT_RCR
    P2LargeScaleParams large_scale; // 大尺度后处理（独立于 pixel kernel）
} P2RejectionPlan;

// Auto 解析请求（planning 层）
typedef struct {
    int request;                 // P2RejectionMethod（允许 AUTO）
    std::uint32_t nominal_contributors; // wbpp_current = integration group
                                       // active independent exposure 数
                                       // （一次解析）；astrocs_adaptive =
                                       // tile nominal geometric depth
    const char* profile;         // "wbpp_current"（nullptr=默认）| "astrocs_adaptive"
    std::uint32_t underdetermined_n;   // 默认 2
} P2RejectionPlanRequest;

// 在 planning 层把 request（含 AUTO）解析为显式 P2RejectionPlan。
// WBPP 2.9.1（本机安装源码 bestRejectionMethod）Auto 路由：
// nominal < 6 → percentile；6..15 → winsorized_sigma；>15 → linear_fit。
// profile 语义：
// wbpp_current → 调用方必须传 integration group active count，一次
// 解析；tile/pixel 不重选；局部候选不足 = UNDERDETERMINED。
// astrocs_adaptive → AstroCS 自有策略：允许按 tile nominal geometric depth
// 自适应；独立命名，不冒充 WBPP exact。
// err 仅作日志文本；返回 0=OK，非 0=非法参数（err 填充原因）。
P2_API int p2_reject_plan_resolve(const P2RejectionPlanRequest* req,
                                  P2RejectionPlan* plan,
                                  char* err, std::size_t err_cap);

// 返回方法的 canonical semantic id 字符串（未知方法返回 "unknown"）
P2_API const char* p2_rejection_semantic_id(int method);

// ---- Eligibility（资格层，单一路径） ----
typedef struct {
    const double* values;        // 原始 contributors（UPM-calibrated）
    const double* weights;       // 可空（等权）；随样本携带到候选栈
    const std::uint8_t* valid;   // 可空（全部有效）
    const double* support;       // 可空（不检查）
    const std::uint32_t* quality; // 可空（不检查）
    std::uint32_t count;
    double support_threshold;          // support > 该值才合格（默认 0.0）
    std::uint32_t quality_flags_required; // 0 = 不要求 quality
} P2EligibilityInput;

typedef struct {
    double* values;              // 输出（容量=count；合格值紧凑写入）
    double* weights;             // 输出（容量=count；输入 weights 为 null 时保持
                                 // 未写，调用方按等权处理）
    std::uint8_t* eligible;      // 每输入样本 1=合格 0=不合格（容量=count）
    std::uint32_t* eligible_count;
    std::uint32_t invalid_finite;    // 诊断：非 finite 计数
    std::uint32_t invalid_valid;     // 诊断：valid=0 计数
    std::uint32_t invalid_support;   // 诊断：support 不合格计数
    std::uint32_t invalid_quality;   // 诊断：quality 不合格计数
} P2EligibilityOutput;

P2_API int p2_eligibility_filter(const P2EligibilityInput* in,
                                 P2EligibilityOutput* out);

// 生产收集器（frame-major strided 输入，一次完成资格判定+紧凑化）。
// stage2 CPU / ACR 使用同一函数（单一路径）；compat 走连续版（同一 policy）。
typedef struct {
    const void* values;              // 必填（frame-major；dtype 见 value_dtype）
    std::size_t value_stride;
    const void* weights;             // 可空（与 values 同 dtype）
    std::size_t weight_stride;
    const std::uint8_t* valid;       // 可空
    std::size_t valid_stride;
    const void* support;             // 可空（与 values 同 dtype）
    std::size_t support_stride;
    const std::uint32_t* quality;    // 可空（像素级 quality；当前数据模型为
                                     // control 级，stage2 传 nullptr 并记录）
    std::size_t quality_stride;
    const std::uint64_t* frame_ids;  // 可空（紧凑，帧序一一对应）
    std::uint32_t count;
    std::uint32_t pixel;
    double support_threshold;        // 默认 0.0
    std::uint32_t quality_flags_required; // 默认 0
    int value_dtype;                 // 0=fp32, 1=fp64（values/weights/support）
} P2EligibilityGatherInput;

typedef struct {
    double* values;                  // 输出紧凑候选（容量 count）
    double* weights;                 // 可空（容量 count）
    double* support;                 // 可空（容量 count）
    std::uint64_t* frame_ids;        // 可空（容量 count）
    // （PHASE2_IVAR_WIRING）：可空；eligible_index → 原始输入 slot。
    // compact 后禁止用 compact index 猜 original slot（ivar/quality/
    // variance/metadata 一律经此映射）。
    std::uint32_t* source_indices;
    std::uint32_t* eligible_count;
    std::uint32_t invalid_finite;
    std::uint32_t invalid_valid;
    std::uint32_t invalid_support;
    std::uint32_t invalid_quality;
} P2EligibilityGatherOutput;

P2_API int p2_collect_candidate_stack(const P2EligibilityGatherInput* in,
                                      P2EligibilityGatherOutput* out);

// ---- CandidateStack（资格层产物；kernel 输入） ----
typedef struct {
    const double* values;        // eligible 样本（紧凑）
    const double* weights;       // 可空
    const std::uint64_t* frame_ids; // 可空（稳定帧标识；tie-break/确定性用）
    std::uint32_t count;         // 候选数
    int data_type;               // 0=fp32 源, 1=fp64（仅诊断）
} P2CandidateStack;

// ---- RejectionDecision（每样本 reason + stack status 分离） ----
typedef struct {
    std::uint8_t* reasons;       // count 字节（调用方分配；P2RejectReason）
    std::uint32_t accepted_count;
    std::uint32_t rejected_low;  // 低于 lower threshold
    std::uint32_t rejected_high; // 高于 upper threshold
    std::uint32_t iterations;
    int status;                  // P2RejectStatus
} P2RejectionDecision;

// 执行显式 RejectionPlan（plan->method 必须为 explicit；AUTO 返回非法参数）。
// kernel 内 n<=64 使用固定 scratch（无每像素堆分配）；>64 走堆。
P2_API int p2_reject_stack_ex(const P2CandidateStack* stack,
                              const P2RejectionPlan* plan,
                              P2RejectionDecision* out);

// 大尺度 grow 后处理（生产 stage2 唯一调用点）。
// low/high 为 frame-major 每帧 width*height 字节（1=rejected），原地修改。
// 仅扩张"分量大小 >= min_structure_pixels"的结构；低/高侧独立半径。
// 返回 0=OK；参数非法返回 1（err 可空）。
P2_API int p2_large_scale_apply(std::uint8_t* low, std::uint8_t* high,
                                int width, int height, int depth,
                                const P2LargeScaleParams* params);

// ---- 旧接口（COMPAT adapter，仅测试/旧调用；生产 Stage2 不再调用） ----
typedef struct {
    const double* values;
    const std::uint8_t* valid;      // 可空（全部有效）
    const double* support;          // 可空
    const double* weights;          // 可空（等权）
    const std::uint32_t* quality;   // 可空
    const std::uint64_t* frame_ids; // 可空（稳定帧标识）
    std::uint32_t count;
    int  data_type;                 // 0=fp32, 1=fp64
    int  method;                    // P2RejectionMethod
    double sigma_low;               // 兼容：见 typed params
    double sigma_high;              // 兼容
    int  max_iterations;            // 兼容
    int  min_samples;               // 兼容：候选数 < 该值 → status=MIN_SAMPLES
} P2SampleStackView;

typedef struct {
    std::uint8_t* accepted;         // 输出掩码（count 字节，调用方分配）
    std::uint32_t accepted_count;
    std::uint32_t rejected_low;
    std::uint32_t rejected_high;
    std::uint32_t iterations;
    int status;                     // P2RejectStatus（兼容旧 0..4 数值）
} P2RejectionResult;

P2_API int p2_reject_stack(const P2SampleStackView* in, P2RejectionResult* out);

#ifdef __cplusplus
}
#endif
