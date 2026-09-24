// lib/algorithms/coverage/include/astro/phase2/sky_plane.h
//
// Phase2 稀疏天光面（sky plane）公共接口。
//
// 权威：
//   ASTROCS_DESIGN.md §4.4（天光亮度平面稀疏表示、按需求值）
//   docs/plugins/algorithms_phase2/10_sampling.md（star_mask / sky_samples / 点权重）
//   docs/plugins/algorithms_phase2/11_upm.md §4.1-§4.4（b_k(x)=B_ref(x)+δ_k(x)、SNR 加权最小 RMS）
//   docs/design/UNIFIED_MODEL.md §2（sky_samples / sky_plane 数据对象）
//
// 模型（加性天光面，与 11_upm.md §4.1 一致）：
//     y_k(x) = b_k(x) + ε_k(x)
//     b_k(x) = B_ref(x) + δ_k(x)
//   B_ref  全帧联合参考天光面，稀疏二维张量积 B 样条系数（不建稠密栅格）；
//   δ_k    每帧相对参考面的平缓梯度（低阶多项式，阶数 frame_gradient_order）。
//
// 表示与内存：
//   - B_ref 只存 (nx*ny) 个样条系数；δ_k 只存每帧 m=(p+1)(p+2)/2 个多项式系数；
//   - 求解时按帧 Schur 消元（profile out δ_k），约化正规矩阵只有 nx*ny 阶，
//     **内存与帧数无关**，也不随像素数增长；像素值一律现场求值（eval/eval_block）；
//   - 稀疏持久化：save/open 只写系数与网格参数。
//
// 权重（11_upm.md §4.4）：加权最小二乘，w_i = 1/σ_i²（inverse_variance，canonical）
// 或显式 w_i ∝ SNR_i²（snr2）；稳健 IRLS（Huber）抑制离群采样点。
//
// gauge（11_upm.md §4.3）：reference_frame（参考帧 δ≡0，参考帧=最小 frame_id，与
// UPM MA 同构）或 sum（Σ_k δ_k ≡ 0，由 reference_frame 解作常数平移得到，平移量
// 记入 gauge_shift）。
//
// fail-closed（11_upm.md §4.5）：无采样点 / 全被掩膜 / 点不足 / 帧欠定 /
// 节点数超上限 / 秩亏 / κ 超门 / 非有限解 一律显式失败；求值越域不外插。
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

// ===========================================================================
// 1. 天光采样点与星点掩膜
// ===========================================================================

// 天光采样点标记位。masked/rejected 点不进入天光面拟合。
enum {
    P2_SKY_FLAG_NONE               = 0u,
    P2_SKY_FLAG_MASKED             = 1u << 0,  // 落在星点/饱和/高结构掩膜内
    P2_SKY_FLAG_LOW_SUPPORT        = 1u << 1,  // 有效像素/support 不足
    P2_SKY_FLAG_HIGH_CONTAMINATION = 1u << 2,  // 亮像素占比过高
    P2_SKY_FLAG_NO_LOCAL_SNR       = 1u << 3,  // 无帧内 SNR，snr 用帧级回退
    P2_SKY_FLAG_REJECTED           = 1u << 4   // 稳健迭代剔除
};

// 每帧稀疏天光采样点（10_sampling.md §3/§4.2）。
// value/variance 单位 ADU / ADU²；snr 为点 SNR（帧级 × 帧内，缺帧内时=帧级）。
typedef struct {
    std::uint64_t frame_id;
    std::uint64_t control_id;   // 采样点拓扑 id（同一 id 跨帧为同一空间位置）
    double ra_deg;
    double dec_deg;
    double value;               // 局部稳健背景估计（可负）
    double variance;            // Var(value)，必须 >0 有限方可用于拟合
    double snr;                 // 点 SNR（>0；<=0 表示未知）
    std::uint32_t flags;        // P2_SKY_FLAG_*
} P2SkySample;

// 星点/饱和/高结构掩膜：球面圆帽（随控制点集分发）。
enum {
    P2_STAR_MASK_STAR           = 0u,
    P2_STAR_MASK_SATURATION     = 1u,
    P2_STAR_MASK_HIGH_STRUCTURE = 2u
};

typedef struct {
    double ra_deg;
    double dec_deg;
    double radius_deg;
    std::uint32_t kind;   // P2_STAR_MASK_*
} P2StarMaskCap;

