// ============================================================================
// benchmark_precise_fast.cpp - PRECISE vs FAST Drizzle 模式基准对比 (R07)
//
// 目的:
//   在合成图像矩阵 (像素尺度 × pixfrac × 天区 × 图案) 上, 通过共享管线 +
//   策略接口分别运行 PRECISE (球面裁剪) 与 FAST (切平面投影) 重叠计算,
//   测量耗时并比较累加器差异 (signal/support 误差、候选漏选、通量守恒),
//   输出符合 METRICS_SCHEMA 的 JSONL.
//
// R07 修复:
//   M05: 共享管线 + 策略接口 (消除 run_drizzle_fast 复制 processPixel)
//   M06: 批量循环计时 (单次 < 1000ms 则重复至累计 >= 1000ms 或 100 次)
//        + peak_rss_bytes (Windows GetProcessMemoryInfo)
//   M08: 相对误差指标 (signal_rel_mae / signal_rel_rmse / signal_max_rel /
//        support_rel_mae / support_max_rel)
//   B09: 独立候选验证 (低 NSIDE <= 1024, 用 RA/Dec 网格 + radec2pix 构造
//        独立候选集, 比较 query_candidate_pixels 结果)
//   METRICS_SCHEMA: 每行包含全部必需字段
//
// 输出:
//   stdout  — 每行一个 JSON 对象 (每个 case × 模式 × replicate 一行,
//             外加每个 case 一行 COMPARISON)
//   stderr  — 末尾打印人类可读汇总 (引擎自身的诊断日志也会出现在 stderr)
//
// 编译命令 (从 experiments/ 目录执行):
//   g++ -std=c++17 -O2 -fopenmp -Wall -DHAS_LZ4 -DHAS_ZSTD -DAIO_ENABLE_HEALPIX
//       -I.. -I../../healpix_stack -I../../../astro_image_io/include
//       benchmark_precise_fast.cpp
//       ../drizzle_engine.cpp ../wcs_sip.cpp ../poly_clip.cpp ../fits_reader.cpp
//       ../spherical_overlap.cpp ../fast_overlap.cpp
//       ../../healpix_stack/healpix_core.cpp
//       -L../../../astro_image_io -lastro_image_io -lpsapi
//       -static-libgcc -static-libstdc++ -lm -o benchmark_precise_fast.exe
// ============================================================================

#include "drizzle_engine.h"
#include "fits_reader.h"
#include "healpix_core.h"
#include "fast_overlap.h"        // FAST 切平面重叠计算
#include "spherical_overlap.h"   // build_drop_polygon_sampled, query_candidate_pixels
#include "wcs_sip.h"             // WcsSip

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
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
// 测试矩阵定义 (DATASET_MATRIX.md)
// ============================================================================

// 像素尺度 → 图像尺寸 + NSIDE
// 高 NSIDE 场景缩小图像尺寸以控制 unordered_map 内存
struct ScaleCfg {
    double arcsec;   // 角秒/像素
    int    size;     // 图像边长 (像素)
    int    nside;    // HEALPix nside
    const char* label;
};
static const ScaleCfg kScales[] = {
    {0.1,    4, 4194304, "0p1"},   // 极高 NSIDE, 极小图像 (内存控制)
    {0.5,    8, 524288,  "0p5"},
    {1.0,    8, 262144,  "1p0"},
    {10.0,   8, 32768,   "10"},
    {60.0,   8, 4096,    "60"},
    {3600.0, 16,64,      "3600"},
};

// pixfrac 取值
struct PfCfg { double value; const char* label; };
static const PfCfg kPixfracs[] = {
    {0.25, "0p25"},
    {0.5,  "0p5"},
    {0.8,  "0p8"},
    {1.0,  "1p0"},
};

// 天区 (覆盖赤道/中纬/南北极/面边界/RA 跨越)
struct SkyRegion { double ra; double dec; const char* label; };
static const SkyRegion kRegions[] = {
    {0.0,    0.0,  "equator"},
    {90.0,   45.0, "midlat"},
    {0.0,    89.0, "north"},
    {0.0,   -89.0, "south"},
    {45.0,   0.0,  "facebound"},
    {359.9,  0.0,  "racross"},
};

// 图像图案
enum Pattern { PAT_UNIFORM = 0, PAT_POINT = 1 };
static const char* kPatternLabels[] = { "uniform", "point" };

// ============================================================================
// 常量
// ============================================================================

static const double D2R_BENCH = 0.017453292519943295769;
static const int    FAST_HEALPIX_SAMPLES = 2;  // FAST 建议 1-2
static const int    BENCH_NUM_THREADS    = 16;  // 与 DrizzleEngine 一致

