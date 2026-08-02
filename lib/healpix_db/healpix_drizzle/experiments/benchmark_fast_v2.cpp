// ============================================================================
// benchmark_fast_v2.cpp - PRECISE vs FAST v2 Drizzle 基准对比 (R08)
//
// 目的:
//   在 R07 的 288 case 矩阵 (6 像素尺度 × 4 pixfrac × 6 天区 × 2 图案) 上,
//   通过共享管线 + 策略接口分别运行 PRECISE (球面裁剪) 与 FAST v2 (分层混合路径),
//   测量耗时并比较累加器差异 (signal/support 误差、候选漏选、通量守恒、面积相对误差),
//   输出 JSONL.
//
// 与 R07 benchmark_precise_fast 的差异:
//   - FAST v1 (切平面) → FAST v2 (分层混合路径)
//   - 新增诊断字段: fast_v2_path, fast_v2_topology, area_rel_err
//   - 路径分布 / 拓扑分布聚合统计
//
// 输出:
//   stdout — 每行一个 JSON 对象 (每个 case × 模式 × replicate 一行,
//            外加每个 case 一行 COMPARISON)
//   stderr — 末尾人类可读汇总
//
// 编译命令 (从 experiments/ 目录执行):
//   g++ -std=c++17 -O2 -fopenmp -Wall -DHAS_LZ4 -DHAS_ZSTD -DAIO_ENABLE_HEALPIX
//       -I.. -I../../healpix_stack -I../../../astro_image_io/include
//       benchmark_fast_v2.cpp
//       ../drizzle_engine.cpp ../wcs_sip.cpp ../poly_clip.cpp ../fits_reader.cpp
//       ../spherical_overlap.cpp ../fast_v2_overlap.cpp
//       ../../healpix_stack/healpix_core.cpp
//       -L../../../astro_image_io -lastro_image_io -lpsapi
//       -static-libgcc -static-libstdc++ -lm -o benchmark_fast_v2.exe
// ============================================================================

#include "drizzle_engine.h"
#include "fits_reader.h"
#include "healpix_core.h"
#include "fast_v2_overlap.h"     // FAST v2
#include "spherical_overlap.h"   // build_drop_polygon_sampled, query_candidate_pixels
#include "wcs_sip.h"             // WcsSip

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <functional>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <omp.h>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace drizzle;

// ============================================================================
// 测试矩阵 (R07 复用)
// ============================================================================

struct ScaleCfg {
    double arcsec;
    int    size;
    int    nside;
    const char* label;
};
static const ScaleCfg kScales[] = {
    {0.1,    4, 4194304, "0p1"},
    {0.5,    8, 524288,  "0p5"},
    {1.0,    8, 262144,  "1p0"},
    {10.0,   8, 32768,   "10"},
    {60.0,   8, 4096,    "60"},
    {3600.0, 16,64,      "3600"},
};

struct PfCfg { double value; const char* label; };
static const PfCfg kPixfracs[] = {
    {0.25, "0p25"},
    {0.5,  "0p5"},
    {0.8,  "0p8"},
    {1.0,  "1p0"},
};

struct SkyRegion { double ra; double dec; const char* label; };
static const SkyRegion kRegions[] = {
    {0.0,    0.0,  "equator"},
    {90.0,   45.0, "midlat"},
    {0.0,    89.0, "north"},
    {0.0,   -89.0, "south"},
    {45.0,   0.0,  "facebound"},
    {359.9,  0.0,  "racross"},
};

enum Pattern { PAT_UNIFORM = 0, PAT_POINT = 1 };
static const char* kPatternLabels[] = { "uniform", "point" };

// ============================================================================
// 常量
// ============================================================================

static const double D2R_BENCH = 0.017453292519943295769;
static const int    FAST_V2_HEALPIX_SAMPLES = 2;
static const int    BENCH_NUM_THREADS = 16;

// ============================================================================
// 合成图像 (TAN, 无 SIP)
// ============================================================================

static FitsImage make_synthetic_image(double scale_arcsec, int size,
                                      double ra0, double dec0, Pattern pat) {
    FitsImage img;
    img.width = size;
    img.height = size;
    img.channels = 1;
    img.pixels.resize(static_cast<size_t>(size) * size);

    if (pat == PAT_UNIFORM) {
        std::fill(img.pixels.begin(), img.pixels.end(), 1000.0f);
    } else {
        std::fill(img.pixels.begin(), img.pixels.end(), 100.0f);
        int cx = size / 2;
        int cy = size / 2;
        img.pixels[static_cast<size_t>(cy) * size + cx] = 10000.0f;
    }

    const double scale_deg = scale_arcsec / 3600.0;
    WcsParams& wcs = img.wcs;
    wcs.has_wcs = true;
    wcs.cd[0] = -scale_deg; wcs.cd[1] = 0.0;
    wcs.cd[2] = 0.0;        wcs.cd[3] = scale_deg;
    wcs.crval[0] = ra0;
    wcs.crval[1] = dec0;
    wcs.crpix[0] = static_cast<double>(size / 2 + 1);
    wcs.crpix[1] = static_cast<double>(size / 2 + 1);
    std::strcpy(wcs.ctype1, "RA---TAN");
    std::strcpy(wcs.ctype2, "DEC--TAN");
    wcs.sip.order = 0;
    wcs.sip.ap_order = 0;

    img.bzero = 0.0;
    img.bscale = 1.0;
    img.photscal = 0.0;
    img.photappl = 0;
    return img;
}

// ============================================================================
// DrizzleConfig
// ============================================================================

static DrizzleConfig make_config(int nside, double pixfrac) {
    DrizzleConfig cfg;
    cfg.nside = nside;
    cfg.nested = true;
    cfg.pixfrac = pixfrac;
    cfg.apply_photometry = false;
    cfg.photscal = 1.0;
    cfg.photometry_applied_upstream = false;
    return cfg;
}

// ============================================================================
// WCS 回调
// ============================================================================

static bool wcsPixelToSkyCallback(double x, double y, double& ra, double& dec,
                                  void* user_data) {
    const WcsSip* wcs = static_cast<const WcsSip*>(user_data);
    wcs->pixelToSky(x, y, ra, dec);
    return std::isfinite(ra) && std::isfinite(dec);
}

// ============================================================================
// 策略接口
// ============================================================================

using OverlapPolicy = double(*)(
    const std::vector<spherical::Vec3>& drop_corners,
    const healpix::HealpixCore& hp,
    uint64_t target_ipix,
    int nside,
    double center_ra,
    double center_dec,
    int healpix_samples);

static double precise_overlap_policy(
    const std::vector<spherical::Vec3>& drop_corners,
    const healpix::HealpixCore& hp, uint64_t target_ipix, int nside,
    double center_ra, double center_dec, int healpix_samples) {
    (void)center_ra; (void)center_dec; (void)healpix_samples; (void)nside;
    return spherical::compute_overlap_area(drop_corners, hp, target_ipix);
}

static double fast_v2_overlap_policy(
    const std::vector<spherical::Vec3>& drop_corners,
    const healpix::HealpixCore& hp, uint64_t target_ipix, int nside,
    double center_ra, double center_dec, int healpix_samples) {
    return fast_v2::compute_overlap_area_v2(drop_corners, hp, target_ipix, nside,
                                             center_ra, center_dec, healpix_samples,
                                             nullptr);
}

// ============================================================================
// 路径标签
// ============================================================================
static const char* path_label(fast_v2::FastV2Diagnostics::Path p) {
    switch (p) {
        case fast_v2::FastV2Diagnostics::COMMON_EXACT:     return "COMMON_EXACT";
        case fast_v2::FastV2Diagnostics::CACHED_APPROX:    return "CACHED_APPROX";
        case fast_v2::FastV2Diagnostics::PRECISE_FALLBACK: return "PRECISE_FALLBACK";
    }
    return "UNKNOWN";
}

static const char* topology_label(int t) {
    switch (t) {
        case fast_v2::FastV2Diagnostics::TOPO_UNKNOWN:          return "UNKNOWN";
        case fast_v2::FastV2Diagnostics::TOPO_DROP_IN_PIXEL:    return "DROP_IN_PIXEL";
        case fast_v2::FastV2Diagnostics::TOPO_PIXEL_IN_DROP:    return "PIXEL_IN_DROP";
        case fast_v2::FastV2Diagnostics::TOPO_2PIXEL_OVERLAP:   return "2PIXEL_OVERLAP";
        case fast_v2::FastV2Diagnostics::TOPO_3PIXEL_OVERLAP:   return "3PIXEL_OVERLAP";
        case fast_v2::FastV2Diagnostics::TOPO_4PIXEL_OVERLAP:   return "4PIXEL_OVERLAP";
        case fast_v2::FastV2Diagnostics::TOPO_COMPLEX:          return "COMPLEX";
        case fast_v2::FastV2Diagnostics::TOPO_PRECISE_FALLBACK: return "PRECISE_FALLBACK";
    }
    return "UNKNOWN";
}