// 局部稳健背景估计（纯函数，无 I/O；与 sampler 的 patch estimator 同一数学定义：
// median + 1.4826*MAD 尺度 + 亮端 σ-clipping + 保留比例门）。
typedef struct {
    int    min_samples;              // 默认 5
    int    clip_iters;               // 默认 3
    double clip_sigma;               // 默认 3.0（MAD 单位，亮端单侧）
    double contamination_sigma;      // 默认 3.0
    double min_retained_fraction;    // 默认 0.60
    double k_corr;                   // 默认 1.4（Drizzle 相关放大，冻结保守值）
} P2SkyPatchConfig;

typedef struct {
    double value;            // 稳健中位数
    double sigma_mad;        // 1.4826*MAD（>0）
    double variance;         // k_corr*(π/2)*sigma_mad²/N_retained
    double ivar;             // 1/variance（variance<=0 → 0）
    double uncertainty;      // sqrt(variance)
    std::int32_t n_total;    // 有效像素数
    std::int32_t n_retained; // clipping 后保留数
    double bright_fraction;  // > value + contamination_sigma*sigma 的占比
    std::int32_t status;     // P2SkyPatchStatus
} P2SkyPatchEstimate;

enum {
    P2_SKY_PATCH_OK                    = 0,
    P2_SKY_PATCH_INSUFFICIENT_SAMPLES  = 1,
    P2_SKY_PATCH_INSUFFICIENT_RETAINED = 2,
    P2_SKY_PATCH_HIGH_CONTAMINATION    = 3,
    P2_SKY_PATCH_INVALID_ARGS          = 4
};

P2_API P2SkyPatchConfig p2_sky_patch_default_config(void);

// values/valid 可空（valid 空=全部有效）。valid[i]==0 或非有限值跳过。
P2_API int p2_sky_patch_estimate(const double* values, const std::uint8_t* valid,
                                 std::uint64_t n, const P2SkyPatchConfig* cfg,
                                 P2SkyPatchEstimate* out,
                                 char* err, std::size_t err_size);

// 由星表（ra/dec/snr）构造掩膜圆帽；snr > snr_threshold 的星按 radius_deg 膨胀。
// out 可空（查询容量）；返回 0=ok（out_n 写真实需求），1=参数错误。
P2_API int p2_star_mask_caps(const double* ra_deg, const double* dec_deg,
                             const double* snr, std::uint64_t n,
                             double snr_threshold, double radius_deg,
                             P2StarMaskCap* out, std::uint64_t cap,
                             std::uint64_t* out_n);

// 点 (ra,dec) 是否落在任一圆帽内（含边界）。1=命中，0=未命中，-1=参数错误。
P2_API int p2_star_mask_contains(const P2StarMaskCap* caps, std::uint64_t n,
                                 double ra_deg, double dec_deg);

// ===========================================================================
// 2. 稀疏天光面求解
// ===========================================================================

// ---------------------------------------------------------------------------
// 2a. 节点间距的输入自适应导出（SCI-UPM-CAP-001；docs/science/PHASE2_UPM.md §7a）
// ---------------------------------------------------------------------------
//
// 规范依据（§7a「节点间距必须由输入自适应导出（不得是标定常数）」）：
//   节点间距 ≤ (该产品实际约束帧间改正量的最小尺度) / 2；
//   对「帧间加性天光差」这一目标量，该尺度是**重叠带宽度**与**指向间距**中的较小者。
//
// 为什么不能取标定常数：节点间距是 B_ref **表示能力的唯一决定量**（§7a 首条），
// 而「约束 δ_k 的最小尺度」是**逐产品**由输入几何决定的。取固定值会在几何不同的
// 产品上要么粗到不可表示（M42 实测：1.0° vs 需要 0.075°，见 run/UPM-NODE-ADAPT-01），
// 要么细到欠定。
//
// 量纲与换算（逐项；换仪器/换像素尺度后行为必须自洽）：
//   overlap_band_width_deg  度。相邻指向公共成像区在**指向连线方向**上的角宽度。
//   pointing_spacing_deg    度。相邻指向中心之间的天球角距离。
//   sample_pitch_deg        度。采样点（control）角间距 = 数据自身的分辨率极限：
//                           比它更细的节点不可能被数据约束。由 sampler 几何算出：
//                           Δθ = tile 角边长 / control_grid_per_tile（PHASE2_SAMPLER §…）。
//   pixel_scale_arcsec      角秒/像素。源帧像素角尺度。**只**用于把导出的角量换算到
//                           像素域（h_px = h_deg·3600 / pixel_scale_arcsec）与给出像素级
//                           分辨率极限；不参与 h_deg 的取值。故 h_deg 与像素尺度无关、
//                           h_px 与像素尺度成反比——这正是「换仪器后度数不变、像素数按
//                           比例变」的自洽性要求。
typedef struct {
    double overlap_band_width_deg;
    double pointing_spacing_deg;
    double sample_pitch_deg;
    double pixel_scale_arcsec;
} P2SkyPlaneGeometry;

