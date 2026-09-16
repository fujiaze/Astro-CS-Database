// lib/algorithms/coverage/include/astro/phase2/upm.h
//
// Phase2 Unified Photometric Model (UPM) 公共接口（W2 冻结，
// AstroCS_Phase2_Implementation_Control_Package_V1，SHA 34A532A2...B2EB308）。
//
// 语义（冻结）：
// - 输入为多个 Phase1 单帧 HiPS；覆盖并集 Ω = MOC union；
// - 在 Ω 内布置稀疏球面光度控制点（与 SNR 几何解耦）；
// - 全局联合求解 ONE UnifiedPhotometricModel（Huber IRLS、
// SNR-aware 权重、图平滑、弱零锚、连通分量）；
// - 模型内部允许 frame_id 联合系数（曝光残余背景），但同一模型版本管理；
// - 不暴露 per-frame gradient 产品；运行时只经
// p2_upm_calibrate_block(model, frame_id, ...) 使用；
// - sparse/dense 持久化与缓存 checksum 校验。
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

// ===== 控制观测（W2 冻结 + control-variance 合同）=====
typedef struct {
    std::uint64_t frame_id;
    std::uint64_t control_id;
    std::uint64_t leaf_ipix;      // NESTED leaf pixel（控制拓扑位置）
    double ra_deg;
    double dec_deg;
    double value;                 // local photometric estimate（可负）
    double uncertainty;           // control estimator 标准误（= sqrt(control_variance)）
    double snr;
    // 弃用（仅诊断）：NoiseWeightModelV1 控制 leaf 单像素 Phase1 ivar。
    // 它不是 Var(control estimator)，禁止在科学权重中使用。
    double ivar;
    // 冻结（SCI-UPM-WEIGHT-001 / ALG-UPM-CONTROL-IVAR-001 /
    // DATA-UPM-CONTROL-UNC-001）：
    // control estimator = background-clean patch median；其统计方差
    // control_variance = k_corr × (π/2) × sigma_bg² / N_retained；
    // control_ivar = 1 / control_variance。
    // k_corr 由当前 Drizzle synthetic noise/covariance MC 校准（非猜测）。
    double control_variance;
    double control_ivar;
    // local SNR 可用性。1=该 control cell 邻域确有 catalogue 星点
    // （snr 为真实局部中位数，可为 1.0）；0=无局部星点（snr 无意义，由
    // 调用方回退整帧 median，禁止以 1.0 伪装 unknown）。
    int snr_available;
    double support;
    std::uint32_t quality_flags;
} P2ControlObservation;

// ===== 模型信息（W2 冻结）=====
typedef struct {
    std::uint32_t version;
    std::uint32_t precision;      // 0=fp32, 1=fp64
    std::uint32_t target_order;
    std::uint64_t control_count;
    std::uint64_t observation_count;
    std::uint32_t component_count;
    char model_hash[65];          // 模型内容 SHA-256
} P2ModelInfo;