// ============================================================================
// RunResult (与 R07 一致, 增加 FAST v2 路径分布)
// ============================================================================

struct RunResult {
    bool        ok = false;
    std::unordered_map<uint64_t, PixelAccumulator> acc;
    DrizzleStats stats;
    double      wall_ms = 0.0;
    double      cpu_ms = 0.0;
    std::string err;

    double      avg_wall_ms = 0.0;
    double      avg_cpu_ms = 0.0;
    int         iterations = 0;

    double      area_closure_mean = 0.0;
    double      area_closure_max  = 0.0;

    int64_t     total_candidate_pixels = 0;
    int64_t     total_overlap_pixels_nonzero = 0;
    int64_t     total_drops = 0;

    bool        candidate_verified = false;
    int64_t     total_independent_misses = 0;
    int64_t     total_production_candidates = 0;
    int64_t     total_independent_candidates = 0;
    int64_t     total_drops_verified = 0;

    // FAST v2 诊断聚合
    int64_t     v2_path_common_exact = 0;
    int64_t     v2_path_cached_approx = 0;
    int64_t     v2_path_precise_fallback = 0;
    int64_t     v2_topo_drop_in_pixel = 0;
    int64_t     v2_topo_pixel_in_drop = 0;
    int64_t     v2_topo_2pixel = 0;
    int64_t     v2_topo_3pixel = 0;
    int64_t     v2_topo_4pixel = 0;
    int64_t     v2_topo_complex = 0;
    int64_t     v2_topo_precise_fallback = 0;
    int64_t     v2_topo_unknown = 0;
    double      v2_sum_area_rel_err = 0.0;  // Σ |raw-precise|/precise (诊断模式)
    int64_t     v2_area_rel_err_count = 0;
    double      v2_max_area_rel_err = 0.0;
};

struct ThreadStats {
    double sum_area_closure = 0.0;
    double max_area_closure = 0.0;
    int64_t total_drops = 0;
    int64_t total_candidates = 0;
    int64_t total_overlap_nonzero = 0;
    int64_t indep_misses = 0;
    int64_t prod_candidates = 0;
    int64_t indep_candidates = 0;
    int64_t drops_verified = 0;
    // FAST v2 诊断
    int64_t v2_path_common = 0;
    int64_t v2_path_cached = 0;
    int64_t v2_path_precise = 0;
    int64_t v2_topo[8] = {0};
    double  v2_sum_area_rel_err = 0.0;
    int64_t v2_area_rel_err_count = 0;
    double  v2_max_area_rel_err = 0.0;
};

// ============================================================================
// SharedContext
// ============================================================================

struct SharedContext {
    WcsSip wcs;
    healpix::HealpixCore hp;
    std::vector<std::unordered_map<uint64_t, PixelAccumulator>> threadAccums;
    std::vector<ThreadStats> threadStats;
    bool do_verify = false;
    bool valid = false;

    SharedContext(const FitsImage& img, const DrizzleConfig& cfg,
                  bool enable_candidate_verification)
        : wcs(img.wcs)
        , hp(cfg.nside, cfg.nested)
        , do_verify(enable_candidate_verification && (cfg.nside <= 1024)) {
        if (!wcs.hasWcs()) return;
        omp_set_num_threads(BENCH_NUM_THREADS);
        threadAccums.resize(BENCH_NUM_THREADS);
        threadStats.resize(BENCH_NUM_THREADS);
        size_t reserve_buckets = std::max((size_t)256,
            static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * 32);
        for (auto& acc : threadAccums) acc.reserve(reserve_buckets);
        valid = true;
    }

    void reset() {
        for (auto& acc : threadAccums) acc.clear();
        for (auto& ts : threadStats) ts = ThreadStats{};
    }
};

// ============================================================================
// B09 独立候选验证 (低 NSIDE <= 1024) — 与 R07 一致
// ============================================================================

struct CandidateVerification {
    bool     verified = false;
    int64_t  independent_misses = 0;
    int64_t  production_count = 0;
    int64_t  independent_count = 0;
};

static CandidateVerification verify_candidates(
    const std::vector<spherical::Vec3>& drop_corners,
    const healpix::HealpixCore& hp,
    const std::vector<uint64_t>& production_candidates) {

    CandidateVerification cv;
    const int nside = hp.getNside();
    if (nside > 1024) return cv;
    cv.verified = true;

    int nd = (int)drop_corners.size();
    spherical::Vec3 drop_centroid = {0.0, 0.0, 0.0};
    for (const auto& v : drop_corners) {
        drop_centroid.x += v.x; drop_centroid.y += v.y; drop_centroid.z += v.z;
    }
    drop_centroid = spherical::normalize(drop_centroid);
    std::vector<spherical::Vec3> drop_normals;
    drop_normals.reserve(nd);
    for (int j = 0; j < nd; j++) {
        const spherical::Vec3& P1 = drop_corners[j];
        const spherical::Vec3& P2 = drop_corners[(j + 1) % nd];
        spherical::Vec3 n = spherical::cross(P1, P2);
        if (spherical::dot(n, drop_centroid) < 0.0) {
            n.x = -n.x; n.y = -n.y; n.z = -n.z;
        }
        drop_normals.push_back(spherical::normalize(n));
    }

    auto point_in_drop = [&](const spherical::Vec3& v) -> bool {
        for (const auto& n : drop_normals) {
            if (spherical::dot(v, n) < -1e-12) return false;
        }
        return true;
    };

    double ra_min = 360.0, ra_max = 0.0;
    double dec_min = 90.0, dec_max = -90.0;
    for (const auto& v : drop_corners) {
        double ra, dec;
        spherical::vec_to_radec(v, ra, dec);
        if (ra < ra_min) ra_min = ra;
        if (ra > ra_max) ra_max = ra;
        if (dec < dec_min) dec_min = dec;
        if (dec > dec_max) dec_max = dec;
    }
    bool ra_wrap = (ra_max - ra_min) > 180.0;

    double hp_res_deg = hp.pixelResolutionArcsec() / 3600.0;
    double step = hp_res_deg / 4.0;
    if (step <= 0.0) step = 0.001;
    double pad = hp_res_deg;

    std::unordered_set<uint64_t> independent_set;
    double dec_lo = std::max(dec_min - pad, -90.0);
    double dec_hi = std::min(dec_max + pad,  90.0);

    auto check_grid_point = [&](double ra_use, double dec_use) {
        spherical::Vec3 pt = spherical::radec_to_vec(ra_use, dec_use);
        if (!point_in_drop(pt)) return;
        int64_t ipix = hp.radec2pix(ra_use, dec_use);
        if (ipix >= 0) independent_set.insert(static_cast<uint64_t>(ipix));
    };

    for (double dec = dec_lo; dec <= dec_hi; dec += step) {
        for (double ra = ra_min - pad; ra <= ra_max + pad; ra += step) {
            double ra_use = ra;
            while (ra_use < 0.0)    ra_use += 360.0;
            while (ra_use >= 360.0) ra_use -= 360.0;
            check_grid_point(ra_use, dec);
        }
    }
    if (ra_wrap) {
        for (double dec = dec_lo; dec <= dec_hi; dec += step) {
            for (double ra = 0.0; ra <= ra_max + pad; ra += step) {
                check_grid_point(ra, dec);
            }
            for (double ra = ra_min - pad; ra < 360.0; ra += step) {
                double ra_use = (ra < 0.0) ? ra + 360.0 : ra;
                check_grid_point(ra_use, dec);
            }
        }
    }

    std::unordered_set<uint64_t> prod_set(production_candidates.begin(),
                                          production_candidates.end());
    for (uint64_t ipix : independent_set) {
        if (prod_set.find(ipix) == prod_set.end()) {
            cv.independent_misses++;
        }
    }

    cv.production_count  = static_cast<int64_t>(production_candidates.size());
    cv.independent_count = static_cast<int64_t>(independent_set.size());
    return cv;
}

