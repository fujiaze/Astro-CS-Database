// ============================================================================
// hiss_tile_model.cpp - AstroCS HISS Tile 父子几何模型实现
//
// 依据:
//   - 02_FROZEN_STAGE1_HISS_SPEC.md §11
//   - docs/stage1_fix/00_COMMON_CONTRACTS.md §2.1
//   - docs/stage1_fix/spec.md 步骤1
//
// 实现要点:
//   1. depth = min(9, log2(NSIDE/16)) — 满 Tile 最多 4^9=262144 叶像素
//   2. tile_nside = NSIDE / 2^depth — Tile 父级 NSIDE 不低于 16
//   3. n_leaf_per_tile = 4^depth — 单 Tile 叶像素数 (不是 tile_nside^2*12)
//   4. NESTED 排序下父子关系为位运算:
//        global = (parent << 2d) | local
//        parent = global >> 2d
//        local  = global & ((1 << 2d) - 1)
// ============================================================================

#include "hiss_tile_model.h"

#include <cstdio>
#include <cstdint>
#include <limits>

namespace hiss {

// ============================================================================
// 内部辅助: log2_pow2 — 求 2 的幂的对数
//   nside 是 2 的幂 (HEALPix 强制约束), 用位运算求 log2(nside)
//   输入 v 必须大于 0
// ============================================================================
static int log2_pow2(uint32_t v) {
    if (v == 0) return -1;
    int n = 0;
    while (v > 1) { v >>= 1; n++; }
    return n;
}

// ============================================================================
// make_tile_geometry: 构造 Tile 几何 (基础几何, parent_ipix=0)
//
// 步骤:
//   1. NSIDE < 16: depth=0, tile_nside=nside, n_leaf_per_tile=1
//      (整个球面只有一个 Tile 父级, 即 NSIDE 自身)
//   2. NSIDE >= 16: depth = min(9, log2(nside) - 4)
//      tile_nside = nside >> depth
//      n_leaf_per_tile = 1u << (2 * depth)  // 4^depth
// ============================================================================
HissTileGeometry make_tile_geometry(uint32_t nside) {
    HissTileGeometry g{};
    g.nside = nside;
    g.parent_ipix = 0;

    if (nside == 0) {
        fprintf(stderr,
                "[hiss][tile_model] make_tile_geometry: nside=0 非法\n");
        return g;  // 全部为 0
    }

    if (nside < 16) {
        // NSIDE < 16 时 d=0, tile_nside = nside (整个球面一个 Tile 父级组)
        // n_leaf_per_tile = 4^0 = 1
        g.depth = 0;
        g.tile_nside = nside;
        g.n_leaf_per_tile = 1;
        fprintf(stdout,
                "[hiss][tile_model] make_tile_geometry: nside=%u (<16) -> "
                "depth=0, tile_nside=%u, n_leaf_per_tile=1\n",
                nside, g.tile_nside);
        return g;
    }

    // nside >= 16: 计算 depth = min(9, log2(nside/16)) = min(9, log2(nside) - 4)
    int log2_nside = log2_pow2(nside);
    int d = log2_nside - 4;
    if (d < 0) d = 0;
    if (d > 9) d = 9;  // 上限 9: 满 Tile 最多 4^9 = 262144 叶像素

    g.depth = d;
    g.tile_nside = nside >> d;          // nside / 2^d
    g.n_leaf_per_tile = 1u << (2 * d);  // 4^d = 2^(2d)

    fprintf(stdout,
            "[hiss][tile_model] make_tile_geometry: nside=%u -> depth=%d, "
            "tile_nside=%u, n_leaf_per_tile=%u (4^%d)\n",
            nside, g.depth, g.tile_nside, g.n_leaf_per_tile, g.depth);
    return g;
}

// ============================================================================
// make_tile_geometry_for_parent: 为指定 parent_ipix 构造 Tile 几何
// ============================================================================
HissTileGeometry make_tile_geometry_for_parent(uint32_t nside,
                                                uint64_t parent_ipix) {
    HissTileGeometry g = make_tile_geometry(nside);
    g.parent_ipix = parent_ipix;
    return g;
}

// ============================================================================
// local_to_global: Tile 内局部索引 -> 全局 NESTED ipix
//
// 公式: global = (parent_ipix << 2d) | local_ipix
//
// 边界检查:
//   - local_ipix >= n_leaf_per_tile: 返回 UINT64_MAX 并打日志
// ============================================================================
uint64_t HissTileGeometry::local_to_global(uint32_t local_ipix) const {
    if (local_ipix >= n_leaf_per_tile) {
        fprintf(stderr,
                "[hiss][tile_model] local_to_global: local_ipix=%u 超出范围 "
                "[0, %u), nside=%u, depth=%d\n",
                local_ipix, n_leaf_per_tile, nside, depth);
        return std::numeric_limits<uint64_t>::max();
    }
    // 2d 位移 (depth 最大 9, 2*depth 最大 18, 不溢出 uint64)
    int shift = 2 * depth;
    return (parent_ipix << shift) | (uint64_t)local_ipix;
}

// ============================================================================
// global_to_local: 全局 NESTED ipix -> Tile 内局部索引
//
// 公式: local = global & ((1 << 2d) - 1)
//
// 注意: 仅提取低位, 不校验该 global_ipix 是否属于本 Tile。
//       depth=0 时掩码=0, 所有 global_ipix 都映射到 local=0 (n_leaf=1)。
// ============================================================================
uint32_t HissTileGeometry::global_to_local(uint64_t global_ipix) const {
    if (depth == 0) {
        // 4^0 = 1, 只有 local=0
        return 0;
    }
    int shift = 2 * depth;
    // 构造低 (2d) 位掩码: (1 << shift) - 1
    // shift 最大 18, 安全
    uint64_t mask = (1ULL << shift) - 1ULL;
    return (uint32_t)(global_ipix & mask);
}

// ============================================================================
// global_to_parent: 全局 NESTED ipix -> 父像素 ipix
//
// 公式: parent = global >> 2d
// ============================================================================
uint64_t HissTileGeometry::global_to_parent(uint64_t global_ipix) const {
    int shift = 2 * depth;
    return global_ipix >> shift;
}

// ============================================================================
// is_valid_local: 校验 local_ipix 是否在 [0, n_leaf_per_tile) 范围内
// ============================================================================
bool HissTileGeometry::is_valid_local(uint32_t local_ipix) const {
    return local_ipix < n_leaf_per_tile;
}

// ============================================================================
// owns_global: 校验 global_ipix 是否属于本 Tile (parent_ipix 匹配)
//
// 用途: 给定一个全局 ipix, 判断它是否落在本 Tile 父像素下。
// ============================================================================
bool HissTileGeometry::owns_global(uint64_t global_ipix) const {
    return global_to_parent(global_ipix) == parent_ipix;
}

} // namespace hiss