// ============================================================================
// 辅助: 构造合成 FitsImage (TAN 投影, 无 SIP)
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
    } else { // PAT_POINT: 中心一个亮像素, 其余背景
        std::fill(img.pixels.begin(), img.pixels.end(), 100.0f);
        int cx = size / 2;
        int cy = size / 2;
        img.pixels[static_cast<size_t>(cy) * size + cx] = 10000.0f;
    }

    // WCS: 对角 CD 矩阵, cd1_1 = -scale_deg (RA 随 x 增大而减小)
    const double scale_deg = scale_arcsec / 3600.0;
    WcsParams& wcs = img.wcs;
    wcs.has_wcs = true;
    wcs.cd[0] = -scale_deg; wcs.cd[1] = 0.0;
    wcs.cd[2] = 0.0;        wcs.cd[3] = scale_deg;
    wcs.crval[0] = ra0;
    wcs.crval[1] = dec0;
    // CRPIX 1-based, 图像中心
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
// 辅助: 构造 DrizzleConfig
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
// WCS 回调 (与 drizzle_engine.cpp 的 wcsPixelToSkyCallback 一致)
// ============================================================================

static bool wcsPixelToSkyCallback(double x, double y, double& ra, double& dec,
                                  void* user_data) {
    const WcsSip* wcs = static_cast<const WcsSip*>(user_data);
    wcs->pixelToSky(x, y, ra, dec);
    return std::isfinite(ra) && std::isfinite(dec);
}

// ============================================================================
// M05: 策略接口 — 重叠面积计算函数指针
//
// PRECISE 策略: 调用 spherical::compute_overlap_area (忽略 center_ra/dec/samples)
// FAST 策略:    调用 fast::compute_overlap_area_fast (使用全部参数)
//
// 签名: (drop_corners, hp, target_ipix, nside, center_ra, center_dec, healpix_samples)
//   - center_ra/center_dec: drop 中心天球坐标 (FAST 切点, PRECISE 忽略)
//   - healpix_samples: FAST HEALPix 边界采样段数 (PRECISE 忽略)
// ============================================================================

using OverlapPolicy = double(*)(
    const std::vector<spherical::Vec3>& drop_corners,
    const healpix::HealpixCore& hp,
    uint64_t target_ipix,
    int nside,
    double center_ra,
    double center_dec,
    int healpix_samples);

// PRECISE 策略: 球面 Sutherland-Hodgman + Girard
static double precise_overlap_policy(
    const std::vector<spherical::Vec3>& drop_corners,
    const healpix::HealpixCore& hp, uint64_t target_ipix, int nside,
    double center_ra, double center_dec, int healpix_samples) {
    (void)center_ra;
    (void)center_dec;
    (void)healpix_samples;
    (void)nside;
    return spherical::compute_overlap_area(drop_corners, hp, target_ipix);
}

// FAST 策略: 切平面 gnomonic 投影 + 2D 裁剪 + 鞋带 + Jacobian 积分
static double fast_overlap_policy(
    const std::vector<spherical::Vec3>& drop_corners,
    const healpix::HealpixCore& hp, uint64_t target_ipix, int nside,
    double center_ra, double center_dec, int healpix_samples) {
    return fast::compute_overlap_area_fast(drop_corners, hp, target_ipix, nside,
                                           center_ra, center_dec, healpix_samples);
}

// ============================================================================
// B09: 独立候选验证 (低 NSIDE <= 1024)
//
// 从 drop 的球面包围盒 (RA/Dec 范围) 枚举所有可能的像素, 用 hp.radec2pix
// 逐点检查, 构造独立候选集, 比较 query_candidate_pixels 结果.
// ============================================================================

struct CandidateVerification {
    bool     verified = false;          // 是否执行了独立验证 (低 NSIDE)
    int64_t  independent_misses = 0;    // 独立候选集有但生产候选集无的像素数
    int64_t  production_count = 0;      // 生产候选集大小
    int64_t  independent_count = 0;     // 独立候选集大小
};

