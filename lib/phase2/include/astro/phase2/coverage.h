// lib/phase2/include/astro/phase2/coverage.h
//
// Phase2 W3：输入发现 / 兼容校验 / coverage union / target_order。
//
// 语义（冻结，控制包 34A532A2...B2EB308 + wiki Phase2_Architecture）：
//   - 输入为多个 Phase1 单帧 HiPS（signal/support/snr），不重新校准/PlateSolve/PSF/DR3SP/Drizzle；
//   - 兼容：同一 equatorial/ICRS、同一 filter/passband、同一 signal/support 语义、NESTED；
//   - target_order = min(所有输入最高 leaf order)，禁止低 order 插值伪装分辨率；
//   - Ω = MOC_1 ∪ ... ∪ MOC_N（NESTED，允许不连通分量）。
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#define P2_API __declspec(dllexport)
#else
#define P2_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    std::uint64_t order;     // leaf order（NESTED）
    std::uint64_t ipix;      // NESTED leaf pixel（tile 父单元）
} P2MocCell;

typedef struct {
    char hips_path[1024];
    char frame_id[64];       // 唯一帧标识（文件名/哈希）
    int  max_leaf_order;
    int  n_tiles;
    char filter_passband[64];
    char frame_type[32];     // equatorial / icrs
} P2HipsInputInfo;

typedef struct {
    std::uint64_t n_inputs;
    P2HipsInputInfo* inputs;         // 调用方分配 n_inputs
    std::uint64_t n_union_cells;
    P2MocCell* union_cells;          // 调用方分配（先 n_union_cells=0 查询）
    int  target_order;
    int  status;                     // 0=ok
    char error[512];
} P2CoverageResult;

// 发现并校验输入 HiPS，计算 union MOC 与 target_order。
// 两次调用：第一次 n_union_cells=0 查询所需容量；第二次传入容量。
P2_API int p2_coverage_build(
    const char* const* hips_paths, std::uint64_t n_inputs,
    P2CoverageResult* out);

P2_API int p2_coverage_free(P2CoverageResult* out);

#ifdef __cplusplus
}
#endif