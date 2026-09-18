// lib/algorithms/coverage/include/astro/phase2/rejection.h
//
// Phase2 Rejection Framework 公共接口。
// Trace: ALG-P2-REJ-001（TRACEABILITY_MATRIX MOD-astrocs-phase2-reject 行）。
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
    P2_REJECT_AUTO = 10,            // 只在 planning 层解析，永不进入 kernel
    // 已知先验 σ 的极值检验（FIX-REJ §3 内置映射 n=2 档；见
    // P2ExtremeValuePriorSigmaParams）。显式方法：永不参与 AUTO 路由。
    P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA = 11
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
#define P2_SEMANTIC_EXTREME_VALUE_PRIOR_SIGMA \
    "astrocs.extreme_value_clip_prior_sigma.v1"
#define P2_SEMANTIC_LARGE_SCALE           "astrocs.large_scale_rejection.v1"

// 冻结 profile 名（planning 层路由选择器；kernel 只见显式方法）
#define P2_PROFILE_WBPP_2_9_1             "wbpp_2_9_1"
#define P2_PROFILE_WBPP_CURRENT           "wbpp_current"  // = wbpp_2_9_1 alias
#define P2_PROFILE_ASTROCS_ADAPTIVE       "astrocs_adaptive"
// FIX-REJ §3 AstroCS 自有「按几何 n」内置映射（逐输出像素；含 n=2 先验 σ 档）。
// 独立命名，不改变 wbpp_2_9_1 / astrocs_adaptive 的冻结 AUTO 路由。
#define P2_PROFILE_ASTROCS_ADAPTIVE_PIXEL "astrocs_adaptive_pixel"

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

// astrocs.extreme_value_clip_prior_sigma.v1 —— 已知先验 σ 的极值检验
// （NIST/SEMATECH e-Handbook §1.3.5.17.1 Grubbs / 已知方差单离群变体；
// 单趟、无迭代、无 N-r 最小保留闸）。FIX-REJ §3 内置映射的 n=2 档：小栈
// 无法估计稳健尺度，必须由调用方提供**先验**噪声尺度（方案 A：该帧该 tile
// 的 31×31 邻域中位数/MAD；kernel 不自己算邻域）。
//
//   z_i = (v_i − center_i) / sigma_i
//   k   = Φ⁻¹(1 − α/(2N))，α=0.05，N=Bonferroni 检验数（= 该像素几何
//         nominal contributors，见 P2RejectionPlan.nominal_n；0 时退化为
//         候选数 n）
//   z_i > +k → REJECTED_HIGH；z_i < −k → REJECTED_LOW；否则 ACCEPTED
//
// 先验来源优先级：
//   1) P2CandidateStack.prior_sigma[i] / prior_sky[i]（逐样本，可空）
//   2) 本结构 prior_sigma / prior_sky（标量回退）
//   3) prior_sky 缺失且 center_mode!=0 → 候选栈中位数（**注意**：n=2 时
//      栈中位数落在两样本之间，单离群会使两侧同时超阈 → 全拒 → 走 n<=4
//      全接受容错；生产必须提供外部 prior_sky）。
// prior_sigma 缺失/非有限/<=0 → UNDERDETERMINED 全接受（fail-closed，
// 禁止用栈内尺度冒名顶替）。
// normalization 必须 = P2_NORMALIZE_NONE（本方法在原始 calibrated 值域
// 直接比较绝对 prior_sky；planning 层解析已保证）。
typedef struct {
    double alpha;         // 显著性水平（冻结默认 0.05）
    double prior_sigma;   // 先验噪声尺度（>0 且 finite；调用方提供）
    double prior_sky;     // 先验中心（非 finite → 用候选栈中位数）
    int    center_mode;   // 0=强制用 prior_sky；1=prior_sky 缺失时用栈中位数
                          // （默认 1）
} P2ExtremeValuePriorSigmaParams;

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
    P2ExtremeValuePriorSigmaParams extreme_prior; // P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA
    // 本 plan 解析时使用的**几何 nominal n**（coverage 覆盖图；路由依据，
    // 非存活数）。extreme_prior 的 Bonferroni N；0 = 未记录（用候选数）。
    std::uint32_t nominal_n;
} P2RejectionPlan;

// Auto 解析请求（planning 层）
typedef struct {
    int request;                 // P2RejectionMethod（允许 AUTO）
    std::uint32_t nominal_contributors; // wbpp_current = integration group
                                       // active independent exposure 数
                                       // （一次解析）；astrocs_adaptive =
                                       // tile nominal geometric depth
    const char* profile;         // P2_PROFILE_*（nullptr=wbpp_2_9_1）
    std::uint32_t underdetermined_n;   // 0=按 profile 默认（wbpp/adaptive=2；
                                       // astrocs_adaptive_pixel=1，因为 n=2 档
                                       // 由 extreme_prior 承担）
} P2RejectionPlanRequest;