static CandidateVerification verify_candidates(
    const std::vector<spherical::Vec3>& drop_corners,
    const healpix::HealpixCore& hp,
    const std::vector<uint64_t>& production_candidates) {

    CandidateVerification cv;
    const int nside = hp.getNside();
    // 仅对低 NSIDE 执行独立验证 (高 NSIDE 网格枚举不可行)
    if (nside > 1024) {
        return cv;
    }
    cv.verified = true;

    // 计算 drop 的 RA/Dec 范围 (球面向量 → 天球坐标)
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

    // RA 跨越 0/360 处理: 若 ra_max - ra_min > 180, 说明跨越, 用模运算
    bool ra_wrap = (ra_max - ra_min) > 180.0;

    // 网格步长: HEALPix 像素尺度的 1/4 (确保覆盖每个像素)
    double hp_res_deg = hp.pixelResolutionArcsec() / 3600.0;
    double step = hp_res_deg / 4.0;
    if (step <= 0.0) step = 0.001;  // 防护

    // 边界扩展一个 HEALPix 像素 (缓冲)
    double pad = hp_res_deg;

    std::unordered_set<uint64_t> independent_set;
    double dec_lo = std::max(dec_min - pad, -90.0);
    double dec_hi = std::min(dec_max + pad,  90.0);

    for (double dec = dec_lo; dec <= dec_hi; dec += step) {
        for (double ra = ra_min - pad; ra <= ra_max + pad; ra += step) {
            double ra_use = ra;
            // 归一化 RA 到 [0, 360)
            while (ra_use < 0.0)    ra_use += 360.0;
            while (ra_use >= 360.0) ra_use -= 360.0;
            int64_t ipix = hp.radec2pix(ra_use, dec);
            if (ipix >= 0) {
                independent_set.insert(static_cast<uint64_t>(ipix));
            }
        }
    }
    // RA 跨越时, 还需枚举 [0, ra_max+pad] 和 [ra_min-pad, 360) 的补充区间
    // (上面循环已通过模运算覆盖, 但为稳妥再补一轮)
    if (ra_wrap) {
        for (double dec = dec_lo; dec <= dec_hi; dec += step) {
            for (double ra = 0.0; ra <= ra_max + pad; ra += step) {
                int64_t ipix = hp.radec2pix(ra, dec);
                if (ipix >= 0) independent_set.insert(static_cast<uint64_t>(ipix));
            }
            for (double ra = ra_min - pad; ra < 360.0; ra += step) {
                double ra_use = (ra < 0.0) ? ra + 360.0 : ra;
                int64_t ipix = hp.radec2pix(ra_use, dec);
                if (ipix >= 0) independent_set.insert(static_cast<uint64_t>(ipix));
            }
        }
    }

    // 比较: 独立候选集有但生产候选集无的像素 = independent_misses
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
// 单次 drizzle 运行结果 (含计时 + 面积闭合 + 候选验证聚合)
// ============================================================================

struct RunResult {
    bool        ok = false;
    std::unordered_map<uint64_t, PixelAccumulator> acc;
    DrizzleStats stats;
    double      wall_ms = 0.0;
    double      cpu_ms = 0.0;
    std::string err;

    // M06 批量计时聚合
    double      avg_wall_ms = 0.0;   // 批量循环平均 wall_ms
    double      avg_cpu_ms = 0.0;    // 批量循环平均 cpu_ms
    int         iterations = 0;      // 批量循环次数

    // pre_normalization_area_closure (B01): 每个 drop 的 |Σ overlap - drop_area| / drop_area
    double      area_closure_mean = 0.0;  // 所有 drop 的平均闭合误差
    double      area_closure_max  = 0.0;   // 最大闭合误差

    // 候选统计 (case 级别聚合, 所有源像素 drop)
    int64_t     total_candidate_pixels = 0;   // 所有 drop 的候选总数 (含重复)
    int64_t     total_overlap_pixels_nonzero = 0;  // 有非零重叠的候选数
    int64_t     total_drops = 0;               // 有效 drop 数 (通过面积检查)

    // B09 独立候选验证聚合 (仅低 NSIDE)
    bool        candidate_verified = false;            // 是否执行了独立验证
    int64_t     total_independent_misses = 0;           // 独立漏选总数
    int64_t     total_production_candidates = 0;       // 生产候选总数 (验证的 drop)
    int64_t     total_independent_candidates = 0;      // 独立候选总数
    int64_t     total_drops_verified = 0;              // 验证的 drop 数
};

// ============================================================================
// 线程局部统计 (避免竞争)
// ============================================================================

struct ThreadStats {
    double sum_area_closure = 0.0;
    double max_area_closure = 0.0;
    int64_t total_drops = 0;
    int64_t total_candidates = 0;
    int64_t total_overlap_nonzero = 0;
    // B09
    int64_t indep_misses = 0;
    int64_t prod_candidates = 0;
    int64_t indep_candidates = 0;
    int64_t drops_verified = 0;
};

// ============================================================================
// SharedContext — 共享上下文 (构造一次, 批量循环复用)
//
// M06 优化: WcsSip, HealpixCore 和 threadAccums 构造移出批量循环,
// 避免每次循环重新分配 4M 桶 × 16 线程 = 512MB 内存.
// reserve 按图像大小动态调整 (而非固定 4M).
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
        // 按图像大小动态 reserve (替代固定 4M, 大幅降低内存分配)
        size_t reserve_buckets = std::max((size_t)256,
            static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * 32);
        for (auto& acc : threadAccums) acc.reserve(reserve_buckets);
        valid = true;
    }

    // 每次运行前清空 (保留容量, 避免重新分配)
    void reset() {
        for (auto& acc : threadAccums) acc.clear();
        for (auto& ts : threadStats) ts = ThreadStats{};
    }
};