// 导出结果。全部字段都是**由输入几何算出**的量，不含任何标定常数。
typedef struct {
    double constraining_scale_deg;    // min(重叠带宽度, 指向间距)：真正约束 δ_k 的窄尺度
    double upper_deg;                 // 规则 1 上界 = constraining_scale_deg / 2
    double lower_deg;                 // 下界 = 数据自身分辨率极限 = max(sample_pitch_deg, 像素角尺度)
    double node_spacing_deg;          // 导出节点间距（= upper_deg；自适应从它向 lower_deg 细化）
    double representable_scale_deg;   // 2·h：B_ref 可表示的最小尺度（§7a）
    double pixel_scale_deg;           // pixel_scale_arcsec / 3600（供调用方复核换算）
    double node_spacing_px;           // h 换算到源像素：h_deg·3600 / pixel_scale_arcsec
    double representable_scale_px;    // 2h 的源像素数
    double lower_px;                  // 下界的源像素数
} P2SkyPlaneNodeSpacing;

enum {
    P2_SKY_NODE_SPACING_OK = 0,
    P2_SKY_NODE_SPACING_INVALID_ARGS = 1,          // 几何量缺失/非有限/非正
    P2_SKY_NODE_SPACING_GEOMETRY_UNSUPPORTED = 2   // 规则上界 < 数据分辨率极限：无可采纳节点间距
};

// 由输入几何导出节点间距。**纯函数**（无 I/O、无全局状态），可单测。
// 返回 P2_SKY_NODE_SPACING_*；out 可空（只做合法性检查）。
// 失败时 err 写明缺了哪个量纲，**不**回退任何常数。
P2_API int p2_sky_plane_derive_node_spacing(const P2SkyPlaneGeometry* geom,
                                            P2SkyPlaneNodeSpacing* out,
                                            char* err, std::size_t err_size);

// 天光面配置（默认值见 p2_sky_plane_default_config）。
typedef struct {
    int    spline_degree;         // B_ref 样条阶数：1（双线性）或 3（双三次）；默认 1
    // B_ref 节点间距（切平面角度，度）。
    //   >0：调用方显式给定（**不再有默认标定值**，§7a 禁止配置里留「默认节点间距」）；
    //   <=0：未显式给出 ⇒ 由 geometry 字段经 p2_sky_plane_derive_node_spacing 导出；
    //        几何量缺失 ⇒ p2_sky_plane_build 返回 P2_SKY_PLANE_GEOMETRY_REQUIRED
    //        （**显式失败**，禁止回退常数）。
    double node_spacing_deg;      // 默认 0.0 = 未给出（由输入导出）
    int    frame_gradient_order;  // δ_k 阶数：0=偏移 1=平面 2=二次；默认 1
    double roughness_penalty;     // B_ref 二阶差分粗糙度惩罚 λ；默认 1e-3
    double huber_delta;           // 稳健 Huber δ；默认 1.345
    int    max_iterations;        // 稳健 IRLS 外层迭代上限；默认 30
    double tolerance;             // 收敛门（max|ΔB| 相对量）；默认 1e-10
    int    gauge_mode;            // 0=reference_frame；1=sum_zero；默认 0
    int    weight_mode;           // 0=inverse_variance；1=snr2；默认 0
    // κ 门控上限。**默认 0.0 = 由矩阵谱自身派生**（= 1/rank_rtol，见下），
    // 不再是标定常数（§7a「κ 上限不得是两个不同的标定值」）。>0 时表示调用方
    // 显式覆盖，门控来源会记入 info.kappa_max_source 与 provenance。
    double kappa_max;             // 默认 0.0 = 由 rank_rtol 派生（=1/rank_rtol）
    double rank_rtol;             // 奇异值相对门；默认 1e-10（κ 门与秩判据**同一口径**）
    std::int32_t min_samples;     // 拟合最少有效采样点；默认 8
    std::int32_t min_samples_per_frame;  // 每帧 δ_k 可辨识最少点；默认 4
    std::int32_t max_nodes;       // B_ref 系数上限（内存门）；默认 8192
    double max_extrapolation_deg; // 求值允许越域余量；默认 0（不外插）
    // 注：本求解（按帧 Schur 消元 + 稳健 IRLS，整面一次）内在串行，无并行路径，
    // 故**不设 worker 数字段**——原「预留 cpu_workers=1」是零消费者的死字段，
    // 且其字面量默认值违反 QA-002/P2-002（生产禁止 workers=1 硬编码）。按
    // docs/standards/CONCURRENCY_STANDARD.md「线程数外部可配置，禁止硬编码」与
    // docs/architecture/THREAD_BUDGET_ARCH.md §1（线程预算唯一来源 = Runtime
    // lease），将来若引入并行，worker 数必须由 Runtime 预算/租约注入（形如
    // P2SamplerConfig.cpu_workers，见 module_adapters.cpp CON-004），不得在此
    // 以字面量预留。
    // 节点间距导出所需输入几何（见 §2a）。node_spacing_deg<=0 时**必须**给出；
    // 未给出即显式失败（P2_SKY_PLANE_GEOMETRY_REQUIRED），不回退常数。
    P2SkyPlaneGeometry geometry;
} P2SkyPlaneConfig;