// ===== 构建配置（W2 冻结）=====
typedef struct {
    int    robust_loss;           // 0=huber（首版）
    int    snr_weight_mode;       // 0=snr2_normalized（首版）
    double huber_delta;           // Huber delta（默认 1.345）
    double smoothing_lambda;      // 图平滑权重（默认 0=关闭）
    double zero_anchor_weight;    // 弱零校正锚权重（生产装配显式 1e-3，SCI §9a:133）
    int    max_iterations;        // IRLS 最大迭代（默认 100）
    double tolerance;             // 收敛容差（默认 1e-6）
    int    target_order;          // 模型目标 order（-1=auto）
    double sigma_floor;           // uncertainty 下限（默认 1e-3）
    double support_power;         // support 因子指数（默认 1.0）
    int    quality_mode;          // 0=flags 映射（默认）
    // 1=science weight 用 control_ivar（=1/control_variance，SCI-UPM-WEIGHT-001）；
    // 0=legacy snr²/(1+snr²)/unc² 仅用于 ablation/诊断 (SNR-015)。
    // production 模式 control_ivar<=0/非有限 → 显式 INVALID（禁止静默回退）。
    int    use_ivar_weight;       // 默认 1
    double control_reliability;   // 默认 control reliability（默认 1.0）
    const char* input_manifest_hash;  // 输入稳定 manifest（可空；非空时参与模型 hash）
    // CON-005 并行观察/聚合 worker 数（0=auto；1=串行默认）。仅 P2_ENABLE_OPENMP
    // 且 >1 时并行 compute_raw/聚合；gauge/连通分量/收敛/归并保持固定顺序。
    int    cpu_workers;          // 来自 Runtime lease(p2_session 传 budget.max_workers); 1=串行 reference
    // M7-C-001: UPM 控制 cell 网格边长 G，必须 == 采样器
    // control_grid_per_tile 且 == UPM 网格常数 8；不等时 p2_upm_build* 返回 3
    // （禁静默错格架：control 场会按错误格架插值）。
    int    grid = 8;
} P2UpmBuildConfig;

// ===== 构建 / 持久化 / 求值 =====
P2_API int p2_upm_build(
    const P2ControlObservation* obs, std::uint64_t n_obs,
    const P2UpmBuildConfig* cfg, void** out_model);

// 全几何节点 UPM 构建。nodes 覆盖 coverage union 全部
// control cell（含单帧区），obs 只含 ≥2 clean 帧观测；单帧区节点无
// 数据项，由全局平滑/Laplacian 延拓得到 C（harmonic continuation）。
typedef struct P2ControlNode P2ControlNode;
P2_API int p2_upm_build_geo(
    const P2ControlObservation* obs, std::uint64_t n_obs,
    const P2ControlNode* nodes, std::uint64_t n_nodes,
    const P2UpmBuildConfig* cfg, void** out_model);

P2_API int p2_upm_save(const void* model, const char* path);
P2_API int p2_upm_open(const char* path, void** out_model);
P2_API int p2_upm_info(const void* model, P2ModelInfo* out_info);

// 校准一块（W2 冻结核心接口）：frame_id + leaf_ipix[] + input_signal[] → output_signal[]
P2_API int p2_upm_calibrate_block(
    const void* model,
    std::uint64_t frame_id,
    const std::uint64_t* leaf_ipix,
    const double* input_signal,
    double* output_signal,
    std::uint64_t count);

// 直接求值空间校正场 C_frame(leaf_ipix)（sparse/dense 同一科学语义）。
P2_API double p2_upm_evaluate_c(const void* model, std::uint64_t frame_id,
                                std::uint64_t leaf_ipix);

// 观测 raw weight（production UPM 权重公式，单一实现）。
// production（cfg.use_ivar_weight != 0，SCI-UPM-WEIGHT-001）：
// raw_w = quality_factor × control_ivar（几何可靠性在 per-control 归一化
// 中施加）；obs->control_ivar <= 0 / 非有限 → 返回 2（显式缺 control ivar，
// 禁止静默回退 support/SNR）。
// ablation/诊断（cfg.use_ivar_weight == 0）：
// raw_w = quality_factor * support^support_power * snr^2/(1+snr^2) /
// max(unc^2, sigma_floor^2)。
// 返回 0=ok；1=参数错误；2=production 缺 control ivar。
P2_API int p2_upm_raw_weight(const P2ControlObservation* obs,
                             const P2UpmBuildConfig* cfg,
                             double* out_raw);

// per-control 归一化权重（raw/sum_j(raw) × control_reliability）。
P2_API int p2_upm_normalized_weights(const P2ControlObservation* obs,
                                     std::uint64_t n_obs,
                                     const P2UpmBuildConfig* cfg,
                                     double* out_norm);