// ============================================================================
// 共享管线 run_drizzle_impl (R07 同款, 增加 FAST v2 诊断收集)
// ============================================================================

static RunResult run_drizzle_impl(const FitsImage& img, const DrizzleConfig& cfg,
                                  SharedContext& ctx, OverlapPolicy policy,
                                  bool is_fast_v2) {
    RunResult r;
    if (!ctx.valid) {
        r.err = "SharedContext init failed (WcsSip)";
        return r;
    }
    ctx.reset();

    const double THRESH_60ARCSEC  = 60.0  * (M_PI / 180.0) / 3600.0;
    const double THRESH_600ARCSEC = 600.0 * (M_PI / 180.0) / 3600.0;

    auto t0 = std::chrono::steady_clock::now();
    std::clock_t c0 = std::clock();

    #pragma omp parallel for schedule(guided)
    for (int y = 0; y < img.height; y++) {
        int tid = omp_get_thread_num();
        auto& localAccum = ctx.threadAccums[tid];
        ThreadStats& ts = ctx.threadStats[tid];

        for (int x = 0; x < img.width; x++) {
            float pixelValue = img.pixels[(size_t)y * img.width + x];
            if (!std::isfinite(pixelValue)) continue;

            double px = (double)x, py = (double)y;
            double half = 0.5 * cfg.pixfrac;

            double corners_xy[4][2] = {
                {px - half, py - half},
                {px + half, py - half},
                {px + half, py + half},
                {px - half, py + half}
            };
            double corners_ra[4], corners_dec[4];
            bool wcs_ok = true;
            for (int i = 0; i < 4; i++) {
                ctx.wcs.pixelToSky(corners_xy[i][0], corners_xy[i][1],
                               corners_ra[i], corners_dec[i]);
                if (!std::isfinite(corners_ra[i]) || !std::isfinite(corners_dec[i])) {
                    wcs_ok = false;
                    break;
                }
            }
            if (!wcs_ok) continue;

            double max_edge_rad = 0.0;
            for (int i = 0; i < 4; i++) {
                int j = (i + 1) % 4;
                double dra  = (corners_ra[j]  - corners_ra[i])  * D2R_BENCH;
                double ddec = (corners_dec[j] - corners_dec[i]) * D2R_BENCH;
                double edge = std::sqrt(dra * dra + ddec * ddec);
                if (edge > max_edge_rad) max_edge_rad = edge;
            }

            int samples_per_edge = 1;
            if (max_edge_rad >= THRESH_600ARCSEC)      samples_per_edge = 8;
            else if (max_edge_rad >= THRESH_60ARCSEC)  samples_per_edge = 4;

            std::vector<spherical::Vec3> drop_corners;
            if (samples_per_edge == 1) {
                drop_corners.resize(4);
                for (int i = 0; i < 4; i++) {
                    drop_corners[i] = spherical::radec_to_vec(corners_ra[i], corners_dec[i]);
                }
            } else {
                drop_corners = spherical::build_drop_polygon_sampled(
                    px, py, cfg.pixfrac,
                    wcsPixelToSkyCallback, &ctx.wcs, samples_per_edge);
                if (drop_corners.empty()) continue;
            }

            double drop_area = spherical::spherical_polygon_area(drop_corners);
            if (!(drop_area > 0.0) || !std::isfinite(drop_area)) continue;

            double center_ra, center_dec;
            ctx.wcs.pixelToSky(px, py, center_ra, center_dec);
            if (!std::isfinite(center_ra) || !std::isfinite(center_dec)) continue;

            std::vector<uint64_t> candidates;
            spherical::query_candidate_pixels(drop_corners, ctx.hp, candidates);
            if (candidates.empty()) continue;

            ts.total_candidates += static_cast<int64_t>(candidates.size());

            if (ctx.do_verify) {
                CandidateVerification cv = verify_candidates(drop_corners, ctx.hp, candidates);
                ts.indep_misses      += cv.independent_misses;
                ts.prod_candidates   += cv.production_count;
                ts.indep_candidates  += cv.independent_count;
                ts.drops_verified    += 1;
            }

            double sum_overlap_for_drop = 0.0;
            for (uint64_t ipix : candidates) {
                // FAST v2 模式: 收集诊断
                fast_v2::FastV2Diagnostics diag;
                double overlap_area;
                if (is_fast_v2) {
                    overlap_area = fast_v2::compute_overlap_area_v2(
                        drop_corners, ctx.hp, ipix, cfg.nside,
                        center_ra, center_dec, FAST_V2_HEALPIX_SAMPLES, &diag);

                    // 路径分布
                    switch (diag.path_used) {
                        case fast_v2::FastV2Diagnostics::COMMON_EXACT:     ts.v2_path_common++;  break;
                        case fast_v2::FastV2Diagnostics::CACHED_APPROX:    ts.v2_path_cached++;  break;
                        case fast_v2::FastV2Diagnostics::PRECISE_FALLBACK: ts.v2_path_precise++; break;
                    }
                    // 拓扑分布
                    if (diag.topology >= 0 && diag.topology < 8) {
                        ts.v2_topo[diag.topology]++;
                    }
                } else {
                    overlap_area = policy(drop_corners, ctx.hp, ipix, cfg.nside,
                                          center_ra, center_dec, FAST_V2_HEALPIX_SAMPLES);
                }

                if (!(overlap_area > 0.0) || !std::isfinite(overlap_area)) continue;

                sum_overlap_for_drop += overlap_area;
                ts.total_overlap_nonzero++;

                double weight = overlap_area / drop_area;
                if (weight <= 0.0) continue;

                auto& acc = localAccum[ipix];
                acc.sumFlux   += (double)pixelValue * weight;
                acc.sumWeight += weight;
                acc.sumArea   += overlap_area;
                acc.nContrib++;
            }

            double closure = (drop_area > 0.0)
                ? std::fabs(sum_overlap_for_drop - drop_area) / drop_area : 0.0;
            ts.sum_area_closure += closure;
            if (closure > ts.max_area_closure) ts.max_area_closure = closure;
            ts.total_drops++;
        }
    }

    for (int t = 0; t < BENCH_NUM_THREADS; t++) {
        for (auto& [ipix, acc] : ctx.threadAccums[t]) {
            auto& dst = r.acc[ipix];
            dst.sumFlux   += acc.sumFlux;
            dst.sumWeight += acc.sumWeight;
            dst.sumArea   += acc.sumArea;
            dst.nContrib  += acc.nContrib;
        }
        const ThreadStats& ts = ctx.threadStats[t];
        r.total_candidate_pixels        += ts.total_candidates;
        r.total_overlap_pixels_nonzero  += ts.total_overlap_nonzero;
        r.total_drops                   += ts.total_drops;
        r.area_closure_mean              += ts.sum_area_closure;
        if (ts.max_area_closure > r.area_closure_max) r.area_closure_max = ts.max_area_closure;
        r.total_independent_misses       += ts.indep_misses;
        r.total_production_candidates    += ts.prod_candidates;
        r.total_independent_candidates  += ts.indep_candidates;
        r.total_drops_verified          += ts.drops_verified;
        // FAST v2 诊断聚合
        r.v2_path_common_exact     += ts.v2_path_common;
        r.v2_path_cached_approx    += ts.v2_path_cached;
        r.v2_path_precise_fallback += ts.v2_path_precise;
        r.v2_topo_drop_in_pixel    += ts.v2_topo[fast_v2::FastV2Diagnostics::TOPO_DROP_IN_PIXEL];
        r.v2_topo_pixel_in_drop    += ts.v2_topo[fast_v2::FastV2Diagnostics::TOPO_PIXEL_IN_DROP];
        r.v2_topo_2pixel           += ts.v2_topo[fast_v2::FastV2Diagnostics::TOPO_2PIXEL_OVERLAP];
        r.v2_topo_3pixel           += ts.v2_topo[fast_v2::FastV2Diagnostics::TOPO_3PIXEL_OVERLAP];
        r.v2_topo_4pixel           += ts.v2_topo[fast_v2::FastV2Diagnostics::TOPO_4PIXEL_OVERLAP];
        r.v2_topo_complex          += ts.v2_topo[fast_v2::FastV2Diagnostics::TOPO_COMPLEX];
        r.v2_topo_precise_fallback += ts.v2_topo[fast_v2::FastV2Diagnostics::TOPO_PRECISE_FALLBACK];
        r.v2_topo_unknown          += ts.v2_topo[fast_v2::FastV2Diagnostics::TOPO_UNKNOWN];
        r.v2_sum_area_rel_err      += ts.v2_sum_area_rel_err;
        r.v2_area_rel_err_count    += ts.v2_area_rel_err_count;
        if (ts.v2_max_area_rel_err > r.v2_max_area_rel_err) {
            r.v2_max_area_rel_err = ts.v2_max_area_rel_err;
        }
    }
    if (r.total_drops > 0) {
        r.area_closure_mean /= static_cast<double>(r.total_drops);
    }
    r.candidate_verified = ctx.do_verify;

    std::clock_t c1 = std::clock();
    auto t1 = std::chrono::steady_clock::now();
    r.wall_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    r.cpu_ms  = static_cast<double>(c1 - c0) / CLOCKS_PER_SEC * 1000.0;
    r.ok = true;
    r.stats.nHealpixPixels = (int64_t)r.acc.size();
    r.stats.nSourcePixels  = (int64_t)img.width * img.height;
    r.stats.nside          = cfg.nside;
    r.stats.nested         = cfg.nested;
    return r;
}

