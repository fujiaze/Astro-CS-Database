// ============================================================================
// healpix_core.h - AstroCS 共享 HEALPix 核心 (Phase1 Final Signoff)
//
// 单一权威 NESTED HEALPix 位置/层级实现, AIO / Drizzle / Browser 共同使用。
// 来源: 依据公开 HEALPix 算法 (Gorski et al. 2005, arXiv:astro-ph/0409513)
// 的独立实现, 由 astropy-healpix (BSD-3-Clause) 作为外部 Oracle
// 以 1,000,000 全天随机点 + 12 base face / 极区 / RA 跨界锚点
// (order 0..22) 交叉验证, mismatch=0。
// 禁止在本模块之外维护第二套 ang2pix/pix2ang。
// ============================================================================

#ifndef ASTROCS_HEALPIX_CORE_H
#define ASTROCS_HEALPIX_CORE_H

#include <cstdint>

namespace astrocs {
namespace healpix {

// (ra_deg, dec_deg) -> NESTED ipix @ nside (nside 必须为 2 的幂)
// ra/dec 单位: 度; ra 任意值(内部归一化), dec ∈ [-90, 90]
uint64_t ang2pix_nest(uint32_t nside, double ra_deg, double dec_deg);

// NESTED ipix @ nside -> (ra_deg, dec_deg), ra ∈ [0, 360), dec ∈ [-90, 90]
// ipix 越界时 ra=dec=0 (与调用方约定一致)
void pix2ang_nest(uint32_t nside, uint64_t ipix, double& ra_deg, double& dec_deg);

// 像素中心大圆角距 (度)
double angular_distance_deg(double ra1, double dec1, double ra2, double dec2);

// ---- NESTED 局部索引 ↔ 二维 xy / 标准 HiPS tile 排列 ----
// tile 为 2^shift × 2^shift 像素 (HiPS 标准 tile_width = 2^shift, 本包 512 → shift=9)。
// NESTED local index (0 .. 4^shift-1) 的位交错解码: x = 偶数位, y = 奇数位
// (与 ang2pix_nest 内部 xy_to_nest/nest_to_xy 同一数学核心, 禁止各模块重写)。
void nested_local_to_xy(uint64_t local, uint32_t shift, uint32_t& x, uint32_t& y);
uint64_t xy_to_nested_local(uint32_t x, uint32_t y, uint32_t shift);

// 标准 HiPS Image tile 二维排列 (IVOA HiPS 1.0 Image tile packaging,
// 由 CDS Hipsgen MAPTILES 外部 Oracle 逐像素冻结):
// FITS 列 (NAXIS1) = y, FITS 行 (NAXIS2) = tile_width-1-x,
// 行主序 fits_index = (tile_width-1-x)*tile_width + y
uint64_t nested_local_to_fits_index(uint64_t local, uint32_t shift, uint32_t tile_width);
// 逆映射: 标准 FITS 行主序 index -> NESTED local index
uint64_t fits_index_to_nested_local(uint64_t fits_index, uint32_t shift, uint32_t tile_width);

// ---- NESTED 层级操作 ----
// 父像素: ipix >> (2*shift), shift = 层级差 (k = order 差)
uint64_t parent_nest(uint64_t ipix, uint32_t shift);

// 子像素: ipix << (2*shift) (调用方保证不溢出目标阶)
uint64_t child_nest(uint64_t ipix, uint32_t shift);

// 由叶级 nside 计算 tile 阶 (log2)
inline uint32_t nside_to_order(uint32_t nside) {
    uint32_t k = 0;
    while ((uint32_t(1) << k) < nside) ++k;
    return k;
}

// 由阶计算 nside
inline uint32_t order_to_nside(uint32_t order) { return uint32_t(1) << order; }

// 叶级 ipix (nside=2^leaf_order) -> tile ipix (nside=2^tile_order), tile_order<=leaf_order
inline uint64_t leaf_to_tile_nest(uint64_t leaf_ipix, uint32_t leaf_order, uint32_t tile_order) {
    const uint32_t shift = (leaf_order >= tile_order) ? (leaf_order - tile_order) : 0;
    return leaf_ipix >> (2u * shift);
}

// tile ipix -> 叶级首像素 (tile_order<=leaf_order)
inline uint64_t tile_to_leaf_nest(uint64_t tile_ipix, uint32_t tile_order, uint32_t leaf_order) {
    const uint32_t shift = (leaf_order >= tile_order) ? (leaf_order - tile_order) : 0;
    return tile_ipix << (2u * shift);
}

} // namespace healpix
} // namespace astrocs

#endif // ASTROCS_HEALPIX_CORE_H