// geometry/topology hash（仅 geometry/coverage 决定，不含
// SNR/quality/support 等观测可信度；权重变化不得改变）。
P2_API int p2_upm_geometry_hash(const void* model, char* out, int buf_size);

// 每连通分量求解前固定的 gauge frame id（分量内最小 frame_id；
// 构建与重开后一致）。out 可为 NULL 只取数量。
P2_API int p2_upm_component_gauges(const void* model,
                                   std::uint64_t* out_component_count,
                                   std::uint64_t* out_ref_frame_ids);

// materialize dense cache（同模型 hash/目标 order/frame hash 校验）
P2_API int p2_upm_materialize_dense(
    const void* model, int target_order, const char* cache_path);

// 读取 dense cache 信息（稀疏=稠密 Gate 用）
P2_API int p2_upm_dense_info(
    const void* model, const char* cache_path,
    int* out_target_order, std::uint64_t* out_pixels,
    char* out_source_hash, std::size_t hash_buf_size);

// 读取 dense cache 一块（与 sparse calibrate_block 数值等价；stale 拒绝）
// 返回 0=ok, 1=io/parse, 2=stale-cache（source hash 不匹配）
P2_API int p2_upm_dense_read_block(
    const void* model, const char* cache_path,
    std::uint64_t frame_id,
    const std::uint64_t* leaf_ipix,
    const double* input_signal,
    double* output_signal,
    std::uint64_t count);
// materialize dense cache（同模型 hash/目标 order/frame hash 校验）
P2_API int p2_upm_materialize_dense(
    const void* model, int target_order, const char* cache_path);
// worker 数显式版本（CON-010 并行物化；workers<=0 => auto=omp_get_max_threads）。
P2_API int p2_upm_materialize_dense_n(
    const void* model, int target_order, const char* cache_path, int workers);

P2_API int p2_upm_close(void* model);

// ===========================================================================
// V6 目标态：UPM 乘法/加性分离求解器（ALG-P2S-UPM.1..8）
// ---------------------------------------------------------------------------
// 模型（ALG-P2S-UPM.1，FZ 语义冻结 / docs/contracts/v6/frozen）：
//     y_k(p) = g_k * s(p) + b_k + eps_k(p)
//   g_k   每帧乘法光度响应（相对参考帧无量纲；单位 1）——不得藏进加性场；
//   b_k   每帧加性背景（帧级常数，单位 ADU）；
//   s(p)  潜在真实场（参考帧口径，单位 ADU）；
//   eps   随机项；观测权重 = control_ivar = 1/control_variance。
//
// 分离不变量（宪章 §6.3）：g_k 与 b_k 分别估计，禁止互相代替 / 禁止把乘法
// 尺度隐藏在加性梯度曲面。空间加性场 b_k(x) 的数据面表示属 OPEN-P2S-02
// （DATA-DESIGN-001 / SCHEMA-INTEGRATE-001），本 API 只实现帧级 b_k；空间
// 变化由 s(p) 承载，不自行定值 OPEN 项。
//
// overlap graph / gauge（ALG-P2S-UPM.2）：
//   frame-control 二分图连通分量；每分量独立 gauge，ref = 分量内最小
//   frame_id，scale gauge g_ref=1，level gauge b_ref=0（共 2*n_components）。
//
// 秩 / 条件数（ALG-P2S-UPM.3/.4）：
//   rank(J) == n_free = n_p + 2F - 2*n_components；
//   奇异值判据 sigma_i/sigma_max > rank_rtol（FZ-AP2S-RANK-RTOL=1e-10）；
//   kappa = cond_2( D^-1 (J^T W J) D^-1 )，D=diag(列范数)，上限
//   kappa_max（FZ-AP2S-KAPPA-MAX=1e6）；超限 fail-closed，禁止静默欠定解。
//   min_frames（FZ-AP2S-UPM-MINFRAMES=2）：单帧分量禁拟合 g，须显式
//   additive-only 降级声明。
//
// 参数协方差（ALG-P2S-UPM.5/.6，FZ-FORMULA-COV-PROP）：
//   C_theta = (J^T W J)^-1（gauge 消除后的可辨识子空间）；
//   C_out   = C_stat + J_out C_theta J_out^T。
//   禁止由权重标量/诊断量反推 variance；禁止 variance=1/W_psfsw。
//
// 参数向量布局（p2_upm_ma_param_cov 输出顺序，确定性）：
//   full = [ s_j : control_id 升序 ] ++ [ g_k : frame_id 升序 ]
//          ++ [ b_k : frame_id 升序 ]，去掉 gauge 固定项后按 full 下标升序
//   即 free 参数顺序；n_free = n_p + 2F - 2*n_components。
//
// 返回值（负数/正数语义，全部 fail-closed）：
//   0  ok
//   1  参数错误（空指针/空数据/非法配置）
//   2  缺/非法 control_ivar（<=0 或非有限；production 禁止静默回退 legacy）
//   3  秩亏（rank < n_free；含恒常 s 场导致的 g/b 退化）
//   4  kappa > kappa_max
//   5  单帧分量未声明 additive-only（FZ-AP2S-UPM-MINFRAMES）
//   6  共享系统项按独立处理（未表示共享项声明为已表示）
//   7  k_corr provenance 不完整/越域（FZ-PROV-KCORR）
//   8  非有限解（求解失败）
// ===========================================================================

