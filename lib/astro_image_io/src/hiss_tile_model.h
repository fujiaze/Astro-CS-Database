#ifndef HISS_TILE_MODEL_H
#define HISS_TILE_MODEL_H

// ============================================================================
// hiss_tile_model.h - AstroCS HISS Tile 父子几何模型
//
// 依据:
// - 02_FROZEN_STAGE1_HISS_SPEC.md §11 (自适应空间 Tile)
// - docs/stage1_fix/00_COMMON_CONTRACTS.md §2.1 (Tile 几何冻结接口)
// - docs/stage1_fix/spec.md 步骤1 (Tile 父子模型修正)
//
// 核心公式 (冻结, 子代理不得修改):
// d = min(9, log2(NSIDE/16))
// NSIDE_tile = NSIDE / 2^d
// n_leaf_per_tile = 4^d = (NSIDE / NSIDE_tile)^2
//
// 关键修正:
// 旧错误: 单 Tile 叶像素数 = tile_nside^2 * 12 (这是全天像素数, 错误)
// 新正确: 单 Tile 叶像素数 = 4^d (仅一个父像素下的子像素数)
//
// NESTED 排序下的父子关系 (位运算):
// 全局 ipix = (parent_ipix << 2d) | local_ipix
// parent_ipix = global_ipix >> 2d
// local_ipix = global_ipix & ((1 << 2d) - 1)
// ============================================================================

#include <cstdint>
#include "hiss_format.h"  // HISS_EXPORT 宏

namespace hiss {

// ============================================================================
// HissTileGeometry - Tile 几何结构体 (依据 00_COMMON_CONTRACTS §2.1)
//
// 一个 HissTileGeometry 描述一个 Tile 父像素下的叶像素网格。
// 同一 NSIDE 下所有 Tile 共享 depth/tile_nside/n_leaf_per_tile,
// 区别仅在 parent_ipix。
// ============================================================================
struct HissTileGeometry {
    uint32_t nside            = 0;  // 全局 NSIDE (2 的幂)
    uint32_t tile_nside       = 0;  // Tile 父级 NSIDE (>=16)
    int      depth            = 0;  // d = min(9, log2(NSIDE/16))
    uint32_t n_leaf_per_tile  = 0;  // 4^d = (NSIDE/tile_nside)^2, 单 Tile 叶像素数
    uint64_t parent_ipix      = 0;  // Tile 父像素 NESTED ipix

    // --------------------------------------------------------------------
    // local_to_global: Tile 内局部索引 -> 全局 NESTED ipix
    // local_ipix: Tile 内局部索引, 范围 [0, n_leaf_per_tile)
    // 返回: 全局 NESTED ipix; 若 local_ipix 非法返回 UINT64_MAX
    // --------------------------------------------------------------------
    uint64_t local_to_global(uint32_t local_ipix) const;

    // --------------------------------------------------------------------
    // global_to_local: 全局 NESTED ipix -> Tile 内局部索引
    // 注意: 本函数仅提取低位, 不校验该 global_ipix 是否属于本 Tile。
    // 如需校验请用 owns_global。
    // --------------------------------------------------------------------
    uint32_t global_to_local(uint64_t global_ipix) const;

    // --------------------------------------------------------------------
    // global_to_parent: 全局 NESTED ipix -> 父像素 ipix
    // --------------------------------------------------------------------
    uint64_t global_to_parent(uint64_t global_ipix) const;

    // --------------------------------------------------------------------
    // is_valid_local: 校验 local_ipix 是否在 [0, n_leaf_per_tile) 范围内
    // --------------------------------------------------------------------
    bool is_valid_local(uint32_t local_ipix) const;

    // --------------------------------------------------------------------
    // owns_global: 校验 global_ipix 是否属于本 Tile (parent_ipix 匹配)
    // --------------------------------------------------------------------
    bool owns_global(uint64_t global_ipix) const;
};

// ============================================================================
// make_tile_geometry: 构造 Tile 几何 (基础几何, parent_ipix=0)
//
// 入参:
// nside - 全局 NSIDE (2 的幂, >=1)
// 返回:
// HissTileGeometry, depth/tile_nside/n_leaf_per_tile 已填充, parent_ipix=0
//
// 规则:
// - nside < 16: depth=0, tile_nside=nside, n_leaf_per_tile=1
// - nside >= 16: depth = min(9, log2(nside/16)), tile_nside = nside>>depth,
// n_leaf_per_tile = 1<<(2*depth) = 4^depth
// ============================================================================
HISS_EXPORT HissTileGeometry make_tile_geometry(uint32_t nside);

// ============================================================================
// make_tile_geometry_for_parent: 为指定 parent_ipix 构造 Tile 几何
//
// 入参:
// nside - 全局 NSIDE
// parent_ipix - Tile 父像素 NESTED ipix (必须 < 12*tile_nside^2)
// 返回:
// HissTileGeometry, 含 parent_ipix
// ============================================================================
HISS_EXPORT HissTileGeometry make_tile_geometry_for_parent(uint32_t nside,
                                                            uint64_t parent_ipix);

} // namespace hiss

#endif // HISS_TILE_MODEL_H
