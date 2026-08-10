// lib/phase2/include/astro/phase2/upm.h
//
// Phase2 Unified Photometric Model (UPM) 公共接口（W2 冻结，控制包
// AstroCS_Phase2_Implementation_Control_Package_V1，SHA 34A532A2...B2EB308）。
//
// 语义（冻结）：
//   - 输入为多个 Phase1 单帧 HiPS；覆盖并集 Ω = MOC union；
//   - 在 Ω 内布置稀疏球面光度控制点（与 SNR 几何解耦）；
//   - 全局联合求解 ONE UnifiedPhotometricModel（Huber IRLS、
//     SNR-aware 权重、图平滑、弱零锚、连通分量）；
//   - 模型内部允许 frame_id 联合系数（曝光残余背景），但同一模型版本管理；
//   - 不暴露 per-frame gradient 产品；运行时只经
//     p2_upm_calibrate_block(model, frame_id, ...) 使用；
//   - sparse/dense 持久化与缓存 checksum 校验。
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

// ===== 控制观测（W2 冻结）=====
typedef struct {
    std::uint64_t frame_id;
    std::uint64_t control_id;
    std::uint64_t leaf_ipix;      // NESTED leaf pixel（控制拓扑位置）
    double ra_deg;
    double dec_deg;
    double value;                 // local photometric estimate（可负）
    double uncertainty;           // measurement uncertainty
    double snr;
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
} P2UpmBuildConfig;

// ===== 构建 / 持久化 / 求值 =====
P2_API int p2_upm_build(
    const P2ControlObservation* obs, std::uint64_t n_obs,
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

P2_API int p2_upm_close(void* model);

#ifdef __cplusplus
}
#endif
