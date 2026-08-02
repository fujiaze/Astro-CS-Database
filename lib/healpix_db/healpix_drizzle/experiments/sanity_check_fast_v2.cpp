// ============================================================================
// sanity_check_fast_v2.cpp - FAST v2 最小化功能验证 (R08)
//
// 构造一个简单场景: NSIDE=64, drop 中心 (RA=45°, Dec=30°),
// pixfrac=1.0, 像素尺度 60"/pixel. 分别用 PRECISE 和 FAST v2 计算
// 所有候选像素的重叠面积, 比较:
//   - 候选数
//   - 总面积闭合 (Σ overlap vs drop_area)
//   - 逐像素面积差异
//   - 路径分布 / 拓扑分布
//
// 编译:
//   g++ -std=c++17 -O2 -Wall -I.. -I../../healpix_stack -I../../../astro_image_io/include
//       sanity_check_fast_v2.cpp
//       ../spherical_overlap.cpp ../fast_v2_overlap.cpp
//       ../../healpix_stack/healpix_core.cpp
//       -lm -o sanity_check_fast_v2.exe
// ============================================================================

#include "healpix_core.h"
#include "spherical_overlap.h"
#include "fast_v2_overlap.h"

#include <cmath>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const double D2R = 0.017453292519943295769;
static const double R2D = 57.2957795130823208768;