// UPM 乘法/加性观测（一个 control 点 p 的一帧观测 k）。
typedef struct {
    std::uint64_t frame_id;
    std::uint64_t control_id;   // overlap graph 的 control 节点 id
    double value;               // y_k(p)，单位 ADU
    double control_ivar;        // 1/control_variance，单位 ADU^-2；必须 >0 有限
} P2UpmMaObservation;

// UPM 乘法/加性构建配置（默认值在 upm.cpp 中生效）。
typedef struct {
    int    min_frames;          // FZ-AP2S-UPM-MINFRAMES，默认 2
    double rank_rtol;           // FZ-AP2S-RANK-RTOL，默认 1e-10
    double kappa_max;           // FZ-AP2S-KAPPA-MAX，默认 1e6
    // 0 = min_frame_id gauge（g_ref=1, b_ref=0）；其他值 → 参数错误。
    int    gauge_mode;
    // 1 = 显式声明单帧分量 additive-only 降级（g=1 固定，不拟合 g）；
    // 0（默认）= 单帧分量 fail-closed（rc=5）。
    int    allow_additive_only_single_frame;
    // 1 = C_in 含未表示共享系统项（低秩/相关核）→ fail-closed rc=6；默认 0。
    int    c_in_has_unrepresented_shared_terms;
    // 收敛参数（FZ-UPM-CONVERGENCE，原样继承，不改）。
    double huber_delta;         // 默认 1.345
    int    max_iterations;      // 默认 100
    double tolerance;           // 默认 1e-6
    double sigma_floor;         // 默认 1e-3
    // 默认 1e-3（FZ-UPM-CONVERGENCE 继承值，为合同/provenance 一致性保留）。
    // 乘法/加性模型的退化由显式 gauge + 秩门 fail-closed 处理，本求解器
    // 不对 b_k 施加弱零锚（加零锚会引入偏差）。
    double zero_anchor_weight;
    // k_corr provenance（FZ-PROV-KCORR）。k_corr<=0 表示未声明（不使用）；
    // >0 时必须提供非空 applicability_domain，且只能是域内冻结值 1.4
    // （无固定种子 MC 标定 run 时不得外推/忽略相关 → rc=7）。
    double k_corr;              // 默认 0（未声明）
    const char* k_corr_applicability_domain;  // 可空
    const char* k_corr_calibration_run_id;    // 可空
    const char* flux_conservation_factor;     // provenance 透传，可空
} P2UpmMaConfig;