// 在 planning 层把 request（含 AUTO）解析为显式 P2RejectionPlan。
// WBPP 2.9.1（本机安装源码 bestRejectionMethod）Auto 路由：
// nominal < 6 → percentile；6..15 → winsorized_sigma；>15 → linear_fit。
// profile 语义：
// wbpp_2_9_1（wbpp_current alias）→ 调用方必须传 integration group active
// count，一次解析；tile/pixel 不重选；局部候选不足 = UNDERDETERMINED。
// astrocs_adaptive → AstroCS 自有策略：允许按 tile nominal geometric depth
// 自适应；独立命名，不冒充 WBPP exact；AUTO 路由与 wbpp 冻结表一致。
// astrocs_adaptive_pixel → FIX-REJ §3 AstroCS 自有「按逐输出像素几何 n」
// 内置映射（n=2 走 extreme_value_clip_prior_sigma；3..7 percentile；
// 8..15 winsorized；>=16 linear_fit）；独立命名，不改变上述冻结路由。
// err 仅作日志文本；返回 0=OK，非 0=非法参数（err 填充原因）。
//
// 确定性：同 (profile, request, nominal_contributors, underdetermined_n)
// 恒等输出（纯函数、无状态、无随机）⇒ 同 n 方法选择确定性一致。
P2_API int p2_reject_plan_resolve(const P2RejectionPlanRequest* req,
                                  P2RejectionPlan* plan,
                                  char* err, std::size_t err_cap);

// 返回方法的 canonical semantic id 字符串（未知方法返回 "unknown"）
P2_API const char* p2_rejection_semantic_id(int method);

// ---- 逐 stack（逐输出像素/块）按几何 n 解析（FIX-REJ / DESIGN §4.5） ----
//
// nominal_n = 该 stack 的**几何 nominal contributors**（coverage 覆盖图：
// 该输出像素被多少帧 footprint 覆盖），一次解析；**不是**整组帧数，也
// **不是**资格/掩膜后的存活数 n_eff。
//
// p2_reject_plan_resolve_n 是 p2_reject_plan_resolve 的显式逐 stack 入口：
// 以 nominal_n 覆盖 req->nominal_contributors 后解析。纯函数（同输入恒等
// 输出，无状态、无随机）⇒ 调用方可按 n 缓存 plan（最多 n_max 个），
// 1 worker 与 N worker 结果一致。
P2_API int p2_reject_plan_resolve_n(std::uint32_t nominal_n,
                                    const P2RejectionPlanRequest* req,
                                    P2RejectionPlan* plan,
                                    char* err, std::size_t err_cap);

// FIX-REJ §4.2 方法适用域（WARN 级，advisory；不改判据、不阻断执行）。
// 返回 0 = (method, nominal_n) 适用；非 0 = 不适用，warn_code 填稳定码
// （"W_MINMAX"/"W_NONE"/"W_PCT_GT8"/... 见 rejection.cpp）。
// 仅 planning/CLI 用于分级提示（-y 跳过确认 / -force 强制执行）；
// kernel 执行不受本函数影响（显式指定一律照执行，不静默改算法）。
P2_API int p2_rejection_applicability(int method, std::uint32_t nominal_n,
                                      char* warn_code, std::size_t warn_cap);

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
    // （FIX-REJ n=2 先验 σ 档）可空；仅 P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA
    // 消费。逐样本先验噪声尺度/中心（与 values 同序；调用方从该样本所属
    // 帧/tile 的稳健邻域统计得到）。nullptr → 回退 plan.extreme_prior 标量。
    const double* prior_sigma;   // 先验 σ（>0 且 finite）
    const double* prior_sky;     // 先验中心
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

// 逐 stack 解析 + 执行（单次调用完成 planning→kernel）：
// req->nominal_contributors 必须是该 stack 的几何 n；req->request 允许
// AUTO（在本函数内于 planning 层解析，kernel 永不见 AUTO）。resolved_plan
// 可空；非空时回填解析结果（provenance）。返回 0=OK（out->status 表达
// 科学状态）；非 0=参数非法（err 填充原因）。
P2_API int p2_reject_stack_resolve_ex(const P2CandidateStack* stack,
                                      const P2RejectionPlanRequest* req,
                                      P2RejectionDecision* out,
                                      P2RejectionPlan* resolved_plan,
                                      char* err, std::size_t err_cap);

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