// ---------------------------------------------------------------------------
// 2b. 节点间距自适应重试（§7a「节点间距必须进入自适应重试回路」）
// ---------------------------------------------------------------------------
//
// 分工（§7a 正向约束）：**节点间距负责表示能力，粗糙度惩罚负责条件数**。
// 故本回路的两条分支互不代偿：
//   · 残差仍受表示能力限制（细化节点能实质降低 chi2_red）⇒ 细化节点间距；
//   · κ 超门（求解矩阵病态）⇒ 提高 roughness_penalty，**不动**节点间距。
// 禁止用强平滑掩盖不可表示分量（§7a：那会把可见台阶变成被抹平的错误电平）。
enum {
    P2_SKY_ADAPT_ACCEPTED             = 0,  // 本次尝试被采纳
    P2_SKY_ADAPT_REFINE_NODES         = 1,  // 残差仍受表示能力限制 ⇒ 细化节点间距
    P2_SKY_ADAPT_COARSEN_NODES        = 2,  // 网格不可行（节点数/秩）⇒ 放粗节点间距
    P2_SKY_ADAPT_RAISE_PENALTY        = 3,  // κ 超门 ⇒ 提高粗糙度惩罚（表示能力不动）
    P2_SKY_ADAPT_REPRESENTATION_LIMIT = 4,  // 已到分辨率极限下界而残差仍在下降（诚实边界）
    P2_SKY_ADAPT_BUILD_FAILED         = 5   // 无可行分支，fail-closed
};

// 单次尝试的完整记录（写入 provenance）。
typedef struct {
    double node_spacing_deg;     // 本次尝试的节点间距（度）
    double roughness_penalty;    // 本次尝试的 λ
    double kappa;                // 求解矩阵 H_solve 的条件数（门控量）
    double kappa_data;           // 未惩罚数据矩阵 H_red 的条件数（诊断）
    double chi2_red;
    double rms_weighted;
    std::uint64_t rank;          // H_red 在 rank_rtol 口径下的有效秩
    std::uint64_t rank_solve;    // H_solve 在 rank_rtol 口径下的有效秩（κ 门的等价表述）
    std::uint64_t n_nodes;
    std::int32_t rc;             // P2_SKY_PLANE_*
    std::int32_t action;         // P2_SKY_ADAPT_*（本次尝试后所走分支）
    std::int32_t adopted;        // 1 = 本尝试的解即最终生效解；0 = 被后续尝试取代/被拒
} P2SkyPlaneAttempt;

enum { P2_SKY_ADAPT_MAX_ATTEMPTS = 12 };