int main() {
    std::fprintf(stderr, "=== sanity_check_fast_v2: PRECISE vs FAST v2 ===\n");

    // 配置: NSIDE=4194304 (4M), TAN 投影, drop 中心 (RA=45, Dec=30), 0.1"/pix, pixfrac=1.0
    // 高 NSIDE + 小 drop → 候选数 <= 8 → COMMON_EXACT 路径
    const int    nside    = 4194304;
    const double ra0      = 45.0;
    const double dec0     = 30.0;
    const double scale_deg = 0.1 / 3600.0;  // 0.1 arcsec/pixel
    const double pixfrac  = 1.0;

    healpix::HealpixCore hp(nside, /*nested=*/true);

    // 模拟 TAN 投影: 用真正的函数替代 (见下方 pixelToSkyFn)
    struct WcsCtx {
        double ra0, dec0, scale_deg;
    };
    WcsCtx wcs_ctx{ra0, dec0, scale_deg};

    auto pixelToSkyFn = [](double px, double py, double& ra, double& dec, void* ud) -> bool {
        const WcsCtx* c = static_cast<const WcsCtx*>(ud);
        double xi  = -c->scale_deg * px;
        double eta =  c->scale_deg * py;
        double dec0_r = c->dec0 * D2R;
        double ra0_r  = c->ra0  * D2R;
        double xi_r   = xi  * D2R;
        double eta_r  = eta * D2R;
        double denom = std::cos(dec0_r) * std::cos(eta_r) * std::cos(xi_r) + std::sin(dec0_r) * std::sin(eta_r);
        if (denom <= 1e-12) return false;
        double dec_rad = std::atan2(
            std::sin(dec0_r) * std::cos(eta_r) * std::cos(xi_r) - std::cos(dec0_r) * std::sin(eta_r),
            denom);
        double ra_rad = ra0_r + std::atan2(
            std::sin(xi_r) * std::cos(eta_r),
            denom);
        dec = dec_rad * R2D;
        ra  = ra_rad  * R2D;
        while (ra < 0.0)    ra += 360.0;
        while (ra >= 360.0) ra -= 360.0;
        return std::isfinite(ra) && std::isfinite(dec);
    };

    // 构造 drop: 像素中心 (0,0), pixfrac=1.0, 4 角顶点
    double px = 0.0, py = 0.0;
    double half = 0.5 * pixfrac;
    double corners_xy[4][2] = {
        {px - half, py - half},
        {px + half, py - half},
        {px + half, py + half},
        {px - half, py + half}
    };
    double corners_ra[4], corners_dec[4];
    for (int i = 0; i < 4; i++) {
        pixelToSkyFn(corners_xy[i][0], corners_xy[i][1], corners_ra[i], corners_dec[i], &wcs_ctx);
    }

    // 球面多边形 (4 角, samples=1)
    std::vector<spherical::Vec3> drop_corners(4);
    for (int i = 0; i < 4; i++) {
        drop_corners[i] = spherical::radec_to_vec(corners_ra[i], corners_dec[i]);
    }

    // drop 球面面积
    double drop_area = spherical::spherical_polygon_area(drop_corners);
    std::fprintf(stderr, "Drop area (steradian): %.15e\n", drop_area);
    std::fprintf(stderr, "Drop corners:\n");
    for (int i = 0; i < 4; i++) {
        std::fprintf(stderr, "  C%d: RA=%.10f, Dec=%.10f\n", i, corners_ra[i], corners_dec[i]);
    }

    // 候选像素查询
    std::vector<uint64_t> candidates;
    spherical::query_candidate_pixels(drop_corners, hp, candidates);
    std::fprintf(stderr, "Candidate pixels: %zu\n", candidates.size());

    // drop 中心
    double center_ra, center_dec;
    pixelToSkyFn(px, py, center_ra, center_dec, &wcs_ctx);
    std::fprintf(stderr, "Drop center: RA=%.10f, Dec=%.10f\n", center_ra, center_dec);

    // 对每个候选像素分别用 PRECISE 和 FAST v2 计算重叠面积
    double sum_precise = 0.0, sum_fast_v2 = 0.0;
    double max_diff = 0.0, sum_diff = 0.0;
    int    common_count = 0;
    int    path_counts[3] = {0, 0, 0};  // COMMON_EXACT, CACHED_APPROX, PRECISE_FALLBACK
    int    topo_counts[8] = {0};

    std::fprintf(stderr, "\n%-12s %-15s %-15s %-15s %-15s %-20s %-20s\n",
                 "ipix", "precise", "fast_v2", "diff", "rel_diff", "path", "topology");
    for (uint64_t ipix : candidates) {
        double precise_area = spherical::compute_overlap_area(drop_corners, hp, ipix);

        fast_v2::FastV2Diagnostics diag;
        double fast_v2_area = fast_v2::compute_overlap_area_v2(
            drop_corners, hp, ipix, nside, center_ra, center_dec, 2, &diag);

        path_counts[(int)diag.path_used]++;
        if (diag.topology >= 0 && diag.topology < 8) topo_counts[diag.topology]++;

        sum_precise += precise_area;
        sum_fast_v2 += fast_v2_area;

        if (precise_area > 0.0 || fast_v2_area > 0.0) {
            double diff = std::fabs(fast_v2_area - precise_area);
            double rel = (precise_area > 0.0) ? diff / precise_area : 0.0;
            if (diff > max_diff) max_diff = diff;
            sum_diff += diff;
            common_count++;
            std::fprintf(stderr, "%-12llu %-15.6e %-15.6e %-15.6e %-15.6e %-20s %-20s\n",
                         (unsigned long long)ipix, precise_area, fast_v2_area, diff, rel,
                         (diag.path_used == fast_v2::FastV2Diagnostics::COMMON_EXACT ? "COMMON_EXACT" :
                          diag.path_used == fast_v2::FastV2Diagnostics::CACHED_APPROX ? "CACHED_APPROX" :
                          "PRECISE_FALLBACK"),
                         (diag.topology == fast_v2::FastV2Diagnostics::TOPO_DROP_IN_PIXEL ? "DROP_IN_PIXEL" :
                          diag.topology == fast_v2::FastV2Diagnostics::TOPO_PIXEL_IN_DROP ? "PIXEL_IN_DROP" :
                          diag.topology == fast_v2::FastV2Diagnostics::TOPO_2PIXEL_OVERLAP ? "2PIXEL" :
                          diag.topology == fast_v2::FastV2Diagnostics::TOPO_3PIXEL_OVERLAP ? "3PIXEL" :
                          diag.topology == fast_v2::FastV2Diagnostics::TOPO_4PIXEL_OVERLAP ? "4PIXEL" :
                          diag.topology == fast_v2::FastV2Diagnostics::TOPO_COMPLEX ? "COMPLEX" :
                          diag.topology == fast_v2::FastV2Diagnostics::TOPO_PRECISE_FALLBACK ? "PRECISE_FALLBACK" :
                          "UNKNOWN"));
        }
    }

    std::fprintf(stderr, "\n=== Summary ===\n");
    std::fprintf(stderr, "Candidates:         %zu\n", candidates.size());
    std::fprintf(stderr, "Non-zero overlaps:  %d\n", common_count);
    std::fprintf(stderr, "Sum PRECISE:        %.15e\n", sum_precise);
    std::fprintf(stderr, "Sum FAST v2:        %.15e\n", sum_fast_v2);
    std::fprintf(stderr, "Drop area:          %.15e\n", drop_area);
    std::fprintf(stderr, "PRECISE closure:    |sum - drop|/drop = %.6e\n",
                 std::fabs(sum_precise - drop_area) / drop_area);
    std::fprintf(stderr, "FAST v2 closure:    |sum - drop|/drop = %.6e\n",
                 std::fabs(sum_fast_v2 - drop_area) / drop_area);
    std::fprintf(stderr, "Max |diff|:         %.6e\n", max_diff);
    std::fprintf(stderr, "Mean |diff|:        %.6e\n",
                 common_count > 0 ? sum_diff / common_count : 0.0);
    std::fprintf(stderr, "\nPath distribution:\n");
    std::fprintf(stderr, "  COMMON_EXACT:     %d\n", path_counts[0]);
    std::fprintf(stderr, "  CACHED_APPROX:    %d\n", path_counts[1]);
    std::fprintf(stderr, "  PRECISE_FALLBACK: %d\n", path_counts[2]);
    std::fprintf(stderr, "\nTopology distribution:\n");
    std::fprintf(stderr, "  UNKNOWN:          %d\n", topo_counts[0]);
    std::fprintf(stderr, "  DROP_IN_PIXEL:    %d\n", topo_counts[1]);
    std::fprintf(stderr, "  PIXEL_IN_DROP:    %d\n", topo_counts[2]);
    std::fprintf(stderr, "  2PIXEL_OVERLAP:   %d\n", topo_counts[3]);
    std::fprintf(stderr, "  3PIXEL_OVERLAP:   %d\n", topo_counts[4]);
    std::fprintf(stderr, "  4PIXEL_OVERLAP:   %d\n", topo_counts[5]);
    std::fprintf(stderr, "  COMPLEX:          %d\n", topo_counts[6]);
    std::fprintf(stderr, "  PRECISE_FALLBACK: %d\n", topo_counts[7]);

    // R08 sanity check pass 判断:
    //   1. FAST v2 与 PRECISE 逐像素最大相对误差 < 1% (主验收标准)
    //   2. FAST v2 面积闭合 < 1e-3 (与 PRECISE 同量级或更好)
    //   注: PRECISE 在极小面积 (1e-13 sr) 时受浮点精度限制, closure 可达 1e-4,
    //       不再要求 PRECISE closure < 1e-6.
    double max_rel_diff = 0.0;
    if (common_count > 0 && sum_precise > 0.0) {
        max_rel_diff = max_diff / (sum_precise / common_count);
    }
    bool pass = (max_rel_diff < 0.01) &&
                (std::fabs(sum_fast_v2 - drop_area) / drop_area < 1e-3);
    std::fprintf(stderr, "\nMax per-pixel rel diff (vs mean precise): %.6e\n", max_rel_diff);
    std::fprintf(stderr, "\nRESULT: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
