// ============================================================================
// aio_tile_geometry.cpp - HEALPix Tile 自适应层级几何实现
//
// 从已退场的 hiss_common.cpp 迁出。实现逐字保留原算法 (仅命名空间由
// hiss 改为 aio), 不改任何数值口径。
// ============================================================================
#include "aio_tile_geometry.h"

namespace aio {

uint32_t compute_tile_depth(uint32_t nside) {
    if (nside < 16) {
        // NSIDE < 16 时 d=0, tile_nside = nside (整个球面一个 Tile 组)
        return 0;
    }
    // nside 是 2 的幂, 用位运算求 log2(nside)
    int log2_nside = 0;
    uint32_t v = nside;
    while (v > 1) { v >>= 1; log2_nside++; }
    // log2(nside/16) = log2(nside) - 4
    int d = log2_nside - 4;
    if (d < 0) d = 0;
    if (d > 9) d = 9;  // 上限 9
    return (uint32_t)d;
}

uint32_t compute_tile_nside(uint32_t nside) {
    uint32_t d = compute_tile_depth(nside);
    return nside >> d;  // nside / 2^d
}

}  // namespace aio