// 自适应回路的完整 provenance。n_node_refinements>0 或 n_node_coarsenings>0
// 即「节点间距自适应确实被触发过」（§7a：无触发记录的路径视为未实现）。
typedef struct {
    std::int32_t n_attempts;
    std::int32_t n_node_refinements;      // 细化次数
    std::int32_t n_node_coarsenings;      // 放粗次数
    std::int32_t n_penalty_escalations;   // 粗糙度提升次数
    std::int32_t node_adaptive_used;      // 节点间距是否**真的被调整过**
    std::int32_t penalty_adaptive_used;   // 粗糙度是否**真的被提高过**
    std::int32_t representation_limited;  // 在下界上残差仍未收敛 ⇒ 表示能力到顶（如实登记）
    std::int32_t clamped_to_upper;        // 显式初值被规则 1 上界夹紧过
    double constraining_scale_deg;
    double node_spacing_upper_deg;        // 搜索上界（规则 1 的值）
    double node_spacing_lower_deg;        // 搜索下界（数据自身分辨率极限）
    double node_spacing_deg;              // 最终生效
    double roughness_penalty;             // 最终生效
    double kappa;                         // 最终 κ
    double kappa_data;
    double chi2_red;
    double residual_improve_ratio;        // 实际使用的表示收敛判据
    double kappa_max_effective;           // 实际生效的 κ 门
    P2SkyPlaneAttempt attempts[P2_SKY_ADAPT_MAX_ATTEMPTS];
} P2SkyPlaneAdaptiveReport;

// 自适应回路配置。默认值见 p2_sky_plane_default_adaptive_config。
// **没有任何标定常数**：搜索区间由输入几何给，判据是相对量。
typedef struct {
    int    enabled;                   // 0=关闭（单次求解）；默认 1
    int    max_attempts;              // 总尝试上限（含首次）；<=0 → 默认 6
    int    max_node_refinements;      // 细化次数上限；<0 → 默认 4
    int    max_node_coarsenings;      // 放粗次数上限；<0 → 默认 4
    // κ 触顶时先走「提高粗糙度惩罚」分支的次数上限；用完仍超门才转「细化节点重解」
    // （§7a 规则 2：条件数不达门时，除提高粗糙度惩罚外**必须允许细化节点重解**）。
    int    max_penalty_escalations;   // <0 → 默认 2
    // 表示收敛判据 r∈(0,1)：细化到 h/2 后 chi2_red 至少降到 r 倍，才认为残差仍受
    // 表示能力限制、继续细化；否则认为已收敛、采纳较粗的网格（条件数更好）。
    // 该判据是**相对量**（同一数据两次求解之比），不含任何绝对标定值。
    double residual_improve_ratio;    // <=0 → 默认 0.5
    double penalty_growth;            // κ 触顶时 λ 的倍率；<=1 → 默认 10
} P2SkyPlaneAdaptiveConfig;

typedef struct {
    std::uint32_t version;
    std::uint64_t n_samples;      // 输入采样点
    std::uint64_t n_used;         // 参与拟合（未被掩膜/拒绝）
    std::uint64_t n_frames;       // 出现的帧数
    std::uint64_t n_nodes;        // B_ref 系数（nx*ny）
    std::uint64_t n_params;       // 自由度 = n_nodes + n_frames*m - gauge
    std::uint64_t rank;
    double kappa;
    double rms_weighted;          // Σw r²/Σw 的平方根
    double rms_unweighted;        // 未加权残差 RMS
    double chi2_red;              // Σw r²/(n_used-n_params)
    int iterations;
    int gauge_mode;
    int weight_mode;
    int frame_gradient_order;
    int spline_degree;
    double node_spacing_deg;
    double ra0_deg;
    double dec0_deg;
    double u_min_deg, u_max_deg, v_min_deg, v_max_deg;  // 采样域（切平面）
    double gauge_shift;           // sum 规范相对 reference_frame 的常数平移
    std::uint64_t n_masked;       // 被掩膜/低支持剔除的点
    std::uint64_t n_rejected;     // 稳健迭代剔除的点
    char model_hash[65];
    // SCI-502 FIX-3：门控 kappa 取**求解矩阵**（H_red + λ·DᵀD）的条件数——只有它
    // 随 roughness_penalty 下降，自适应重试才可能成功；本字段保留**未惩罚**数据
    // 矩阵的条件数作为独立诊断量（λ=0 时两者逐位相等）。
    double kappa_data;
    // 实际生效的 κ 门与来源（§7a 规则 4：门控口径必须按矩阵谱自身定，不得保留
    // 互相矛盾的绝对常数）。kappa_max_source: 0 = 由 rank_rtol 派生（1/rank_rtol，
    // 与「H_solve 在 rank_rtol 口径下的有效秩 == n_free」等价）；1 = 调用方显式覆盖。
    double kappa_max_effective;
    int    kappa_max_source;
    // H_solve 在 rank_rtol 口径下的有效秩（κ 门的等价表述；rank 字段是 H_red 的）。
    std::uint64_t rank_solve;
    // 节点间距来源：0 = 由输入几何导出；1 = 调用方显式给出。
    int    node_spacing_source;
    double node_spacing_upper_deg;   // 规则 1 上界（导出时）
    double node_spacing_lower_deg;   // 数据分辨率极限下界（导出时）
} P2SkyPlaneInfo;

