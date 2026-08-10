// lib/phase2/include/astro/phase2/block.h
//
// Phase2 W6：动态分块（Block Planner）。
//
// 语义（冻结，控制包 34A532A2...B2EB308 + wiki Phase2_Block_Integration）：
//   - 峰值估算 ≈ P·N_B·B_sample + P·B_scratch + M_AIO + M_UPM + M_ACR；
//   - 从少量相邻 HiPS tiles 增长，保持空间局部性与顺序 I/O；
//   - 使用真实 coverage depth N_B（不按总帧数规划）；
//   - OOM：正常路径减块，不 swap；单 tile 仍超则 tile 内 micro-chunk；
//   - determinism：block size 不改变 accepted mask/输出（precision tolerance 内）。
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
    std::uint64_t output_pixels;      // 当前块输出像素数 P
    std::uint64_t covering_frames;    // 当前块真实覆盖帧数 N_B
    int  precision;                   // 0=fp32, 1=fp64
    std::uint64_t memory_limit_bytes; // 用户限制
    double safety_factor;             // 默认 0.75
    std::uint64_t scratch_bytes_per_sample;
    std::uint64_t scratch_bytes_per_pixel;
    std::uint64_t fixed_overhead;     // AIO+UPM+ACR staging
} P2BlockPlannerInput;

typedef struct {
    std::uint64_t block_pixels;
    std::uint64_t estimated_peak_bytes;
    int  micro_chunk_required;        // 1=需 tile 内 micro-chunk
    int  status;                      // 0=ok, 1=unfeasible
    char error[256];
} P2BlockPlan;

P2_API int p2_block_plan(const P2BlockPlannerInput* in, P2BlockPlan* out);

#ifdef __cplusplus
}
#endif