// =====================================================================
// V6 分类排异（ALG-P2S-REJ.1..7；DESIGN-P2-001 §5）
//
// 语义：
// - 阈值使用**预测残差方差** sigma_eff^2 = sigma_phase1^2 + J C_theta J^T
//   （单位：signal ADU/px^2，variance ADU^2/px^4）。禁止用裸原始残差
//   （未含 UPM 参数不确定度）作阈值（ALG-P2S-REJ.3，fail-closed）。
// - 每样本输出 reason（4 继承：accepted/rejected_low/rejected_high/
//   underdetermined）+ reason_class（6 污染类，与拒绝方向正交，
//   ADJ-GEN-02）+ probability（仅门/推断，禁止进权重面）。
// - 继承阈值（sigma 4.0/3.0/8、linear_fit 5.0/3.5/8、percentile 0.2/0.1、
//   ESD alpha 0.05/max 10、large_scale 8/2/2 等）原样继承 ALG-REJ-001，
//   本层不得改动（FZ-REJ-INHERITED-THRESH）。
// =====================================================================

// reason_class：污染机制（与 reason 方向字段分别落在两个字段）。
enum P2RejectClass {
    P2_CLASS_NONE = 0,            // 未归入任何污染类
    P2_CLASS_COSMIC_RAY = 1,      // 宇宙线/热像素（单帧紧凑，不生长）
    P2_CLASS_SATELLITE_TRAIL = 2, // 卫星线/拖线（大尺度结构生长）
    P2_CLASS_BAD_COLUMN = 3,      // 坏列/坏像素（固定列坐标跨帧一致）
    P2_CLASS_MOVING_SOURCE = 4,   // 移动源（科学信号，默认保留独立层）
    P2_CLASS_CLOUD_GRADIENT = 5,  // 云/梯度（低频残差与 UPM 背景失配）
    P2_CLASS_DEFOCUS_TRAIL = 6    // 失焦/拖线（PSF 形状/集中度失配）
};
#define P2_REJECT_CLASS_COUNT 7

#define P2_CLASS_SEMANTIC_NONE        "none"
#define P2_CLASS_SEMANTIC_COSMIC_RAY  "cosmic_ray"
#define P2_CLASS_SEMANTIC_SAT_TRAIL   "satellite_trail"
#define P2_CLASS_SEMANTIC_BAD_COLUMN  "bad_column"
#define P2_CLASS_SEMANTIC_MOVING_SRC  "moving_source"
#define P2_CLASS_SEMANTIC_CLOUD_GRAD  "cloud_gradient"
#define P2_CLASS_SEMANTIC_DEFOCUS     "defocus_trail"

// 噪声模型声明位：sigma_eff 必须同时含 Phase1 噪声与 UPM 参数不确定度，
// 否则 fail-closed（P2_STATUS_INVALID_INPUT）。
#define P2_NOISE_PHASE1_DECLARED 0x1u
#define P2_NOISE_UPM_DECLARED    0x2u
#define P2_NOISE_REQUIRED        (P2_NOISE_PHASE1_DECLARED | P2_NOISE_UPM_DECLARED)

// 校准门数值（ALG-P2S-REJ.4；状态 PENDING_OWNER_SIGNOFF SO-07）。
// fail-closed：按文档值实现，未签字生效前不得放宽（不得改成更松的值）。
#define P2_REJ_CALIB_BINMIN   50u
#define P2_REJ_CALIB_ABS      0.10
#define P2_REJ_CALIB_BSS_MIN  0.10

// 分类 profile 版本（方法/证据门/后验参数版本化，宪章 §6.3）。
#define P2_REJECT_CLASSIFY_PROFILE "astrocs.rejection.classify.v1"
// v1 profile 常量（未版本化改动 → invalid_configuration）
#define P2_REJ_PROFILE_MOTION_MIN_PX  0.5
#define P2_REJ_PROFILE_PSF_ANOMALY_MIN 0.2
#define P2_REJ_PROFILE_CONTAM_PRIOR   0.05
#define P2_REJ_PROFILE_OUTLIER_KAPPA  4.0
#define P2_REJ_PROFILE_BIN_COUNT      10u

// 返回污染类 canonical id（未知→"unknown"）。
P2_API const char* p2_rejection_class_id(int reason_class);

// 校验 plan 的继承阈值（ALG-REJ-001）未被改动：
// 返回 1 = 全部继承值一致；0 = 任一被改动（调用方 → invalid_configuration）。
P2_API int p2_reject_plan_thresholds_inherited(const P2RejectionPlan* plan);

// 权重面守卫：tokens 为将写入 weight.sources/weight_value/variance_from 的
// 来源名。命中禁止 token（rejection/probability/support/coverage/median SN
// R/FWHM/residual/psfsw）或延迟/legacy 模式值（psf_snr_power/auto/
// support_x_snr2/0）→ 返回非 0（REJECT）。
P2_API int p2_rejection_weight_surface_guard(const char* const* tokens,
                                             std::size_t count,
                                             char* err, std::size_t err_cap);