enum {
    P2_SKY_PLANE_OK                  = 0,
    P2_SKY_PLANE_INVALID_ARGS        = 1,
    P2_SKY_PLANE_NO_USABLE_SAMPLES   = 2,
    P2_SKY_PLANE_TOO_FEW_SAMPLES     = 3,
    P2_SKY_PLANE_FRAME_UNDERDETERMINED = 4,
    P2_SKY_PLANE_TOO_MANY_NODES      = 5,
    P2_SKY_PLANE_RANK_DEFICIENT      = 6,
    P2_SKY_PLANE_KAPPA_EXCEEDED      = 7,
    P2_SKY_PLANE_NONFINITE_SOLUTION  = 8,
    P2_SKY_PLANE_IO_ERROR            = 9,
    // 未给出 node_spacing_deg 且无法由输入几何导出（几何量缺失/不受支持）。
    // **显式失败**：§7a 禁止回退到「默认节点间距」标定常数。
    P2_SKY_PLANE_GEOMETRY_REQUIRED   = 10
};

// 求值状态。
enum {
    P2_SKY_EVAL_OK           = 0,
    P2_SKY_EVAL_OUT_OF_DOMAIN = 1,  // 越域：禁止外插
    P2_SKY_EVAL_UNKNOWN_FRAME = 2,
    P2_SKY_EVAL_INVALID       = 3
};

P2_API P2SkyPlaneConfig p2_sky_plane_default_config(void);

P2_API P2SkyPlaneAdaptiveConfig p2_sky_plane_default_adaptive_config(void);

// 联合拟合 B_ref + δ_k（按帧 Schur 消元 + 稳健 IRLS）。
// 返回 P2_SKY_PLANE_*。err 可空（8KB 文本建议）。
// cfg->node_spacing_deg<=0 时由 cfg->geometry 导出；几何缺失 ⇒
// P2_SKY_PLANE_GEOMETRY_REQUIRED（显式失败，不回退常数）。
P2_API int p2_sky_plane_build(const P2SkySample* samples, std::uint64_t n,
                              const P2SkyPlaneConfig* cfg, void** out_model,
                              char* err, std::size_t err_size);

// 带**节点间距自适应**的联合拟合（§7a「节点间距必须进入自适应重试回路」）。
// 搜索区间由输入几何给（上界 = 规则 1 的值，下界 = 数据自身分辨率极限），
// 起点默认取上界（在规则内最粗 ⇒ 条件数最好），按表示收敛判据向下细化。
// cfg->geometry 必须有效；否则返回 P2_SKY_PLANE_GEOMETRY_REQUIRED。
// out_report 可空；非空时写出**每次尝试**的节点间距/λ/κ/rank/残差与最终生效值。
// 返回最终采纳解的 P2_SKY_PLANE_*；全部尝试失败时返回最后一次的失败码。
P2_API int p2_sky_plane_build_adaptive(const P2SkySample* samples, std::uint64_t n,
                                       const P2SkyPlaneConfig* cfg,
                                       const P2SkyPlaneAdaptiveConfig* adaptive,
                                       void** out_model,
                                       P2SkyPlaneAdaptiveReport* out_report,
                                       char* err, std::size_t err_size);

// 取回模型上记录的自适应 provenance（由 p2_sky_plane_build_adaptive 写入；
// 单次 build 时 n_attempts=0）。返回 0=ok，1=参数错误。
P2_API int p2_sky_plane_adaptive_report(const void* model,
                                        P2SkyPlaneAdaptiveReport* out);

