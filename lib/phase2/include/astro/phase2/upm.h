// lib/phase2/include/astro/phase2/upm.h
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
    double zero_anchor_weight;    // 弱零校正锚权重（默认 1e-3）
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
    int    cpu_workers;          // 默认 0
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

#ifdef __cplusplus
}
#endif
