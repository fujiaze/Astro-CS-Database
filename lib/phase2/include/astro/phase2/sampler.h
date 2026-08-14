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
    // V13：background-clean 采样（DBE-like；BACKGROUND_SAMPLER_SPEC.md）
    int    background_patch_radius;        // 背景 patch 半径（默认 8 → 17×17）
    double background_clip_sigma;          // 亮端迭代 clipping 阈值（MAD 单位，默认 3.0）
    int    background_clip_iters;          // 亮端 clipping 迭代次数（默认 3）
    double background_max_contamination;   // 亮像素占比上限（默认 0.20）
    double background_contamination_sigma; // 污染判定 sigma（默认 3.0）
    double background_min_retained_fraction; // clipping 后保留比例下限（默认 0.60）
    double background_tolerance;           // 局部 tolerance gate（MAD 单位，默认 3.0）
    int    background_neighbor_radius;     // 局部 baseline 邻域 cell 半径（默认 2）
    int    background_catalog_veto;        // 允许 SNR catalogue veto（默认 1）
} P2SamplerConfig;

// V15：sampler 默认配置单一来源（null cfg 时使用；显式 cfg 覆盖）。
P2_API P2SamplerConfig p2_sampler_default_config(void);

// V13：background-clean 采样统计（accepted/rejected 可追踪）
typedef struct {
    std::uint64_t candidate_observations;   // 候选观测总数（几何×覆盖帧）
    std::uint64_t accepted_observations;    // 进入 UPM 的 clean 观测
    std::uint64_t rejected_insufficient_support;   // support/finite 不足
    std::uint64_t rejected_insufficient_retained;  // clipping 后保留比例过低
    std::uint64_t rejected_bright_tolerance;       // 超过局部 tolerance
    std::uint64_t rejected_high_contamination;     // 亮像素占比过高
    std::uint64_t rejected_catalog_veto;           // 星表 veto
    std::uint64_t rejected_lt_two_clean_frames;    // clean 帧数 <2 未入拟合
    std::uint64_t accepted_controls;               // 有 ≥1 clean obs 的 control
    std::uint64_t overlap_controls;                // 有 ≥2 clean obs 的 control
} P2SampleStats;

// V13：control 几何节点（全 coverage 网格；与观测解耦）
typedef struct P2ControlNode {
    std::uint64_t control_id;
    std::uint64_t tile_ipix;
    int  gx, gy;
    double ra_deg, dec_deg;
    std::uint64_t leaf_ipix;   // cell 中心叶级像素
} P2ControlNode;

// 内容稳定帧标识（FNV-1a 64）：由输入路径派生，与输入顺序无关；
// UPM 参考帧 = 最小 frame_id（保证输入重排输出不变）。
P2_API std::uint64_t p2_frame_id(const char* hips_path);

// R1：统一统计量（sampler patch estimator / MAD / SNR 邻域共用同一实现）。
// median：偶数 n 取上下中位数平均；NaN 自动过滤（全部 NaN → 0）。
P2_API double p2_stats_median(const double* vals, std::uint64_t n);
P2_API double p2_stats_mad(const double* vals, std::uint64_t n,
                           double* out_median = nullptr);

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
    P2SampleStats* out_stats,        // 可空（V13 统计）
    P2ControlNode* out_controls,     // 可空（V13 全几何节点）
    std::uint64_t ctrl_capacity,
    char* err, std::size_t err_size);

#ifdef __cplusplus
}
#endif