// ============================================================================
// 包装 (单次 / 批量循环)
// ============================================================================

static RunResult run_drizzle_shared(const FitsImage& img, const DrizzleConfig& cfg,
                                    OverlapPolicy policy, bool is_fast_v2,
                                    bool enable_candidate_verification) {
    SharedContext ctx(img, cfg, enable_candidate_verification);
    RunResult r = run_drizzle_impl(img, cfg, ctx, policy, is_fast_v2);
    r.avg_wall_ms = r.wall_ms;
    r.avg_cpu_ms  = r.cpu_ms;
    r.iterations  = 1;
    return r;
}

static RunResult run_with_batch(const FitsImage& img, const DrizzleConfig& cfg,
                                OverlapPolicy policy, bool is_fast_v2,
                                bool enable_candidate_verification) {
    SharedContext ctx(img, cfg, enable_candidate_verification);
    if (!ctx.valid) {
        RunResult fail;
        fail.err = "SharedContext init failed (WcsSip)";
        return fail;
    }

    RunResult last;
    const int MAX_ITERS = 100;
    const double TARGET_MS = 1000.0;

    double total_wall = 0.0, total_cpu = 0.0;
    int iters = 0;

    for (int i = 0; i < MAX_ITERS; i++) {
        RunResult r = run_drizzle_impl(img, cfg, ctx, policy, is_fast_v2);
        if (!r.ok) {
            r.iterations = i + 1;
            return r;
        }
        total_wall += r.wall_ms;
        total_cpu  += r.cpu_ms;
        iters++;
        last = std::move(r);
        if (total_wall >= TARGET_MS) break;
    }

    last.avg_wall_ms = total_wall / static_cast<double>(iters);
    last.avg_cpu_ms  = total_cpu  / static_cast<double>(iters);
    last.iterations  = iters;
    return last;
}

// ============================================================================
// 通量计算
// ============================================================================

static double input_flux(const FitsImage& img) {
    double s = 0.0;
    for (float v : img.pixels) s += static_cast<double>(v);
    return s;
}
static double output_flux(const std::unordered_map<uint64_t, PixelAccumulator>& acc) {
    double s = 0.0;
    for (const auto& kv : acc) s += kv.second.sumFlux;
    return s;
}

// ============================================================================
// 百分位
// ============================================================================

static void compute_percentiles(std::vector<double>& sorted_values,
                                double& p50, double& p95, double& p99) {
    if (sorted_values.empty()) { p50 = p95 = p99 = 0.0; return; }
    std::sort(sorted_values.begin(), sorted_values.end());
    size_t n = sorted_values.size();
    p50 = sorted_values[(size_t)(0.50 * (n - 1))];
    p95 = sorted_values[(size_t)(0.95 * (n - 1))];
    p99 = sorted_values[(size_t)(0.99 * (n - 1))];
}

static int64_t get_peak_rss_bytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<int64_t>(pmc.PeakWorkingSetSize);
    }
    return 0;
#else
    return 0;
#endif
}

// ============================================================================
// PRECISE vs FAST v2 比较
// ============================================================================

struct Comparison {
    bool   valid = false;
    int64_t overlap_pairs = 0;
    int64_t candidate_misses = 0;
    int64_t extra_pixels = 0;

    double signal_mae = 0.0;
    double signal_rmse = 0.0;
    double signal_max_abs = 0.0;
    double signal_p50 = 0.0, signal_p95 = 0.0, signal_p99 = 0.0;

    double signal_rel_mae = 0.0;
    double signal_rel_rmse = 0.0;
    double signal_max_rel = 0.0;

    double support_mae = 0.0;
    double support_rmse = 0.0;
    double support_max_abs = 0.0;
    double support_p50 = 0.0, support_p95 = 0.0, support_p99 = 0.0;

    double support_rel_mae = 0.0;
    double support_max_rel = 0.0;

    double precise_area_closure_mean = 0.0;
    double fast_area_closure_mean = 0.0;
};

static Comparison compute_comparison(const RunResult& precise, const RunResult& fast) {
    Comparison c;
    if (!precise.ok || !fast.ok) return c;
    c.valid = true;

    double sum_abs_flux = 0.0, sum_sq_flux = 0.0, max_abs_flux = 0.0;
    double sum_abs_area = 0.0, sum_sq_area = 0.0, max_abs_area = 0.0;
    double sum_abs_signal_precise = 0.0, sum_sq_signal_precise = 0.0, max_abs_signal_precise = 0.0;
    double sum_abs_support_precise = 0.0, max_abs_support_precise = 0.0;
    int64_t common = 0;
    std::vector<double> flux_diffs, area_diffs;

    for (const auto& kv : precise.acc) {
        const auto& pa = kv.second;
        double sp = std::fabs(pa.sumFlux);
        double ap = std::fabs(pa.sumArea);
        sum_abs_signal_precise += sp;
        sum_sq_signal_precise  += pa.sumFlux * pa.sumFlux;
        if (sp > max_abs_signal_precise) max_abs_signal_precise = sp;
        sum_abs_support_precise += ap;
        if (ap > max_abs_support_precise) max_abs_support_precise = ap;

        auto it = fast.acc.find(kv.first);
        if (it == fast.acc.end()) {
            c.candidate_misses++;
            continue;
        }
        const auto& fa = it->second;
        const double df = pa.sumFlux  - fa.sumFlux;
        const double da = pa.sumArea  - fa.sumArea;
        const double af = std::fabs(df);
        const double aa = std::fabs(da);
        sum_abs_flux += af;
        sum_sq_flux += df * df;
        if (af > max_abs_flux) max_abs_flux = af;
        sum_abs_area += aa;
        sum_sq_area += da * da;
        if (aa > max_abs_area) max_abs_area = aa;
        flux_diffs.push_back(af);
        area_diffs.push_back(aa);
        ++common;
    }

    for (const auto& kv : fast.acc) {
        if (precise.acc.find(kv.first) == precise.acc.end()) c.extra_pixels++;
    }

    c.overlap_pairs = common;
    if (common > 0) {
        c.signal_mae      = sum_abs_flux / static_cast<double>(common);
        c.signal_rmse     = std::sqrt(sum_sq_flux / static_cast<double>(common));
        c.signal_max_abs  = max_abs_flux;
        c.support_mae     = sum_abs_area / static_cast<double>(common);
        c.support_rmse    = std::sqrt(sum_sq_area / static_cast<double>(common));
        c.support_max_abs = max_abs_area;
        compute_percentiles(flux_diffs, c.signal_p50, c.signal_p95, c.signal_p99);
        compute_percentiles(area_diffs, c.support_p50, c.support_p95, c.support_p99);

        double mean_abs_signal = sum_abs_signal_precise / static_cast<double>(common);
        double rms_signal = std::sqrt(sum_sq_signal_precise / static_cast<double>(common));
        double mean_abs_support = sum_abs_support_precise / static_cast<double>(common);

        c.signal_rel_mae  = (mean_abs_signal > 0.0) ? c.signal_mae / mean_abs_signal : 0.0;
        c.signal_rel_rmse = (rms_signal > 0.0)     ? c.signal_rmse / rms_signal : 0.0;
        c.signal_max_rel  = (max_abs_signal_precise > 0.0)
            ? c.signal_max_abs / max_abs_signal_precise : 0.0;
        c.support_rel_mae = (mean_abs_support > 0.0) ? c.support_mae / mean_abs_support : 0.0;
        c.support_max_rel = (max_abs_support_precise > 0.0)
            ? c.support_max_abs / max_abs_support_precise : 0.0;
    }

    c.precise_area_closure_mean = precise.area_closure_mean;
    c.fast_area_closure_mean    = fast.area_closure_mean;
    return c;
}