P2_API int p2_sky_plane_info(const void* model, P2SkyPlaneInfo* out);

// 现场求值 b_k(ra,dec) = B_ref + δ_k + gauge_shift。frame_id 未知 → 2。
// 越域（含 max_extrapolation_deg 余量）→ out_status=P2_SKY_EVAL_OUT_OF_DOMAIN，
// 返回 0（调用方按状态处理），不做外插。
P2_API int p2_sky_plane_eval(const void* model, std::uint64_t frame_id,
                             double ra_deg, double dec_deg,
                             double* out_value, int* out_status);

// 分块按需求值：out_values[i] 与 out_status[i]（可空）对应 ra[i]/dec[i]。
// 返回 0=全部成功；2=存在越域（状态已逐点写出）；1=参数错误。
P2_API int p2_sky_plane_eval_block(const void* model, std::uint64_t frame_id,
                                   const double* ra_deg, const double* dec_deg,
                                   std::uint64_t n, double* out_values,
                                   std::uint8_t* out_status);

// 只求值逐帧偏差 δ_k(ra,dec) = b_k(x) − B_ref(x)（B 口径归一化用）。
//   corrected_k(x) = raw_k(x) − δ_k(x)  ⇔  raw_k(x) − b_k(x) + B_ref(x)
// 即把第 k 帧归一化到公共参考面 B_ref（多退少补），保留 B_ref 真实天光亮度，
// 只消除帧间差异。δ_k = gauge_shift + δ_k 多项式项；gauge_mode=0 时 gauge_shift=0
// 且参考帧 δ_ref≡0（与 UPM MA 同构）。**不改变 p2_sky_plane_eval 语义**（其他
// 调用方仍取 b_k=B_ref+δ_k）。状态/返回码语义与 p2_sky_plane_eval 完全一致
// （未知帧 → UNKNOWN_FRAME；越域 → OUT_OF_DOMAIN，均不外插）。
P2_API int p2_sky_plane_eval_delta(const void* model, std::uint64_t frame_id,
                                   double ra_deg, double dec_deg,
                                   double* out_value, int* out_status);

// 分块 δ_k 求值：out_values[i]/out_status[i] 对应 ra[i]/dec[i]。
// 返回 0=全部成功；2=存在越域/未知帧（状态逐点写出，out_values 该点=0）；1=参数错误。
P2_API int p2_sky_plane_eval_delta_block(const void* model, std::uint64_t frame_id,
                                         const double* ra_deg, const double* dec_deg,
                                         std::uint64_t n, double* out_values,
                                         std::uint8_t* out_status);

// 每帧 δ_k 系数（m 个，升序 a+b<=order 的 u^a v^b），可空查询数量。
P2_API int p2_sky_plane_frame_delta(const void* model, std::uint64_t frame_id,
                                    double* out_coeffs, std::uint64_t cap,
                                    std::uint64_t* out_n);

// 稀疏持久化（只写系数/网格/帧 δ，不写像素）。
P2_API int p2_sky_plane_save(const void* model, const char* path);
P2_API int p2_sky_plane_open(const char* path, void** out_model);
P2_API void p2_sky_plane_close(void* model);

// 帧间直流比增益估计（P0-10 交叉校验）：取 frame 与 ref_frame 的公共 control，
// 返回 sum(value_frame)/sum(value_ref)。直流不受帧间加性结构（相位/台阶）影响，
// 是独立于 UPM MA 结构定标的稳健增益量；MA 与该值不一致时不得施加 MA 增益。
// 无公共 control 或分母为 0 → 返回非 0（调用方 fail-closed）。
P2_API int p2_sky_estimate_gain_dc(const P2SkySample* samples, std::uint64_t n,
                                   std::uint64_t ref_frame, std::uint64_t frame,
                                   double* out_g);

// 独立残差复算（不依赖模型内部缓存）：给定样本，返回加权/未加权 RMS。
// 用于 Oracle 对拍与前台独立复跑。rc 同 build 参数约定。
P2_API int p2_sky_plane_residuals(const void* model,
                                  const P2SkySample* samples, std::uint64_t n,
                                  double* out_rms_weighted,
                                  double* out_rms_unweighted,
                                  std::uint64_t* out_n_used);

#ifdef __cplusplus
}
#endif