// ============================================================================
// M05: 共享管线 — run_drizzle_impl
//
// PRECISE 和 FAST 共享: WCS, HEALPix, 并行像素遍历, drop 构造,
// 候选查询, 累加器合并. 唯一区别是重叠计算策略 (policy).
//
// 使用预构造的 SharedContext (避免重复构造 WCS/HEALPix/累加器).
// ============================================================================

static RunResult run_drizzle_impl(const FitsImage& img, const DrizzleConfig& cfg,
                                  SharedContext& ctx, OverlapPolicy policy) {
    RunResult r;

    if (!ctx.valid) {
        r.err = "SharedContext init failed (WcsSip)";
        return r;
    }

    ctx.reset();

    // 自适应边细分阈值 (基于 drop 边角跨度)
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

            // ---- Step 1-2: 像素四角 + pixfrac 收缩 + WCS 映射 ----
            double corners_xy[4][2] = {
                {px - half, py - half},  // 左下
                {px + half, py - half},  // 右下
                {px + half, py + half},  // 右上
                {px - half, py + half}   // 左上
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

            // ---- Step 3: 自适应边细分 (基于 drop 边角跨度) ----
            double max_edge_rad = 0.0;
            for (int i = 0; i < 4; i++) {
                int j = (i + 1) % 4;
                double dra  = (corners_ra[j]  - corners_ra[i])  * D2R_BENCH;
                double ddec = (corners_dec[j] - corners_dec[i]) * D2R_BENCH;
                double edge = std::sqrt(dra * dra + ddec * ddec);
                if (edge > max_edge_rad) max_edge_rad = edge;
            }

            int samples_per_edge = 1;
            if (max_edge_rad >= THRESH_600ARCSEC) {
                samples_per_edge = 8;
            } else if (max_edge_rad >= THRESH_60ARCSEC) {
                samples_per_edge = 4;
            }

            // 构造 drop 球面多边形 (与 PRECISE 共享, 来自 build_drop_polygon_sampled)
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

            // ---- Step 4: drop 球面面积 (Eriksson) ----
            double drop_area = spherical::spherical_polygon_area(drop_corners);
            if (!(drop_area > 0.0) || !std::isfinite(drop_area)) continue;

            // drop 中心天球坐标 (FAST 切平面切点)
            double center_ra, center_dec;
            ctx.wcs.pixelToSky(px, py, center_ra, center_dec);
            if (!std::isfinite(center_ra) || !std::isfinite(center_dec)) continue;

            // ---- Step 5: 候选像素查询 (与 PRECISE 一致) ----
            std::vector<uint64_t> candidates;
            spherical::query_candidate_pixels(drop_corners, ctx.hp, candidates);
            if (candidates.empty()) continue;

            ts.total_candidates += static_cast<int64_t>(candidates.size());

            // ---- B09: 独立候选验证 (仅低 NSIDE) ----
            if (ctx.do_verify) {
                CandidateVerification cv = verify_candidates(drop_corners, ctx.hp, candidates);
                ts.indep_misses      += cv.independent_misses;
                ts.prod_candidates   += cv.production_count;
                ts.indep_candidates  += cv.independent_count;
                ts.drops_verified    += 1;
            }

            // ---- Step 6: 重叠面积计算 (通过 policy) ----
            // 同时计算 pre_normalization_area_closure: Σ_p overlap_area vs drop_area
            double sum_overlap_for_drop = 0.0;
            for (uint64_t ipix : candidates) {
                double overlap_area = policy(drop_corners, ctx.hp, ipix, cfg.nside,
                                             center_ra, center_dec, FAST_HEALPIX_SAMPLES);
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

            // 面积闭合误差: |Σ overlap - drop_area| / drop_area
            double closure = (drop_area > 0.0)
                ? std::fabs(sum_overlap_for_drop - drop_area) / drop_area : 0.0;
            ts.sum_area_closure += closure;
            if (closure > ts.max_area_closure) ts.max_area_closure = closure;
            ts.total_drops++;
        }
    }

    // 4. 合并线程累加器 + 统计
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
// M05: run_drizzle_shared — 单次运行包装
//
// 构造 SharedContext + 调用 run_drizzle_impl.
// 用于 warmup (r==0) 或不需要批量循环的场景.
// ============================================================================

static RunResult run_drizzle_shared(const FitsImage& img, const DrizzleConfig& cfg,
                                    OverlapPolicy policy,
                                    bool enable_candidate_verification) {
    SharedContext ctx(img, cfg, enable_candidate_verification);
    RunResult r = run_drizzle_impl(img, cfg, ctx, policy);
    r.avg_wall_ms = r.wall_ms;
    r.avg_cpu_ms  = r.cpu_ms;
    r.iterations  = 1;
    return r;
}

// ============================================================================
// M06: 批量循环计时
//
// 如果单次 wall_ms < 1000ms, 重复运行直到累计 >= 1000ms 或达到 100 次循环.
// 报告每次循环的平均 wall_ms 和 cpu_ms.
//
// 优化: SharedContext (WcsSip/HealpixCore/threadAccums) 构造一次,
// 批量循环中复用 (仅 clear 不重新分配), 大幅降低内存分配开销.
// ============================================================================

static RunResult run_with_batch(const FitsImage& img, const DrizzleConfig& cfg,
                                OverlapPolicy policy,
                                bool enable_candidate_verification) {
    SharedContext ctx(img, cfg, enable_candidate_verification);
    if (!ctx.valid) {
        RunResult fail;
        fail.err = "SharedContext init failed (WcsSip)";
        return fail;
    }

    RunResult last;  // 保留最后一次结果 (含累加器, 用于比较)
    const int MAX_ITERS = 100;
    const double TARGET_MS = 1000.0;

    double total_wall = 0.0, total_cpu = 0.0;
    int iters = 0;

    for (int i = 0; i < MAX_ITERS; i++) {
        RunResult r = run_drizzle_impl(img, cfg, ctx, policy);
        if (!r.ok) {
            // 失败: 返回失败结果
            r.iterations = i + 1;
            return r;
        }
        total_wall += r.wall_ms;
        total_cpu  += r.cpu_ms;
        iters++;
        last = std::move(r);  // 保留最后一次 (含累加器/统计)
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
// 百分位计算
// ============================================================================

static void compute_percentiles(std::vector<double>& sorted_values,
                                double& p50, double& p95, double& p99) {
    if (sorted_values.empty()) {
        p50 = p95 = p99 = 0.0;
        return;
    }
    std::sort(sorted_values.begin(), sorted_values.end());
    size_t n = sorted_values.size();
    p50 = sorted_values[(size_t)(0.50 * (n - 1))];
    p95 = sorted_values[(size_t)(0.95 * (n - 1))];
    p99 = sorted_values[(size_t)(0.99 * (n - 1))];
}

// ============================================================================
// peak RSS (Windows GetProcessMemoryInfo)
// ============================================================================

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
// M08: PRECISE vs FAST 累加器比较 (含相对误差)
// ============================================================================

struct Comparison {
    bool   valid = false;
    int64_t overlap_pairs = 0;
    int64_t candidate_misses = 0;       // PRECISE 有而 FAST 无
    int64_t extra_pixels = 0;           // FAST 有而 PRECISE 无 (理想为 0)

    // signal (sumFlux) 绝对差异
    double signal_mae = 0.0;
    double signal_rmse = 0.0;
    double signal_max_abs = 0.0;
    double signal_p50 = 0.0, signal_p95 = 0.0, signal_p99 = 0.0;

    // M08: signal 相对差异 (按 PRECISE signal 归一化)
    double signal_rel_mae = 0.0;    // signal_mae / mean(|signal_precise|)
    double signal_rel_rmse = 0.0;   // signal_rmse / rms(signal_precise)
    double signal_max_rel = 0.0;    // signal_max_abs / max(|signal_precise|)

    // support (sumArea) 绝对差异
    double support_mae = 0.0;
    double support_rmse = 0.0;
    double support_max_abs = 0.0;
    double support_p50 = 0.0, support_p95 = 0.0, support_p99 = 0.0;

    // M08: support 相对差异
    double support_rel_mae = 0.0;    // support_mae / mean(|support_precise|)
    double support_max_rel = 0.0;   // support_max_abs / max(|support_precise|)

    // 面积闭合 (PRECISE 参考)
    double precise_area_closure_mean = 0.0;
    double fast_area_closure_mean = 0.0;
};

static Comparison compute_comparison(const RunResult& precise, const RunResult& fast) {
    Comparison c;
    if (!precise.ok || !fast.ok) return c;
    c.valid = true;

    double sum_abs_flux = 0.0, sum_sq_flux = 0.0, max_abs_flux = 0.0;
    double sum_abs_area = 0.0, sum_sq_area = 0.0, max_abs_area = 0.0;
    // PRECISE 参考 (用于相对误差归一化)
    double sum_abs_signal_precise = 0.0, sum_sq_signal_precise = 0.0, max_abs_signal_precise = 0.0;
    double sum_abs_support_precise = 0.0, max_abs_support_precise = 0.0;
    int64_t common = 0;
    std::vector<double> flux_diffs, area_diffs;

    // 先遍历 PRECISE, 计算参考量 + 共同像素差异
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

    // FAST 有而 PRECISE 无的像素
    for (const auto& kv : fast.acc) {
        if (precise.acc.find(kv.first) == precise.acc.end()) {
            c.extra_pixels++;
        }
    }

    c.overlap_pairs = common;
    if (common > 0) {
        c.signal_mae      = sum_abs_flux / static_cast<double>(common);
        c.signal_rmse     = std::sqrt(sum_sq_flux / static_cast<double>(common));
        c.signal_max_abs  = max_abs_flux;
        c.support_mae     = sum_abs_area / static_cast<double>(common);
        c.support_rmse    = std::sqrt(sum_sq_area / static_cast<double>(common));
        c.support_max_abs = max_abs_area;
        compute_percentiles(flux_diffs,
                            c.signal_p50, c.signal_p95, c.signal_p99);
        compute_percentiles(area_diffs,
                            c.support_p50, c.support_p95, c.support_p99);

        // M08: 相对误差 (按 PRECISE 参考量归一化)
        // 注意: mean(|signal_precise|) 用共同像素数, rms(signal_precise) 同理
        double mean_abs_signal = sum_abs_signal_precise / static_cast<double>(common);
        double rms_signal = std::sqrt(sum_sq_signal_precise / static_cast<double>(common));
        double mean_abs_support = sum_abs_support_precise / static_cast<double>(common);

        c.signal_rel_mae  = (mean_abs_signal > 0.0)
            ? c.signal_mae / mean_abs_signal : 0.0;
        c.signal_rel_rmse = (rms_signal > 0.0)
            ? c.signal_rmse / rms_signal : 0.0;
        c.signal_max_rel  = (max_abs_signal_precise > 0.0)
            ? c.signal_max_abs / max_abs_signal_precise : 0.0;
        c.support_rel_mae = (mean_abs_support > 0.0)
            ? c.support_mae / mean_abs_support : 0.0;
        c.support_max_rel = (max_abs_support_precise > 0.0)
            ? c.support_max_abs / max_abs_support_precise : 0.0;
    }

    c.precise_area_closure_mean = precise.area_closure_mean;
    c.fast_area_closure_mean    = fast.area_closure_mean;
    return c;
}

// ============================================================================
// JSONL 输出 (METRICS_SCHEMA 合规)
//
// 必需字段:
// case_id, mode, algorithm_id, parameters, nside, pixfrac, sky_region, wcs_id,
// source_scale, source_pixels, candidate_pixels, overlap_pixels, input_flux,
// pre_normalization_area_closure, output_flux, flux_rel_error,
// signal_mae, signal_rel_mae, signal_rmse, signal_rel_rmse, signal_max_rel,
// support_mae, support_max, centroid_error_arcsec, psf_fwhm_rel_error,
// wall_ms, cpu_ms, peak_rss_bytes, threads, replicate, warmup, exit_code, error_code
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

// 输出 PRECISE/FAST 运行行 (mode = "PRECISE" 或 "FAST")
// run 行: signal/support 误差字段填 0 (单模式运行无对比对象)
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
    bool candidate_verified, int64_t independent_misses) {

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
       // run 行 signal/support 误差填 0 (无对比对象)
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
       << "\"independent_misses\":" << independent_misses
       << "}";
    std::cout << ss.str() << "\n";
}

// 输出 COMPARISON 行 (mode = "COMPARISON")
// 填实际 signal/support 误差值
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
    int64_t independent_misses_fast) {

    // comparison 行的 wall_ms 用 PRECISE 平均 (代表单模式基线)
    double avg_wall = precise_avg_wall_ms;
    double avg_cpu  = 0.0;  // comparison 无独立 cpu, 填 0
    double out_flux = input_flux_val;  // comparison 无独立输出, 用输入作占位
    double fre = 0.0;
    // 面积闭合: 用 PRECISE 参考
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
       << "\"independent_misses_fast\":" << independent_misses_fast
       << "}";
    std::cout << ss.str() << "\n";
}

// ============================================================================
// main
// ============================================================================

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    const int kReps = 3; // replicate 0=warmup, 1,2

    // 汇总统计
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

    // 性能加速比统计
    std::vector<double> all_speedups;
    double  max_speedup = 0.0, min_speedup = 1e30;
    double  sum_speedup = 0.0;
    int64_t speedup_count = 0;

    // 按 NSIDE 分组统计
    struct NsideGroupStats {
        int64_t count = 0;
        std::vector<double> speedups;
        std::vector<double> signal_rel_maes;
        std::vector<double> precise_closures;
        std::vector<double> fast_closures;
    };
    std::unordered_map<int, NsideGroupStats> nside_stats;

    std::cerr << "=== benchmark_precise_fast: PRECISE vs FAST (R07) 开始 ===\n";
    std::cerr << "矩阵: " << (sizeof(kScales)/sizeof(kScales[0])) << " 尺度 × "
              << (sizeof(kPixfracs)/sizeof(kPixfracs[0])) << " pixfrac × "
              << (sizeof(kRegions)/sizeof(kRegions[0])) << " 天区 × 2 图案 = "
              << (sizeof(kScales)/sizeof(kScales[0])) *
                 (sizeof(kPixfracs)/sizeof(kPixfracs[0])) *
                 (sizeof(kRegions)/sizeof(kRegions[0])) * 2
              << " cases\n";

    const char* WCS_ID = "TAN_NOPOLY";  // 合成图像: TAN 无 SIP

    for (const auto& sc : kScales) {
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

                    // B09: 仅低 NSIDE 启用独立候选验证
                    const bool enable_verify = (sc.nside <= 1024);

                    // PRECISE/FAST 共享参数 JSON
                    std::string precise_params = "{\"healpix_samples\":0}";
                    std::ostringstream fast_params_ss;
                    fast_params_ss << "{\"healpix_samples\":" << FAST_HEALPIX_SAMPLES
                                   << ",\"projection\":\"gnomonic\""
                                   << ",\"clip\":\"2d_sutherland_hodgman\""
                                   << ",\"area_integration\":\"triangle_gauss3\"}";
                    std::string fast_params = fast_params_ss.str();

                    // ---- PRECISE 模式 (共享管线 + precise 策略) ----
                    RunResult precise_runs[3];
                    for (int r = 0; r < kReps; ++r) {
                        // warmup (r==0) 不做批量循环, 单次即可
                        if (r == 0) {
                            precise_runs[r] = run_drizzle_shared(img, cfg,
                                precise_overlap_policy, enable_verify);
                            precise_runs[r].avg_wall_ms = precise_runs[r].wall_ms;
                            precise_runs[r].avg_cpu_ms  = precise_runs[r].cpu_ms;
                            precise_runs[r].iterations  = 1;
                        } else {
                            precise_runs[r] = run_with_batch(img, cfg,
                                precise_overlap_policy, enable_verify);
                        }
                    }

                    // ---- FAST 模式 (共享管线 + fast 策略) ----
                    RunResult fast_runs[3];
                    for (int r = 0; r < kReps; ++r) {
                        if (r == 0) {
                            fast_runs[r] = run_drizzle_shared(img, cfg,
                                fast_overlap_policy, enable_verify);
                            fast_runs[r].avg_wall_ms = fast_runs[r].wall_ms;
                            fast_runs[r].avg_cpu_ms  = fast_runs[r].cpu_ms;
                            fast_runs[r].iterations  = 1;
                        } else {
                            fast_runs[r] = run_with_batch(img, cfg,
                                fast_overlap_policy, enable_verify);
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

                    // 输出 PRECISE 运行行
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
                                       rr.candidate_verified, rr.total_independent_misses);
                    }

                    // 输出 FAST 运行行
                    for (int r = 0; r < kReps; ++r) {
                        const RunResult& rr = fast_runs[r];
                        const double of = output_flux(rr.acc);
                        const double fre = in_flux > 0.0
                            ? std::fabs(of - in_flux) / in_flux : 0.0;
                        if (r > 0) max_flux_rel_err_fast = std::max(max_flux_rel_err_fast, fre);
                        if (rr.ok) max_fast_area_closure = std::max(max_fast_area_closure, rr.area_closure_mean);
                        emit_run_jsonl(case_id, "FAST", "FAST_GNOMONIC_2D",
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
                                       rr.candidate_verified, rr.total_independent_misses);
                    }

                    // ---- 比较 (基于最后一次非 warmup replicate) ----
                    Comparison cmp = compute_comparison(last_precise, last_fast);

                    if (cmp.valid) {
                        if (cmp.candidate_misses > 0) ++cases_with_misses;
                        max_signal_rel_mae  = std::max(max_signal_rel_mae,  cmp.signal_rel_mae);
                        max_signal_rel_rmse  = std::max(max_signal_rel_rmse, cmp.signal_rel_rmse);
                        max_signal_max_rel   = std::max(max_signal_max_rel, cmp.signal_max_rel);
                        max_support_rel_mae  = std::max(max_support_rel_mae, cmp.support_rel_mae);
                        max_support_max_rel  = std::max(max_support_max_rel, cmp.support_max_rel);

                        // 加速比 (用最后一次 replicate 的平均 wall_ms)
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

                        // B09 独立候选验证统计
                        if (last_precise.candidate_verified) {
                            total_indep_verified++;
                            if (last_precise.total_independent_misses > 0 ||
                                last_fast.total_independent_misses > 0) {
                                ++cases_with_indep_misses;
                            }
                        }

                        // NSIDE 分组
                        NsideGroupStats& ng = nside_stats[sc.nside];
                        ng.count++;
                        if (sp > 0.0) ng.speedups.push_back(sp);
                        ng.signal_rel_maes.push_back(cmp.signal_rel_mae);
                        ng.precise_closures.push_back(cmp.precise_area_closure_mean);
                        ng.fast_closures.push_back(cmp.fast_area_closure_mean);

                        emit_comparison_jsonl(case_id, "FAST_vs_PRECISE",
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
                            last_fast.total_independent_misses);
                    } else {
                        // 其中一个失败, 仍输出 comparison 行 (零值)
                        Comparison empty;
                        emit_comparison_jsonl(case_id, "FAST_vs_PRECISE",
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
                            last_fast.total_independent_misses);
                    }
                }
            }
        }
    }

    std::cout.flush();

    // ---- 百分位加速比 ----
    double sp_p50 = 0.0, sp_p95 = 0.0, sp_p99 = 0.0;
    compute_percentiles(all_speedups, sp_p50, sp_p95, sp_p99);
    const double mean_speedup = (speedup_count > 0)
        ? sum_speedup / static_cast<double>(speedup_count) : 0.0;

    const bool pass = (both_ok == total_cases)
                      && (cases_with_misses == 0)
                      && (cases_with_indep_misses == 0);

    std::cerr << "\n=== BENCHMARK SUMMARY (PRECISE vs FAST, R07) ===\n";
    std::cerr << "Total cases:               " << total_cases << "\n";
    std::cerr << "  Both modes OK:           " << both_ok << "\n";
    std::cerr << "  PRECISE-only OK:         " << precise_only_ok << "\n";
    std::cerr << "  FAST-only OK:            " << fast_only_ok << "\n";
    std::cerr << "  Both failed:             " << both_fail << "\n";
    std::cerr << "Cases with candidate_misses>0: " << cases_with_misses << "\n";
    std::cerr << "Cases with independent_misses>0: " << cases_with_indep_misses
             << " (verified cases: " << total_indep_verified << ")\n";
    std::cerr << "\n--- M08: signal/support 相对误差 ---\n";
    std::cerr << "Max signal_rel_mae:        " << max_signal_rel_mae << "\n";
    std::cerr << "Max signal_rel_rmse:        " << max_signal_rel_rmse << "\n";
    std::cerr << "Max signal_max_rel:         " << max_signal_max_rel << "\n";
    std::cerr << "Max support_rel_mae:        " << max_support_rel_mae << "\n";
    std::cerr << "Max support_max_rel:        " << max_support_max_rel << "\n";
    std::cerr << "\n--- B01: 面积闭合 (pre_normalization_area_closure) ---\n";
    std::cerr << "Max PRECISE area_closure:  " << max_precise_area_closure << "\n";
    std::cerr << "Max FAST area_closure:     " << max_fast_area_closure << "\n";
    std::cerr << "\n--- 通量闭合 ---\n";
    std::cerr << "Max flux_rel_error (PRECISE): " << max_flux_rel_err_precise << "\n";
    std::cerr << "Max flux_rel_error (FAST):    " << max_flux_rel_err_fast << "\n";
    std::cerr << "\n--- M06: 性能加速比 (PRECISE/FAST, 用批量平均 wall_ms) ---\n";
    std::cerr << "Speedup P50:               " << sp_p50 << "\n";
    std::cerr << "Speedup P95:               " << sp_p95 << "\n";
    std::cerr << "Speedup P99:               " << sp_p99 << "\n";
    std::cerr << "Mean speedup:              " << mean_speedup << "\n";
    std::cerr << "Min speedup:               " << (min_speedup < 1e30 ? min_speedup : 0.0) << "\n";
    std::cerr << "Max speedup:               " << max_speedup << "\n";

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
