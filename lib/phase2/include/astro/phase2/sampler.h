// lib/phase2/include/astro/phase2/sampler.h
//
// Phase2 W4：稀疏光度控制点采样器（control sampler）。
//
// 语义（冻结，控制包 34A532A2...B2EB308 + wiki Phase2_Unified_Photometric_Model）：
//   - 控制点 geometry 由 union 几何与目标角间距决定，**不由 SNR 决定**；
//   - 每个覆盖控制节点的 frame 提供观测 y_ik/sigma_ik/snr_ik/support_ik/quality_ik，
//     y_ik 必须从实际 Phase1 HiPS 数据读取；
//   - patch estimator：control cell 附近小型 HEALPix patch，finite/support 过滤，
//     robust median/biweight location，MAD/robust scale，保留负值；
//   - SNR 只作为当前 frame 在当前 control 观测的可信度，来自 Phase1 SNR Catalogue
//     （禁止重新检测星点）。
#pragma once

#include "astro/phase2/coverage.h"
#include "astro/phase2/upm.h"

#include <cstdint>
#include <cstddef>

#ifdef _WIN32
#define P2_API __declspec(dllexport)
#else
#define P2_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 采样配置（默认值在 sampler.cpp 中定义）
typedef struct {
    int control_grid_per_tile;    // 每个 union tile 内的 control cell 网格数（默认 8）
    int patch_radius_leaf;        // 观测 patch 半径（叶级 leaf 数，默认 2 → 5×5）
    int min_samples;              // 有效样本最小数（默认 5）
    double snr_search_radius_deg; // SNR 星点检索半径（度，默认 0.05）
} P2SamplerConfig;

// 采样控制观测。调用方先以 out_n_obs=0 查询所需数量，再分配后二次调用；
// 或直接传入足够大的 out_capacity。
P2_API int p2_sample_controls(
    const P2CoverageResult* coverage,
    const char* const* hips_paths,
    const P2SamplerConfig* cfg,
    P2ControlObservation* out_obs,   // 可空（查询容量）
    std::uint64_t out_capacity,
    std::uint64_t* out_n_obs,        // 实际观测数
    std::uint64_t* out_n_controls,   // 控制节点数
    char* err, std::size_t err_size);

#ifdef __cplusplus
}
#endif
