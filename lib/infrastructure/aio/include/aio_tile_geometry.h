#ifndef AIO_TILE_GEOMETRY_H
#define AIO_TILE_GEOMETRY_H

// ============================================================================
// aio_tile_geometry.h - HEALPix Tile 自适应层级几何
//
// 本头文件从已退场的 hiss_format.h 迁出。迁出的理由: tile 层级规则是
// **标准 HiPS 产品面**的布局规则 (分块深度/分块 nside 决定 512×512 leaf
// 的组织方式), 与 .hiss / .hcsd 容器格式无关。生产 drizzle 引擎
// (drizzle_engine.cpp eff_tile_depth / drizzleTiledImpl) 一直在用它,
// 不能随容器格式一并退场。
//
// 规则 (权威 = docs/detail 与 docs/science 的 tile 层级条款):
//   d           = min(9, log2(NSIDE/16))
//   tile_nside  = NSIDE / 2^d
// 保证: 满 Tile 最多 4^9 = 262144 叶像素, tile_nside >= 16
// ============================================================================

#include <cstdint>

#ifndef AIO_GEOMETRY_EXPORT
#if defined(_WIN32)
#define AIO_GEOMETRY_EXPORT __declspec(dllexport)
#else
#define AIO_GEOMETRY_EXPORT __attribute__((visibility("default")))
#endif
#endif

namespace aio {

// Tile 自适应层级深度。nside < 16 时返回 0 (整个球面一个 Tile 组)。
AIO_GEOMETRY_EXPORT uint32_t compute_tile_depth(uint32_t nside);

// Tile 子块 nside = nside / 2^d。
AIO_GEOMETRY_EXPORT uint32_t compute_tile_nside(uint32_t nside);

}  // namespace aio

#endif  // AIO_TILE_GEOMETRY_H
