// ============================================================================
// representative_probe.cpp — V19R4 DRIZZLE_REALISTIC_SINGLE_FRAME_PROBE
//
// 一次代表性 single-frame / bounded production probe（不跑 16 帧）：
// 1024×1024 合成帧（WCS 300"/px、nside=512、pixfrac=0.8、tile_depth=9），
// 记录：source_pixels / candidates / true_overlap / quick_reject /
// candidate efficiency / geometry cache hit-miss / target geometry builds /
// wall time / science hash（signal DATASUM 与快照一致）。
// ============================================================================
#include "drizzle_engine.h"
#include "fits_reader.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace drizzle;

int main() {
    const int W = 1024, H = 1024;
    FitsImage im;
    im.width = W;
    im.height = H;
    im.channels = 1;
    im.wcs.has_wcs = true;
    im.wcs.crval[0] = 10.0;
    im.wcs.crval[1] = 20.0;
    im.wcs.crpix[0] = (double)W * 0.5 + 0.5;
    im.wcs.crpix[1] = (double)H * 0.5 + 0.5;
    const double deg_per_px = 300.0 / 3600.0;
    im.wcs.cd[0] = -deg_per_px;
    im.wcs.cd[1] = 0.0;
    im.wcs.cd[2] = 0.0;
    im.wcs.cd[3] = deg_per_px;
    std::strncpy(im.wcs.ctype1, "RA---TAN", sizeof(im.wcs.ctype1) - 1);
    std::strncpy(im.wcs.ctype2, "DEC--TAN", sizeof(im.wcs.ctype2) - 1);
    im.pixels.assign((std::size_t)H * W, 1000.0f);
    std::mt19937 rng(20260816);
    std::normal_distribution<double> nd(0.0, 10.0);
    for (auto& v : im.pixels) v += (float)nd(rng);

    DrizzleConfig cfg;
    cfg.nside = 512;
    cfg.nested = true;
    cfg.pixfrac = 0.8;
    cfg.threads = 8;
    cfg.apply_photometry = true;
    cfg.photometry_applied_upstream = true;
    cfg.tile_depth = 9;

    std::vector<TileAccumulatorT<float>> tiles;
    DrizzleStats st;
    std::string err;
    DrizzleEngine eng;
    const auto t0 = std::chrono::steady_clock::now();
    const bool ok = eng.drizzleTiled(im, cfg, nullptr, nullptr, nullptr,
                                     tiles, st, err);
    const double wall =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0)
            .count();
    if (!ok) {
        std::printf("[FAIL] drizzle: %s\n", err.c_str());
        return 1;
    }

    const double cand_eff = st.op_candidates > 0
        ? (double)st.op_true_overlaps / (double)st.op_candidates : 0.0;
    std::printf(
        "=== representative single-frame probe ===\n"
        "source_pixels=%lld\ncandidate_total=%lld\ntrue_overlap=%lld\n"
        "quick_reject=%lld\ncandidate_true_ratio=%.3f\n"
        "pix2radec=%lld\nboundary_builds=%lld\ngeometry_builds=%lld\n"
        "target_boundary_builds=%lld\ntarget_geometry_builds=%lld\n"
        "geometry_cache_hits=%lld\ngeometry_cache_misses=%lld\n"
        "cache_hit_rate=%.3f\n"
        "tile_hash_lookups=%lld\nheap_allocations=%lld\n"
        "tiles=%zu\nwall_sec=%.3f\n",
        (long long)st.op_source_pixels, (long long)st.op_candidates,
        (long long)st.op_true_overlaps, (long long)st.op_quick_rejects,
        cand_eff, (long long)st.op_pix2radec,
        (long long)st.op_boundary_builds, (long long)st.op_geometry_builds,
        (long long)st.op_target_boundary_builds,
        (long long)st.op_target_geometry_builds,
        (long long)st.op_geometry_cache_hits,
        (long long)st.op_geometry_cache_misses,
        (st.op_geometry_cache_hits + st.op_geometry_cache_misses) > 0
            ? (double)st.op_geometry_cache_hits /
                  (double)(st.op_geometry_cache_hits +
                           st.op_geometry_cache_misses) : 0.0,
        (long long)st.op_tile_lookups, (long long)st.op_heap_allocations,
        tiles.size(), wall);

    // science 快照：signal 总和（与同输入参考一致性）
    double sum_flux = 0.0;
    std::size_t touched = 0;
    for (const auto& t : tiles) {
        for (uint32_t local : t.touched) {
            const auto& a = t.pixels[(size_t)local];
            sum_flux += (double)a.sumFlux;
            touched += (a.sumArea > 0.0) ? 1 : 0;
        }
    }
    std::printf("science_snapshot: sum_flux=%.6f touched=%zu\n",
                sum_flux, touched);
    return 0;
}
