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

// 天光面配置（默认值见 p2_sky_plane_default_config）。
typedef struct {
    int    spline_degree;         // B_ref 样条阶数：1（双线性）或 3（双三次）；默认 1
    double node_spacing_deg;      // B_ref 节点间距（切平面角度）；默认 1.0
    int    frame_gradient_order;  // δ_k 阶数：0=偏移 1=平面 2=二次；默认 1
    double roughness_penalty;     // B_ref 二阶差分粗糙度惩罚 λ；默认 1e-3
    double huber_delta;           // 稳健 Huber δ；默认 1.345
    int    max_iterations;        // 稳健 IRLS 外层迭代上限；默认 30
    double tolerance;             // 收敛门（max|ΔB| 相对量）；默认 1e-10
    int    gauge_mode;            // 0=reference_frame；1=sum_zero；默认 0
    int    weight_mode;           // 0=inverse_variance；1=snr2；默认 0
    double kappa_max;             // 约化系统条件数上限；默认 1e8
    double rank_rtol;             // 奇异值相对门；默认 1e-10
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
} P2SkyPlaneConfig;

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
    P2_SKY_PLANE_IO_ERROR            = 9
};

// 求值状态。
enum {
    P2_SKY_EVAL_OK           = 0,
    P2_SKY_EVAL_OUT_OF_DOMAIN = 1,  // 越域：禁止外插
    P2_SKY_EVAL_UNKNOWN_FRAME = 2,
    P2_SKY_EVAL_INVALID       = 3
};

P2_API P2SkyPlaneConfig p2_sky_plane_default_config(void);

// 联合拟合 B_ref + δ_k（按帧 Schur 消元 + 稳健 IRLS）。
// 返回 P2_SKY_PLANE_*。err 可空（8KB 文本建议）。
P2_API int p2_sky_plane_build(const P2SkySample* samples, std::uint64_t n,
                              const P2SkyPlaneConfig* cfg, void** out_model,
                              char* err, std::size_t err_size);

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
