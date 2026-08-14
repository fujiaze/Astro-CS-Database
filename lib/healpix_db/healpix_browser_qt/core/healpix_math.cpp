// ============================================================================
// healpix_math.cpp - Browser HEALPix 工具（V15 单路径收敛）
//
// V15：删除第二套手写 NESTED 映射（healpix_core.h 明确禁止模块外维护
// ang2pix/pix2ang）。pix2ang_nest / ang2pix_nest / angular_distance 全部
// 委托共享 canonical core（astrocs::healpix，astropy-healpix 1M 点 Oracle
// mismatch=0）；query_disc / ud_grade 为浏览器局部工具，构建在 canonical
// 映射之上。
// ============================================================================

#include "healpix_math.h"
#include "logger.h"

#include "healpix/healpix_core.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

// ============================================================================
// pix2ang_nest: 委托 canonical core（同一数学核心，禁止再手写）
// ============================================================================

void HealpixMath::pix2ang_nest(uint32_t nside, uint64_t ipix,
                               double& ra, double& dec) {
    astrocs::healpix::pix2ang_nest(nside, ipix, ra, dec);
}

// ============================================================================
// ang2pix_nest: 委托 canonical core
// ============================================================================

uint64_t HealpixMath::ang2pix_nest(uint32_t nside, double ra, double dec) {
    return astrocs::healpix::ang2pix_nest(nside, ra, dec);
}

// ============================================================================
// query_disc: 圆盘查询（浏览器局部工具；构建在 canonical 映射之上）
// ============================================================================

std::vector<uint64_t> HealpixMath::query_disc(uint32_t nside,
                                               double ra, double dec,
                                               double radius_deg) {
    std::vector<uint64_t> result;
    if (nside == 0) return result;

    const uint64_t total = 12ULL * (uint64_t)nside * (uint64_t)nside;
    result.reserve(1024);
    for (uint64_t ipix = 0; ipix < total; ipix++) {
        double p_ra, p_dec;
        astrocs::healpix::pix2ang_nest(nside, ipix, p_ra, p_dec);
        const double dist = astrocs::healpix::angular_distance_deg(
            ra, dec, p_ra, p_dec);
        if (dist <= radius_deg) result.push_back(ipix);
    }
    return result;
}

// ============================================================================
// ud_grade: NESTED 降采样（位运算，与 canonical NESTED 布局一致）
// ============================================================================

HealpixMath::GradeResult HealpixMath::ud_grade(uint32_t src_nside,
                                                const std::vector<uint64_t>& src_ipix,
                                                const std::vector<float>& src_pixel,
                                                uint32_t target_nside) {
    GradeResult result;
    result.nside = target_nside;

    if (src_nside == 0 || target_nside == 0 || target_nside > src_nside)
        return result;
    const uint32_t ratio = src_nside / target_nside;
    if (ratio == 0) return result;
    if ((ratio & (ratio - 1)) != 0) {
        LOG_WARN("ud_grade: src_nside/target_nside=%u/%u 不是 2 的幂",
                 src_nside, target_nside);
        return result;
    }
    int shift = 0;
    uint32_t r = ratio;
    while (r > 1) { shift++; r >>= 1; }
    const int bit_shift = 2 * shift;

    std::unordered_map<uint64_t, std::pair<double, uint64_t>> agg;
    agg.reserve(src_ipix.size());
    const std::size_t n = std::min(src_ipix.size(), src_pixel.size());
    for (std::size_t i = 0; i < n; i++) {
        const uint64_t ipix_coarse = src_ipix[i] >> bit_shift;
        auto& entry = agg[ipix_coarse];
        entry.first += src_pixel[i];
        entry.second += 1;
    }
    result.ipix.reserve(agg.size());
    result.pixel.reserve(agg.size());
    for (const auto& kv : agg) {
        result.ipix.push_back(kv.first);
        result.pixel.push_back(
            (float)(kv.second.first / (double)kv.second.second));
    }
    return result;
}

// ============================================================================
// angular_distance: 委托 canonical core
// ============================================================================

double HealpixMath::angular_distance(double ra1, double dec1,
                                     double ra2, double dec2) {
    return astrocs::healpix::angular_distance_deg(ra1, dec1, ra2, dec2);
}