// ---- 分类排异配置（阈值必须继承；profile 常量版本化） ----
typedef struct {
    P2RejectionPlan plan;        // method 必须 explicit；阈值必须继承值
    const char* profile_version; // 必须 = P2_REJECT_CLASSIFY_PROFILE
    double motion_min_px;        // 移动源分类门（v1=0.5 px）
    double psf_anomaly_min;      // 失焦/拖线 PSF 形状异常门（v1=0.2，无量纲）
    double contamination_prior;  // 后验先验 P(污染)（v1=0.05）
    double outlier_inflation;    // 后验污染膨胀 kappa（v1=4.0）
    int keep_moving_source;      // 1=移动源保留独立层、不进删除 mask（默认 1）
} P2RejectClassifyConfig;

// 每样本证据（判据域；调用方从像素邻域/跨帧/UPM 残差计算）。
// residual/sigma_phase1 单位 ADU/px^2；upm_variance 单位 ADU^2/px^4。
typedef struct {
    const double* residual;          // r = d - model
    const double* sigma_phase1;      // sigma_phase1
    const double* upm_variance;      // J C_theta J^T（标量对角）
    const std::uint8_t* noise_flags; // P2_NOISE_* 位（每样本）
    const std::uint8_t* large_scale_growth;   // 可空（1=结构生长）
    const std::uint8_t* compact_single_frame; // 可空（1=单帧紧凑）
    const std::uint8_t* column_consistent;    // 可空（1=固定列一致）
    const double* cross_frame_motion;         // 可空（px，单调位移幅度）
    const std::uint8_t* low_frequency;        // 可空（1=低频/UPM 背景失配）
    const double* psf_shape_anomaly;          // 可空（无量纲相对偏差）
    std::uint32_t count;
} P2RejectClassifyInput;

typedef struct {
    std::uint8_t* reasons;         // P2RejectReason（容量 count）
    std::uint8_t* reason_classes;  // P2RejectClass（容量 count）
    std::uint8_t* deleted;         // 1=进入排异删除（容量 count）
    std::uint8_t* preserved;       // 1=移动源保留独立层（容量 count）
    double* sigma_eff;             // sqrt(sigma_phase1^2+UPM)（容量 count）
    double* z;                     // r/sigma_eff（容量 count）
    double* probability;           // p in [0,1]（容量 count）
    double* class_probability;     // count × P2_REJECT_CLASS_COUNT（行主序）
    std::uint32_t accepted_count;
    std::uint32_t rejected_low;
    std::uint32_t rejected_high;
    double recall;                 // 拒绝率；n<=2 全接受 → 0.0 显式
    int status;                    // P2RejectStatus
} P2RejectClassifyOutput;

// 分类排异主入口（确定性：固定序，无随机，无堆分配依赖顺序）。
P2_API int p2_reject_classify(const P2RejectClassifyInput* in,
                              const P2RejectClassifyConfig* cfg,
                              P2RejectClassifyOutput* out);

// ---- probability 校准门（ALG-P2S-REJ.4） ----
enum P2RejectCalibrationStatus {
    P2_CALIB_OK = 0,
    P2_CALIB_INSUFFICIENT_SAMPLES = 1, // 无箱达到 BINMIN → 不可评估
    P2_CALIB_RELIABILITY_FAIL = 2,     // max|obs-mean(p)| > ABS
    P2_CALIB_BSS_FAIL = 3,             // BSS <= BSS_MIN（或 BS_ref 退化）
    P2_CALIB_INVALID_INPUT = 4
};

typedef struct {
    int status;                        // P2RejectCalibrationStatus
    double brier;                      // BS = mean((p-y)^2)
    double brier_ref;                  // BS_ref = 基础率常数预测
    double bss;                        // BSS = 1 - BS/BS_ref
    double max_abs_reliability_dev;    // 合格箱 max|obs-mean(p)|
    std::uint32_t bins_total;
    std::uint32_t bins_used;           // n_bin >= BINMIN
    std::uint32_t bins_skipped_small;  // n_bin in (0,BINMIN)：覆盖须登记
    std::uint32_t used_samples;        // 参与判定样本
    int probability_is_scientific_gate; // 1 当且仅当 status==OK
} P2RejectCalibrationOutput;

// 在预注册污染注入集上评 probability 校准（训练/验收样本须不同）。
P2_API int p2_reject_calibration(const double* probability,
                                 const std::uint8_t* truth,
                                 std::uint32_t count,
                                 std::uint32_t bin_count,
                                 P2RejectCalibrationOutput* out);

#ifdef __cplusplus
}
#endif