// 模型摘要（gauge/秩/条件数/自由度）。
typedef struct {
    std::uint32_t version;
    std::uint64_t n_controls;
    std::uint64_t n_frames;
    std::uint64_t n_components;
    std::uint64_t n_observations;
    std::uint64_t n_params;       // n_free（gauge 消除后）
    std::uint64_t rank;           // rank(J) 于解处
    double rank_rtol;
    double kappa;
    double kappa_max;
    int min_frames;
    int gauge_mode;
    int iterations;
    int additive_only_components; // 显式 additive-only 降级的分量数
    char model_hash[65];
} P2UpmMaInfo;

// 构建并联合求解（overlap graph + gauge + 乘法/加性 GN-IRLS + 秩/κ 门）。
P2_API int p2_upm_ma_build(
    const P2UpmMaObservation* obs, std::uint64_t n_obs,
    const P2UpmMaConfig* cfg, void** out_model);

P2_API int p2_upm_ma_info(const void* model, P2UpmMaInfo* out_info);

// 取某观测节点解：g_k / b_k / s(p)。out_* 可空表示不取该项。
// 未知 frame_id / control_id → 1。
P2_API int p2_upm_ma_solution(
    const void* model, std::uint64_t frame_id, std::uint64_t control_id,
    double* out_g, double* out_b, double* out_s);

// frame -> 连通分量下标；control -> 连通分量下标。未知 id → 返回 1。
P2_API int p2_upm_ma_component_of_frame(
    const void* model, std::uint64_t frame_id, std::uint64_t* out_component);
P2_API int p2_upm_ma_component_of_control(
    const void* model, std::uint64_t control_id, std::uint64_t* out_component);
// 每分量 gauge 参考帧（分量内最小 frame_id；与构建顺序无关）。
P2_API int p2_upm_ma_component_ref_frame(
    const void* model, std::uint64_t component, std::uint64_t* out_ref_frame_id);

// C_theta = (J^T W J)^-1，n_params × n_params，row-major（ld >= n_params）。
P2_API int p2_upm_ma_param_cov(const void* model, double* out_C, std::uint64_t ld);

// C_out = C_stat + J_out C_theta J_out^T（FZ-FORMULA-COV-PROP）。
// J_out: m × n_params row-major（ld_J >= n_params）；C_stat: m × m row-major
// （ld_C >= m）；out_C_out: m × m row-major（ld_out >= m）。禁止权重反推。
P2_API int p2_upm_ma_c_out(
    const void* model, const double* J_out, std::uint64_t ld_J, std::uint64_t m,
    const double* C_stat, std::uint64_t ld_C,
    double* out_C_out, std::uint64_t ld_out);

// provenance JSON（FZ-PROV-MINIMAL-SET / ALG-P2S-UPM.8）：gauge_mode、
// 每分量 ref_frame_id、rank、rank_rtol、kappa、kappa_max、C_theta 摘要、
// k_corr+适用域、flux_conservation_factor、model_hash、min_frames、
// any_fail_closed_reason。成功构建时 reason 为空串。
P2_API int p2_upm_ma_provenance(
    const void* model, char* out_json, std::size_t buf_size);

P2_API void p2_upm_ma_close(void* model);

// ALG-P2S-UPM.7 / FZ-PROV-KCORR：control_variance = k_corr*(pi/2)*sigma_bg^2/N。
// fail-closed：N<1、sigma_bg<=0、k_corr 非有限或 <=0 → 1；
// k_corr==1.0（忽略相关）→ 2；k_corr!=1.4 且无 calibration_run_id → 3
// （域外推，DI-04 未复跑标定）；applicability_domain 空 → 4。
// 返回 0 时写 out_control_variance / out_control_ivar（均可空）。
P2_API int p2_upm_control_variance(
    double k_corr, double sigma_bg, std::uint64_t n_retained,
    const char* applicability_domain, const char* calibration_run_id,
    double* out_control_variance, double* out_control_ivar);

#ifdef __cplusplus
}
#endif
