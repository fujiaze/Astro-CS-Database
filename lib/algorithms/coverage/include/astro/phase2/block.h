// lib/algorithms/coverage/include/astro/phase2/block.h
//
// Phase2 W6：动态分块（Block Planner）。
//
// 语义（冻结；权威 = docs/science/algorithms/PHASE2_MOSAIC_WRITE.md
// （micro-chunk 内存规划、逐 tile 真实 depth 重算 p2_block_plan）与
// docs/engineering/OWNERSHIP_AND_LIFETIME.md）：
// - 峰值估算 ≈ P·N_B·B_sample + P·B_scratch + M_AIO + M_UPM + M_ACR；
// - 从少量相邻 HiPS tiles 增长，保持空间局部性与顺序 I/O；
// - 使用真实 coverage depth N_B（不按总帧数规划）；
// - OOM：正常路径减块，不 swap；单 tile 仍超则 tile 内 micro-chunk；
// - determinism：block size 不改变 accepted mask/输出（precision tolerance 内）。
//
// 返回值与 status 是两件事，不要混用：
//   · **返回值** = 「这次调用本身是否成立」：只有空指针才是 1。
//   · **out->status** = 「计划是否可用」：0 可用，1 不可用（含**入参越界**，
//     此时 error 给出可 grep 的原因文本）。调用方判可用性必须读 status，
//     读返回值会把「入参非法」当成「调用成功」。
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
    int  precision;                   // 0=fp32, 1=fp64；**无其它取值**（其它值 → status=1）
    std::uint64_t memory_limit_bytes; // 用户限制
    // 预算余量系数：budget = memory_limit_bytes · safety_factor。**无默认**，
    // 调用方必须显式给出，取值域 (0, 1]；非有限 / ≤0 / >1 ⇒ status=1。
    // 上界 1 的理由：sf>1 等于按比用户给的内存上限更大的预算规划。
    // 它是需标定的工程参数（不是可由输入几何导出的量），现行生产取值
    // 0.75 由调用点显式给出，见 src/block.cpp 的说明。
    double safety_factor;
    std::uint64_t scratch_bytes_per_sample;
    std::uint64_t scratch_bytes_per_pixel;
    std::uint64_t fixed_overhead;     // AIO+UPM+ACR staging
} P2BlockPlannerInput;

typedef struct {
    std::uint64_t block_pixels;
    std::uint64_t estimated_peak_bytes;
    int  micro_chunk_required;        // 1=需 tile 内 micro-chunk（仅 status==0 时有意义）
    int  status;                      // 0=ok, 1=unfeasible（含入参越界）
    char error[256];                  // status!=0 时给出可 grep 的原因文本
} P2BlockPlan;

P2_API int p2_block_plan(const P2BlockPlannerInput* in, P2BlockPlan* out);

#ifdef __cplusplus
}
#endif