// ============================================================================
// JSONL 输出
// ============================================================================

static std::string fmt_num(double v) {
    std::ostringstream ss;
    ss << std::setprecision(15) << v;
    return ss.str();
}

static std::string escape_json(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\"': out += "\\\""; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

static void emit_run_jsonl(
    const std::string& case_id, const char* mode, const char* algorithm_id,
    const std::string& parameters_json,
    int nside, double pixfrac,
    const std::string& sky_region, const char* wcs_id,
    double scale_arcsec,
    int64_t source_pixels, int64_t candidate_pixels, int64_t overlap_pixels,
    double input_flux_val, double pre_normalization_area_closure,
    double output_flux_val, double flux_rel_error,
    double wall_ms, double cpu_ms, int64_t peak_rss_bytes,
    int threads, int replicate, bool warmup,
    int exit_code, int error_code,
    bool candidate_verified, int64_t independent_misses,
    // FAST v2 诊断字段 (非 FAST v2 模式全填 0)
    int64_t v2_path_common, int64_t v2_path_cached, int64_t v2_path_precise,
    int64_t v2_topo_drop_in_pixel, int64_t v2_topo_pixel_in_drop,
    int64_t v2_topo_2pixel, int64_t v2_topo_3pixel, int64_t v2_topo_4pixel,
    int64_t v2_topo_complex, int64_t v2_topo_precise_fallback, int64_t v2_topo_unknown,
    double v2_max_area_rel_err) {

    std::ostringstream ss;
    ss << std::setprecision(15);
    ss << "{"
       << "\"case_id\":\"" << escape_json(case_id) << "\","
       << "\"mode\":\"" << mode << "\","
       << "\"algorithm_id\":\"" << algorithm_id << "\","
       << "\"parameters\":" << parameters_json << ","
       << "\"nside\":" << nside << ","
       << "\"pixfrac\":" << fmt_num(pixfrac) << ","
       << "\"sky_region\":\"" << sky_region << "\","
       << "\"wcs_id\":\"" << wcs_id << "\","
       << "\"source_scale\":" << fmt_num(scale_arcsec) << ","
       << "\"source_pixels\":" << source_pixels << ","
       << "\"candidate_pixels\":" << candidate_pixels << ","
       << "\"overlap_pixels\":" << overlap_pixels << ","
       << "\"input_flux\":" << fmt_num(input_flux_val) << ","
       << "\"pre_normalization_area_closure\":" << fmt_num(pre_normalization_area_closure) << ","
       << "\"output_flux\":" << fmt_num(output_flux_val) << ","
       << "\"flux_rel_error\":" << fmt_num(flux_rel_error) << ","
       << "\"signal_mae\":0,"
       << "\"signal_rel_mae\":0,"
       << "\"signal_rmse\":0,"
       << "\"signal_rel_rmse\":0,"
       << "\"signal_max_rel\":0,"
       << "\"support_mae\":0,"
       << "\"support_max\":0,"
       << "\"centroid_error_arcsec\":0,"
       << "\"psf_fwhm_rel_error\":0,"
       << "\"wall_ms\":" << fmt_num(wall_ms) << ","
       << "\"cpu_ms\":" << fmt_num(cpu_ms) << ","
       << "\"peak_rss_bytes\":" << peak_rss_bytes << ","
       << "\"threads\":" << threads << ","
       << "\"replicate\":" << replicate << ","
       << "\"warmup\":" << (warmup ? "true" : "false") << ","
       << "\"exit_code\":" << exit_code << ","
       << "\"error_code\":" << error_code << ","
       << "\"candidate_verified\":" << (candidate_verified ? "true" : "false") << ","
       << "\"independent_misses\":" << independent_misses << ","
       << "\"v2_path_common_exact\":" << v2_path_common << ","
       << "\"v2_path_cached_approx\":" << v2_path_cached << ","
       << "\"v2_path_precise_fallback\":" << v2_path_precise << ","
       << "\"v2_topo_drop_in_pixel\":" << v2_topo_drop_in_pixel << ","
       << "\"v2_topo_pixel_in_drop\":" << v2_topo_pixel_in_drop << ","
       << "\"v2_topo_2pixel\":" << v2_topo_2pixel << ","
       << "\"v2_topo_3pixel\":" << v2_topo_3pixel << ","
       << "\"v2_topo_4pixel\":" << v2_topo_4pixel << ","
       << "\"v2_topo_complex\":" << v2_topo_complex << ","
       << "\"v2_topo_precise_fallback\":" << v2_topo_precise_fallback << ","
       << "\"v2_topo_unknown\":" << v2_topo_unknown << ","
       << "\"v2_max_area_rel_err\":" << fmt_num(v2_max_area_rel_err)
       << "}";
    std::cout << ss.str() << "\n";
}

static void emit_comparison_jsonl(
    const std::string& case_id, const char* algorithm_id,
    const std::string& parameters_json,
    int nside, double pixfrac,
    const std::string& sky_region, const char* wcs_id,
    double scale_arcsec,
    int64_t source_pixels, int64_t candidate_pixels, int64_t overlap_pixels,
    double input_flux_val,
    double precise_area_closure, double fast_area_closure,
    const Comparison& cmp,
    double precise_wall_ms, double fast_wall_ms,
    double precise_avg_wall_ms, double fast_avg_wall_ms,
    int precise_iters, int fast_iters,
    int64_t peak_rss_bytes,
    int threads, double speedup,
    bool candidate_verified, int64_t independent_misses_precise,
    int64_t independent_misses_fast,
    // FAST v2 路径分布 (最后一次 replicate)
    int64_t v2_path_common, int64_t v2_path_cached, int64_t v2_path_precise,
    int64_t v2_topo_drop_in_pixel, int64_t v2_topo_pixel_in_drop,
    int64_t v2_topo_2pixel, int64_t v2_topo_3pixel, int64_t v2_topo_4pixel,
    int64_t v2_topo_complex, int64_t v2_topo_precise_fallback, int64_t v2_topo_unknown,
    double v2_max_area_rel_err) {

    double avg_wall = precise_avg_wall_ms;
    double avg_cpu  = 0.0;
    double out_flux = input_flux_val;
    double fre = 0.0;
    double pre_closure = precise_area_closure;

    std::ostringstream ss;
    ss << std::setprecision(15);
    ss << "{"
       << "\"case_id\":\"" << escape_json(case_id) << "\","
       << "\"mode\":\"COMPARISON\","
       << "\"algorithm_id\":\"" << algorithm_id << "\","
       << "\"parameters\":" << parameters_json << ","
       << "\"nside\":" << nside << ","
       << "\"pixfrac\":" << fmt_num(pixfrac) << ","
       << "\"sky_region\":\"" << sky_region << "\","
       << "\"wcs_id\":\"" << wcs_id << "\","
       << "\"source_scale\":" << fmt_num(scale_arcsec) << ","
       << "\"source_pixels\":" << source_pixels << ","
       << "\"candidate_pixels\":" << candidate_pixels << ","
       << "\"overlap_pixels\":" << overlap_pixels << ","
       << "\"input_flux\":" << fmt_num(input_flux_val) << ","
       << "\"pre_normalization_area_closure\":" << fmt_num(pre_closure) << ","
       << "\"output_flux\":" << fmt_num(out_flux) << ","
       << "\"flux_rel_error\":" << fmt_num(fre) << ","
       << "\"signal_mae\":" << fmt_num(cmp.signal_mae) << ","
       << "\"signal_rel_mae\":" << fmt_num(cmp.signal_rel_mae) << ","
       << "\"signal_rmse\":" << fmt_num(cmp.signal_rmse) << ","
       << "\"signal_rel_rmse\":" << fmt_num(cmp.signal_rel_rmse) << ","
       << "\"signal_max_rel\":" << fmt_num(cmp.signal_max_rel) << ","
       << "\"support_mae\":" << fmt_num(cmp.support_mae) << ","
       << "\"support_max\":" << fmt_num(cmp.support_max_abs) << ","
       << "\"centroid_error_arcsec\":0,"
       << "\"psf_fwhm_rel_error\":0,"
       << "\"wall_ms\":" << fmt_num(avg_wall) << ","
       << "\"cpu_ms\":" << fmt_num(avg_cpu) << ","
       << "\"peak_rss_bytes\":" << peak_rss_bytes << ","
       << "\"threads\":" << threads << ","
       << "\"replicate\":0,"
       << "\"warmup\":false,"
       << "\"exit_code\":" << (cmp.valid ? 0 : 1) << ","
       << "\"error_code\":0,"
       << "\"candidate_verified\":" << (candidate_verified ? "true" : "false") << ","
       << "\"independent_misses\":" << independent_misses_precise << ","
       << "\"overlap_pairs\":" << cmp.overlap_pairs << ","
       << "\"candidate_misses\":" << cmp.candidate_misses << ","
       << "\"extra_pixels\":" << cmp.extra_pixels << ","
       << "\"signal_max_abs\":" << fmt_num(cmp.signal_max_abs) << ","
       << "\"support_rel_mae\":" << fmt_num(cmp.support_rel_mae) << ","
       << "\"support_max_rel\":" << fmt_num(cmp.support_max_rel) << ","
       << "\"precise_area_closure_mean\":" << fmt_num(cmp.precise_area_closure_mean) << ","
       << "\"fast_area_closure_mean\":" << fmt_num(cmp.fast_area_closure_mean) << ","
       << "\"precise_wall_ms\":" << fmt_num(precise_wall_ms) << ","
       << "\"fast_wall_ms\":" << fmt_num(fast_wall_ms) << ","
       << "\"precise_avg_wall_ms\":" << fmt_num(precise_avg_wall_ms) << ","
       << "\"fast_avg_wall_ms\":" << fmt_num(fast_avg_wall_ms) << ","
       << "\"precise_iterations\":" << precise_iters << ","
       << "\"fast_iterations\":" << fast_iters << ","
       << "\"speedup\":" << fmt_num(speedup) << ","
       << "\"independent_misses_fast\":" << independent_misses_fast << ","
       << "\"v2_path_common_exact\":" << v2_path_common << ","
       << "\"v2_path_cached_approx\":" << v2_path_cached << ","
       << "\"v2_path_precise_fallback\":" << v2_path_precise << ","
       << "\"v2_topo_drop_in_pixel\":" << v2_topo_drop_in_pixel << ","
       << "\"v2_topo_pixel_in_drop\":" << v2_topo_pixel_in_drop << ","
       << "\"v2_topo_2pixel\":" << v2_topo_2pixel << ","
       << "\"v2_topo_3pixel\":" << v2_topo_3pixel << ","
       << "\"v2_topo_4pixel\":" << v2_topo_4pixel << ","
       << "\"v2_topo_complex\":" << v2_topo_complex << ","
       << "\"v2_topo_precise_fallback\":" << v2_topo_precise_fallback << ","
       << "\"v2_topo_unknown\":" << v2_topo_unknown << ","
       << "\"v2_max_area_rel_err\":" << fmt_num(v2_max_area_rel_err)
       << "}";
    std::cout << ss.str() << "\n";
}

// ============================================================================
// main
// ============================================================================

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    // R08 快速模式: 环境变量 FAST_V2_QUICK=1 时只跑 NSIDE<=64 的 cases
    //   (快速验证); FAST_V2_QUICK=2 时跑 NSIDE<=4096; 不设置跑全部 288 cases.
    // 原因: NSIDE>4096 时 get_healpix_boundary_sampled 自适应细分阈值极小
    //   (hp_res_rad * 1e-12), 每边细分到 256 段, PRECISE/FAST v2 均极慢.
    const int quick_mode = (std::getenv("FAST_V2_QUICK") ? std::atoi(std::getenv("FAST_V2_QUICK")) : 0);
    const int quick_nside_limit = (quick_mode == 1) ? 64 : (quick_mode == 2 ? 4096 : 0);
    if (quick_mode > 0) {
        std::cerr << "=== QUICK MODE " << quick_mode << ": limiting NSIDE <= " << quick_nside_limit << " ===\n";
    }

    const int kReps = 3;

    int64_t total_cases = 0;
    int64_t both_ok = 0, precise_only_ok = 0, fast_only_ok = 0, both_fail = 0;
    int64_t cases_with_misses = 0;
    int64_t cases_with_indep_misses = 0;
    int64_t total_indep_verified = 0;

    double  max_signal_rel_mae = 0.0, max_signal_rel_rmse = 0.0, max_signal_max_rel = 0.0;
    double  max_support_rel_mae = 0.0, max_support_max_rel = 0.0;
    double  max_flux_rel_err_precise = 0.0;
    double  max_flux_rel_err_fast = 0.0;
    double  max_precise_area_closure = 0.0;
    double  max_fast_area_closure = 0.0;
    double  max_v2_area_rel_err = 0.0;

    std::vector<double> all_speedups;
    double  max_speedup = 0.0, min_speedup = 1e30;
    double  sum_speedup = 0.0;
    int64_t speedup_count = 0;

    // FAST v2 路径分布全局聚合
    int64_t g_v2_path_common = 0, g_v2_path_cached = 0, g_v2_path_precise = 0;
    int64_t g_v2_topo_drop_in_pixel = 0, g_v2_topo_pixel_in_drop = 0;
    int64_t g_v2_topo_2pixel = 0, g_v2_topo_3pixel = 0, g_v2_topo_4pixel = 0;
    int64_t g_v2_topo_complex = 0, g_v2_topo_precise_fallback = 0, g_v2_topo_unknown = 0;

    struct NsideGroupStats {
        int64_t count = 0;
        std::vector<double> speedups;
        std::vector<double> signal_rel_maes;
        std::vector<double> precise_closures;
        std::vector<double> fast_closures;
    };
    std::unordered_map<int, NsideGroupStats> nside_stats;

    std::cerr << "=== benchmark_fast_v2: PRECISE vs FAST v2 (R08) 开始 ===\n";
    std::cerr << "矩阵: " << (sizeof(kScales)/sizeof(kScales[0])) << " 尺度 × "
              << (sizeof(kPixfracs)/sizeof(kPixfracs[0])) << " pixfrac × "
              << (sizeof(kRegions)/sizeof(kRegions[0])) << " 天区 × 2 图案 = "
              << (sizeof(kScales)/sizeof(kScales[0])) *
                 (sizeof(kPixfracs)/sizeof(kPixfracs[0])) *
                 (sizeof(kRegions)/sizeof(kRegions[0])) * 2
              << " cases\n";

    const char* WCS_ID = "TAN_NOPOLY";

    for (const auto& sc : kScales) {
        if (quick_mode > 0 && sc.nside > quick_nside_limit) continue;
        for (const auto& pf : kPixfracs) {
            for (const auto& rg : kRegions) {
                for (int pi = 0; pi < 2; ++pi) {
                    Pattern pat = static_cast<Pattern>(pi);

                    std::string case_id = std::string("ps") + sc.label +
                                          "_pf" + pf.label +
                                          "_" + rg.label +
                                          "_" + kPatternLabels[pi];
                    FitsImage img = make_synthetic_image(sc.arcsec, sc.size,
                                                         rg.ra, rg.dec, pat);
                    const double in_flux = input_flux(img);
                    const int64_t source_pixels = static_cast<int64_t>(sc.size) * sc.size;

                    DrizzleConfig cfg = make_config(sc.nside, pf.value);
                    const bool enable_verify = (sc.nside <= 1024);

                    std::string precise_params = "{\"healpix_samples\":0}";
                    std::ostringstream fast_params_ss;
                    fast_params_ss << "{\"healpix_samples\":" << FAST_V2_HEALPIX_SAMPLES
                                   << ",\"path\":\"hierarchical_hybrid\""
                                   << ",\"common_exact\":\"local_2d_clip\""
                                   << ",\"fallback\":\"spherical_s_h\"}";
                    std::string fast_params = fast_params_ss.str();

                    // ---- PRECISE 模式 ----
                    RunResult precise_runs[3];
                    for (int r = 0; r < kReps; ++r) {
                        if (r == 0) {
                            precise_runs[r] = run_drizzle_shared(img, cfg,
                                precise_overlap_policy, /*is_fast_v2=*/false, enable_verify);
                            precise_runs[r].avg_wall_ms = precise_runs[r].wall_ms;
                            precise_runs[r].avg_cpu_ms  = precise_runs[r].cpu_ms;
                            precise_runs[r].iterations  = 1;
                        } else {
                            precise_runs[r] = run_with_batch(img, cfg,
                                precise_overlap_policy, /*is_fast_v2=*/false, enable_verify);
                        }
                    }

                    // ---- FAST v2 模式 ----
                    RunResult fast_runs[3];
                    for (int r = 0; r < kReps; ++r) {
                        if (r == 0) {
                            fast_runs[r] = run_drizzle_shared(img, cfg,
                                fast_v2_overlap_policy, /*is_fast_v2=*/true, enable_verify);
                            fast_runs[r].avg_wall_ms = fast_runs[r].wall_ms;
                            fast_runs[r].avg_cpu_ms  = fast_runs[r].cpu_ms;
                            fast_runs[r].iterations  = 1;
                        } else {
                            fast_runs[r] = run_with_batch(img, cfg,
                                fast_v2_overlap_policy, /*is_fast_v2=*/true, enable_verify);
                        }
                    }

                    ++total_cases;
                    const RunResult& last_precise = precise_runs[kReps - 1];
                    const RunResult& last_fast   = fast_runs[kReps - 1];
                    const bool p_ok = last_precise.ok;
                    const bool f_ok = last_fast.ok;
                    if (p_ok && f_ok) ++both_ok;
                    else if (p_ok && !f_ok) ++precise_only_ok;
                    else if (!p_ok && f_ok) ++fast_only_ok;
                    else ++both_fail;

                    // 输出 PRECISE 运行行 (v2 诊断字段填 0)
                    for (int r = 0; r < kReps; ++r) {
                        const RunResult& rr = precise_runs[r];
                        const double of = output_flux(rr.acc);
                        const double fre = in_flux > 0.0
                            ? std::fabs(of - in_flux) / in_flux : 0.0;
                        if (r > 0) max_flux_rel_err_precise = std::max(max_flux_rel_err_precise, fre);
                        if (rr.ok) max_precise_area_closure = std::max(max_precise_area_closure, rr.area_closure_mean);
                        emit_run_jsonl(case_id, "PRECISE", "PRECISE_SPHERICAL_SH",
                                       precise_params,
                                       sc.nside, pf.value, rg.label, WCS_ID,
                                       sc.arcsec,
                                       source_pixels,
                                       rr.total_candidate_pixels,
                                       static_cast<int64_t>(rr.acc.size()),
                                       in_flux, rr.area_closure_mean,
                                       of, fre,
                                       rr.avg_wall_ms, rr.avg_cpu_ms,
                                       get_peak_rss_bytes(),
                                       BENCH_NUM_THREADS, r, /*warmup=*/(r == 0),
                                       rr.ok ? 0 : 1, rr.ok ? 0 : 1,
                                       rr.candidate_verified, rr.total_independent_misses,
                                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0);
                    }

                    // 输出 FAST v2 运行行 (含 v2 诊断字段)
                    for (int r = 0; r < kReps; ++r) {
                        const RunResult& rr = fast_runs[r];
                        const double of = output_flux(rr.acc);
                        const double fre = in_flux > 0.0
                            ? std::fabs(of - in_flux) / in_flux : 0.0;
                        if (r > 0) max_flux_rel_err_fast = std::max(max_flux_rel_err_fast, fre);
                        if (rr.ok) max_fast_area_closure = std::max(max_fast_area_closure, rr.area_closure_mean);
                        if (rr.ok) max_v2_area_rel_err = std::max(max_v2_area_rel_err, rr.v2_max_area_rel_err);
                        emit_run_jsonl(case_id, "FAST_V2", "FAST_V2_HYBRID",
                                       fast_params,
                                       sc.nside, pf.value, rg.label, WCS_ID,
                                       sc.arcsec,
                                       source_pixels,
                                       rr.total_candidate_pixels,
                                       static_cast<int64_t>(rr.acc.size()),
                                       in_flux, rr.area_closure_mean,
                                       of, fre,
                                       rr.avg_wall_ms, rr.avg_cpu_ms,
                                       get_peak_rss_bytes(),
                                       BENCH_NUM_THREADS, r, /*warmup=*/(r == 0),
                                       rr.ok ? 0 : 1, rr.ok ? 0 : 1,
                                       rr.candidate_verified, rr.total_independent_misses,
                                       rr.v2_path_common_exact, rr.v2_path_cached_approx,
                                       rr.v2_path_precise_fallback,
                                       rr.v2_topo_drop_in_pixel, rr.v2_topo_pixel_in_drop,
                                       rr.v2_topo_2pixel, rr.v2_topo_3pixel, rr.v2_topo_4pixel,
                                       rr.v2_topo_complex, rr.v2_topo_precise_fallback,
                                       rr.v2_topo_unknown, rr.v2_max_area_rel_err);
                    }

                    // 聚合 v2 路径分布 (最后一次 replicate)
                    g_v2_path_common     += last_fast.v2_path_common_exact;
                    g_v2_path_cached     += last_fast.v2_path_cached_approx;
                    g_v2_path_precise    += last_fast.v2_path_precise_fallback;
                    g_v2_topo_drop_in_pixel    += last_fast.v2_topo_drop_in_pixel;
                    g_v2_topo_pixel_in_drop    += last_fast.v2_topo_pixel_in_drop;
                    g_v2_topo_2pixel           += last_fast.v2_topo_2pixel;
                    g_v2_topo_3pixel           += last_fast.v2_topo_3pixel;
                    g_v2_topo_4pixel           += last_fast.v2_topo_4pixel;
                    g_v2_topo_complex          += last_fast.v2_topo_complex;
                    g_v2_topo_precise_fallback += last_fast.v2_topo_precise_fallback;
                    g_v2_topo_unknown          += last_fast.v2_topo_unknown;

                    // ---- 比较 ----
                    Comparison cmp = compute_comparison(last_precise, last_fast);

                    if (cmp.valid) {
                        if (cmp.candidate_misses > 0) ++cases_with_misses;
                        max_signal_rel_mae  = std::max(max_signal_rel_mae,  cmp.signal_rel_mae);
                        max_signal_rel_rmse = std::max(max_signal_rel_rmse, cmp.signal_rel_rmse);
                        max_signal_max_rel  = std::max(max_signal_max_rel, cmp.signal_max_rel);
                        max_support_rel_mae = std::max(max_support_rel_mae, cmp.support_rel_mae);
                        max_support_max_rel = std::max(max_support_max_rel, cmp.support_max_rel);

                        double p_wall = last_precise.avg_wall_ms;
                        double f_wall = last_fast.avg_wall_ms;
                        double sp = (f_wall > 0.0) ? p_wall / f_wall : 0.0;
                        if (sp > 0.0) {
                            max_speedup = std::max(max_speedup, sp);
                            min_speedup = std::min(min_speedup, sp);
                            sum_speedup += sp;
                            ++speedup_count;
                            all_speedups.push_back(sp);
                        }

                        if (last_precise.candidate_verified) {
                            total_indep_verified++;
                            if (last_precise.total_independent_misses > 0 ||
                                last_fast.total_independent_misses > 0) {
                                ++cases_with_indep_misses;
                            }
                        }

                        NsideGroupStats& ng = nside_stats[sc.nside];
                        ng.count++;
                        if (sp > 0.0) ng.speedups.push_back(sp);
                        ng.signal_rel_maes.push_back(cmp.signal_rel_mae);
                        ng.precise_closures.push_back(cmp.precise_area_closure_mean);
                        ng.fast_closures.push_back(cmp.fast_area_closure_mean);

                        emit_comparison_jsonl(case_id, "FAST_V2_vs_PRECISE",
                            fast_params,
                            sc.nside, pf.value, rg.label, WCS_ID,
                            sc.arcsec,
                            source_pixels,
                            last_precise.total_candidate_pixels,
                            static_cast<int64_t>(last_precise.acc.size()),
                            in_flux,
                            cmp.precise_area_closure_mean, cmp.fast_area_closure_mean,
                            cmp,
                            last_precise.wall_ms, last_fast.wall_ms,
                            last_precise.avg_wall_ms, last_fast.avg_wall_ms,
                            last_precise.iterations, last_fast.iterations,
                            get_peak_rss_bytes(),
                            BENCH_NUM_THREADS, sp,
                            last_precise.candidate_verified,
                            last_precise.total_independent_misses,
                            last_fast.total_independent_misses,
                            last_fast.v2_path_common_exact,
                            last_fast.v2_path_cached_approx,
                            last_fast.v2_path_precise_fallback,
                            last_fast.v2_topo_drop_in_pixel,
                            last_fast.v2_topo_pixel_in_drop,
                            last_fast.v2_topo_2pixel,
                            last_fast.v2_topo_3pixel,
                            last_fast.v2_topo_4pixel,
                            last_fast.v2_topo_complex,
                            last_fast.v2_topo_precise_fallback,
                            last_fast.v2_topo_unknown,
                            last_fast.v2_max_area_rel_err);
                    } else {
                        Comparison empty;
                        emit_comparison_jsonl(case_id, "FAST_V2_vs_PRECISE",
                            fast_params,
                            sc.nside, pf.value, rg.label, WCS_ID,
                            sc.arcsec,
                            source_pixels,
                            last_precise.total_candidate_pixels,
                            static_cast<int64_t>(last_precise.acc.size()),
                            in_flux,
                            0.0, 0.0, empty,
                            0.0, 0.0, 0.0, 0.0, 0, 0,
                            get_peak_rss_bytes(),
                            BENCH_NUM_THREADS, 0.0,
                            last_precise.candidate_verified,
                            last_precise.total_independent_misses,
                            last_fast.total_independent_misses,
                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0);
                    }
                }
            }
        }
    }

    std::cout.flush();

    // ---- 汇总 ----
    double sp_p50 = 0.0, sp_p95 = 0.0, sp_p99 = 0.0;
    compute_percentiles(all_speedups, sp_p50, sp_p95, sp_p99);
    const double mean_speedup = (speedup_count > 0)
        ? sum_speedup / static_cast<double>(speedup_count) : 0.0;

    const bool pass = (both_ok == total_cases)
                      && (cases_with_misses == 0)
                      && (cases_with_indep_misses == 0);

    int64_t total_v2_paths = g_v2_path_common + g_v2_path_cached + g_v2_path_precise;
    int64_t total_v2_topos = g_v2_topo_drop_in_pixel + g_v2_topo_pixel_in_drop +
                              g_v2_topo_2pixel + g_v2_topo_3pixel + g_v2_topo_4pixel +
                              g_v2_topo_complex + g_v2_topo_precise_fallback + g_v2_topo_unknown;

    std::cerr << "\n=== BENCHMARK SUMMARY (PRECISE vs FAST v2, R08) ===\n";
    std::cerr << "Total cases:               " << total_cases << "\n";
    std::cerr << "  Both modes OK:           " << both_ok << "\n";
    std::cerr << "  PRECISE-only OK:         " << precise_only_ok << "\n";
    std::cerr << "  FAST_V2-only OK:         " << fast_only_ok << "\n";
    std::cerr << "  Both failed:             " << both_fail << "\n";
    std::cerr << "Cases with candidate_misses>0: " << cases_with_misses << "\n";
    std::cerr << "Cases with independent_misses>0: " << cases_with_indep_misses
             << " (verified cases: " << total_indep_verified << ")\n";

    std::cerr << "\n--- signal/support 相对误差 ---\n";
    std::cerr << "Max signal_rel_mae:        " << max_signal_rel_mae << "\n";
    std::cerr << "Max signal_rel_rmse:        " << max_signal_rel_rmse << "\n";
    std::cerr << "Max signal_max_rel:         " << max_signal_max_rel << "\n";
    std::cerr << "Max support_rel_mae:        " << max_support_rel_mae << "\n";
    std::cerr << "Max support_max_rel:        " << max_support_max_rel << "\n";

    std::cerr << "\n--- 面积闭合 ---\n";
    std::cerr << "Max PRECISE area_closure:  " << max_precise_area_closure << "\n";
    std::cerr << "Max FAST_V2 area_closure:  " << max_fast_area_closure << "\n";
    std::cerr << "Max FAST_V2 area_rel_err:  " << max_v2_area_rel_err << "\n";

    std::cerr << "\n--- 通量闭合 ---\n";
    std::cerr << "Max flux_rel_error (PRECISE):  " << max_flux_rel_err_precise << "\n";
    std::cerr << "Max flux_rel_error (FAST_V2):  " << max_flux_rel_err_fast << "\n";

    std::cerr << "\n--- 性能加速比 (PRECISE/FAST_V2) ---\n";
    std::cerr << "Speedup P50:               " << sp_p50 << "\n";
    std::cerr << "Speedup P95:               " << sp_p95 << "\n";
    std::cerr << "Speedup P99:               " << sp_p99 << "\n";
    std::cerr << "Mean speedup:              " << mean_speedup << "\n";
    std::cerr << "Min speedup:               " << (min_speedup < 1e30 ? min_speedup : 0.0) << "\n";
    std::cerr << "Max speedup:               " << max_speedup << "\n";

    std::cerr << "\n--- FAST v2 路径分布 (累加全部 cases 最后 replicate) ---\n";
    std::cerr << "Total v2 path decisions:   " << total_v2_paths << "\n";
    if (total_v2_paths > 0) {
        std::cerr << "  COMMON_EXACT:     " << g_v2_path_common
                  << " (" << (100.0 * g_v2_path_common / total_v2_paths) << "%)\n";
        std::cerr << "  CACHED_APPROX:    " << g_v2_path_cached
                  << " (" << (100.0 * g_v2_path_cached / total_v2_paths) << "%)\n";
        std::cerr << "  PRECISE_FALLBACK: " << g_v2_path_precise
                  << " (" << (100.0 * g_v2_path_precise / total_v2_paths) << "%)\n";
    }
    std::cerr << "Total v2 topology decisions: " << total_v2_topos << "\n";
    if (total_v2_topos > 0) {
        std::cerr << "  DROP_IN_PIXEL:    " << g_v2_topo_drop_in_pixel
                  << " (" << (100.0 * g_v2_topo_drop_in_pixel / total_v2_topos) << "%)\n";
        std::cerr << "  PIXEL_IN_DROP:    " << g_v2_topo_pixel_in_drop
                  << " (" << (100.0 * g_v2_topo_pixel_in_drop / total_v2_topos) << "%)\n";
        std::cerr << "  2PIXEL_OVERLAP:   " << g_v2_topo_2pixel
                  << " (" << (100.0 * g_v2_topo_2pixel / total_v2_topos) << "%)\n";
        std::cerr << "  3PIXEL_OVERLAP:   " << g_v2_topo_3pixel
                  << " (" << (100.0 * g_v2_topo_3pixel / total_v2_topos) << "%)\n";
        std::cerr << "  4PIXEL_OVERLAP:   " << g_v2_topo_4pixel
                  << " (" << (100.0 * g_v2_topo_4pixel / total_v2_topos) << "%)\n";
        std::cerr << "  COMPLEX:          " << g_v2_topo_complex
                  << " (" << (100.0 * g_v2_topo_complex / total_v2_topos) << "%)\n";
        std::cerr << "  PRECISE_FALLBACK: " << g_v2_topo_precise_fallback
                  << " (" << (100.0 * g_v2_topo_precise_fallback / total_v2_topos) << "%)\n";
        std::cerr << "  UNKNOWN:          " << g_v2_topo_unknown << "\n";
    }

    std::cerr << "\n--- 按 NSIDE 分组统计 ---\n";
    std::cerr << "NSIDE\t\tcount\tspeedup_p50\tsignal_rel_mae_p50\tprecise_closure\tfast_closure\n";
    for (const auto& kv : nside_stats) {
        const NsideGroupStats& ng = kv.second;
        double sp50 = 0.0, sp95 = 0.0, sp99 = 0.0;
        compute_percentiles(const_cast<std::vector<double>&>(ng.speedups), sp50, sp95, sp99);
        double srm50 = 0.0, srm95 = 0.0, srm99 = 0.0;
        compute_percentiles(const_cast<std::vector<double>&>(ng.signal_rel_maes), srm50, srm95, srm99);
        double pc50 = 0.0, pc95 = 0.0, pc99 = 0.0;
        compute_percentiles(const_cast<std::vector<double>&>(ng.precise_closures), pc50, pc95, pc99);
        double fc50 = 0.0, fc95 = 0.0, fc99 = 0.0;
        compute_percentiles(const_cast<std::vector<double>&>(ng.fast_closures), fc50, fc95, fc99);
        std::cerr << kv.first << "\t\t" << ng.count << "\t"
                  << sp50 << "\t" << srm50 << "\t"
                  << pc50 << "\t" << fc50 << "\n";
    }

    std::cerr << "\nRESULT: " << (pass ? "PASS" : "FAIL") << "\n";
    std::cerr << "=== END SUMMARY ===\n";

    return pass ? 0 : 1;
}
