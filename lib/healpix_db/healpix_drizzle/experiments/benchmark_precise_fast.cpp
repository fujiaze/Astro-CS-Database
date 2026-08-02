// ============================================================================
// benchmark_precise_fast.cpp - PRECISE vs FAST Drizzle 模式基准对比
//
// 目的:
//   在合成图像矩阵 (像素尺度 × pixfrac × 天区 × 图案) 上, 分别运行
//   DrizzleEngine::drizzle (PRECISE 球面裁剪) 与手动 FAST 切平面裁剪,
//   测量耗时并比较累加器差异 (signal/support 误差、候选缺失、通量守恒),
//   输出 JSONL.
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
//       -L../../../astro_image_io -lastro_image_io -static-libgcc -static-libstdc++
//       -lm -o benchmark_precise_fast.exe
// ============================================================================

#include "drizzle_engine.h"
#include "fits_reader.h"
#include "healpix_core.h"
#include "fast_overlap.h"        // FAST 切平面重叠计算
#include "spherical_overlap.h"   // build_drop_polygon_sampled, query_candidate_pixels
#include "wcs_sip.h"             // WcsSip (FAST runner 需要)

#include <chrono>
#include <ctime>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <omp.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace drizzle;

// ============================================================================
// 测试矩阵定义
// ============================================================================

// 像素尺度 → 图像尺寸 + NSIDE
// R06 修正: 按 DATASET_MATRIX.md 正确 NSIDE (R05 用 32768/512 是错误的)
// 高 NSIDE 场景缩小图像尺寸以控制 unordered_map 内存
//   (0.1" + nside=4194304: 每源像素 ~1.35M HEALPix 像素)
struct ScaleCfg {
    double arcsec;   // 角秒/像素
    int    size;     // 图像边长 (像素)
    int    nside;    // HEALPix nside
    const char* label;
};
static const ScaleCfg kScales[] = {
    {0.1,    4, 4194304, "0p1"},   // R06: 32768 → 4194304, size 64→4
    {0.5,    8, 524288,  "0p5"},   // R06: 32768 → 524288,  size 64→8
    {1.0,    8, 262144,  "1p0"},   // R06: 32768 → 262144,  size 64→8
    {10.0,   8, 32768,   "10"},    // R06: size 32→8 (控制内存)
    {60.0,   8, 4096,    "60"},    // R06: size 32→8 (控制内存)
    {3600.0, 16,64,      "3600"},  // R06: 512 → 64
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
// 单次 drizzle 运行结果 (含计时)
// ============================================================================
struct RunResult {
    bool        ok = false;
    std::unordered_map<uint64_t, PixelAccumulator> acc;
    DrizzleStats stats;
    double      wall_ms = 0.0;
    double      cpu_ms = 0.0;
    std::string err;
};

// PRECISE 模式: 通过 DrizzleEngine 调用 (球面 Sutherland-Hodgman + Eriksson)
static RunResult run_drizzle_precise(const FitsImage& img, const DrizzleConfig& cfg) {
    RunResult r;
    DrizzleEngine engine;
    auto t0 = std::chrono::steady_clock::now();
    std::clock_t c0 = std::clock();
    r.ok = engine.drizzle(img, cfg, nullptr, nullptr, r.acc, r.stats, r.err);
    std::clock_t c1 = std::clock();
    auto t1 = std::chrono::steady_clock::now();
    r.wall_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    r.cpu_ms  = static_cast<double>(c1 - c0) / CLOCKS_PER_SEC * 1000.0;
    return r;
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
// FAST 模式: 手动复制 DrizzleEngine::processPixel 流水线 (Step 1-5 与 PRECISE
// 完全共享), 仅 Step 6 用 fast::compute_overlap_area_fast 替代
// spherical::compute_overlap_area.
//
// 关键: drop 多边形构造、drop 面积、候选查询均与 PRECISE 一致, 保证唯一变量
// 是重叠面积计算函数. FAST 使用切平面 gnomonic 投影 + 2D 裁剪 + 鞋带公式.
// ============================================================================
static const double D2R_BENCH = 0.017453292519943295769;
static const int    FAST_HEALPIX_SAMPLES = 2;  // FAST 建议 1-2
static const int    BENCH_NUM_THREADS    = 16;  // 与 DrizzleEngine 一致

static RunResult run_drizzle_fast(const FitsImage& img, const DrizzleConfig& cfg) {
    RunResult r;

    // 1. WCS
    WcsSip wcs(img.wcs);
    if (!wcs.hasWcs()) {
        r.err = "WcsSip init failed";
        return r;
    }

    // 2. HEALPix 核心
    healpix::HealpixCore hp(cfg.nside, cfg.nested);

    // 3. 并行设置 (与 DrizzleEngine 一致)
    omp_set_num_threads(BENCH_NUM_THREADS);
    std::vector<std::unordered_map<uint64_t, PixelAccumulator>> threadAccums(BENCH_NUM_THREADS);
    for (auto& acc : threadAccums) acc.reserve(1 << 22);

    // 自适应边细分阈值 (与 drizzle_engine.cpp processPixel 一致)
    const double THRESH_60ARCSEC  = 60.0  * (M_PI / 180.0) / 3600.0;
    const double THRESH_600ARCSEC = 600.0 * (M_PI / 180.0) / 3600.0;

    auto t0 = std::chrono::steady_clock::now();
    std::clock_t c0 = std::clock();

    #pragma omp parallel for schedule(guided)
    for (int y = 0; y < img.height; y++) {
        int tid = omp_get_thread_num();
        auto& localAccum = threadAccums[tid];

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
                wcs.pixelToSky(corners_xy[i][0], corners_xy[i][1],
                               corners_ra[i], corners_dec[i]);
                if (!std::isfinite(corners_ra[i]) || !std::isfinite(corners_dec[i])) {
                    wcs_ok = false;
                    break;
                }
            }
            if (!wcs_ok) continue;

            // ---- Step 3: 自适应边细分 (与 processPixel 一致) ----
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

            // 构造 drop 球面多边形顶点 (与 PRECISE 共享)
            std::vector<spherical::Vec3> drop_corners;
            if (samples_per_edge == 1) {
                drop_corners.resize(4);
                for (int i = 0; i < 4; i++) {
                    drop_corners[i] = spherical::radec_to_vec(corners_ra[i], corners_dec[i]);
                }
            } else {
                drop_corners = spherical::build_drop_polygon_sampled(
                    px, py, cfg.pixfrac,
                    wcsPixelToSkyCallback, &wcs, samples_per_edge);
                if (drop_corners.empty()) continue;
            }

            // ---- Step 4: drop 球面面积 (Eriksson, 与 PRECISE 一致) ----
            // R06: 用 > 0 相对判据替代 < 1e-20 硬阈值
            double drop_area = spherical::spherical_polygon_area(drop_corners);
            if (!(drop_area > 0.0)) continue;

            // drop 中心天球坐标 (FAST 切平面切点)
            double center_ra, center_dec;
            wcs.pixelToSky(px, py, center_ra, center_dec);
            if (!std::isfinite(center_ra) || !std::isfinite(center_dec)) continue;

            // ---- Step 5: 候选像素查询 (与 PRECISE 一致) ----
            std::vector<uint64_t> candidates;
            spherical::query_candidate_pixels(drop_corners, hp, candidates);
            if (candidates.empty()) continue;

            // ---- Step 6: FAST 重叠面积计算 (切平面 + 2D 裁剪) ----
            for (uint64_t ipix : candidates) {
                double overlap_area = fast::compute_overlap_area_fast(
                    drop_corners, hp, ipix, cfg.nside,
                    center_ra, center_dec, FAST_HEALPIX_SAMPLES);
                if (!(overlap_area > 0.0)) continue;

                double weight = overlap_area / drop_area;
                if (weight <= 0.0) continue;

                auto& acc = localAccum[ipix];
                acc.sumFlux   += (double)pixelValue * weight;
                acc.sumWeight += weight;
                acc.sumArea   += overlap_area;
                acc.nContrib++;
            }
        }
    }

    // 4. 合并线程累加器
    for (int t = 0; t < BENCH_NUM_THREADS; t++) {
        for (auto& [ipix, acc] : threadAccums[t]) {
            auto& dst = r.acc[ipix];
            dst.sumFlux   += acc.sumFlux;
            dst.sumWeight += acc.sumWeight;
            dst.sumArea   += acc.sumArea;
            dst.nContrib  += acc.nContrib;
        }
    }

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
// PRECISE vs FAST 累加器比较 (含百分位)
// ============================================================================
struct Comparison {
    bool   valid = false;
    int64_t overlap_pairs = 0;
    int64_t candidate_misses = 0;       // PRECISE 有而 FAST 无
    int64_t extra_pixels = 0;           // FAST 有而 PRECISE 无 (理想为 0)
    // signal (sumFlux) 差异
    double signal_mae = 0.0;
    double signal_rmse = 0.0;
    double signal_max_abs = 0.0;
    double signal_p50 = 0.0, signal_p95 = 0.0, signal_p99 = 0.0;
    // support (sumArea) 差异
    double support_mae = 0.0;
    double support_rmse = 0.0;
    double support_max_abs = 0.0;
    double support_p50 = 0.0, support_p95 = 0.0, support_p99 = 0.0;
};

static Comparison compute_comparison(const RunResult& precise, const RunResult& fast) {
    Comparison c;
    if (!precise.ok || !fast.ok) return c;
    c.valid = true;

    double sum_abs_flux = 0.0, sum_sq_flux = 0.0, max_abs_flux = 0.0;
    double sum_abs_area = 0.0, sum_sq_area = 0.0, max_abs_area = 0.0;
    int64_t common = 0;
    std::vector<double> flux_diffs, area_diffs;

    for (const auto& kv : precise.acc) {
        const auto& pa = kv.second;
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
    }
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

// 输出 PRECISE/FAST 运行行
static void emit_run_jsonl(const std::string& case_id, const char* mode,
                           int nside, double pixfrac, double scale_arcsec,
                           int64_t source_pixels, int64_t output_pixels,
                           double wall_ms, double cpu_ms,
                           double in_flux, double out_flux, double flux_rel_err,
                           int exit_code, int replicate, bool warmup) {
    std::ostringstream ss;
    ss << std::setprecision(15);
    ss << "{\"event\":\"run\","
       << "\"case_id\":\"" << case_id << "\","
       << "\"mode\":\"" << mode << "\","
       << "\"nside\":" << nside << ","
       << "\"pixfrac\":" << fmt_num(pixfrac) << ","
       << "\"source_pixel_scale_arcsec\":" << fmt_num(scale_arcsec) << ","
       << "\"source_pixels\":" << source_pixels << ","
       << "\"output_pixels\":" << output_pixels << ","
       << "\"wall_ms\":" << fmt_num(wall_ms) << ","
       << "\"cpu_ms\":" << fmt_num(cpu_ms) << ","
       << "\"input_flux\":" << fmt_num(in_flux) << ","
       << "\"output_flux\":" << fmt_num(out_flux) << ","
       << "\"flux_rel_error\":" << fmt_num(flux_rel_err) << ","
       << "\"exit_code\":" << exit_code << ","
       << "\"replicate\":" << replicate << ","
       << "\"warmup\":" << (warmup ? "true" : "false")
       << "}";
    std::cout << ss.str() << "\n";
}

// 输出 COMPARISON 行 (每个 case 一行, 基于最后一次非 warmup replicate)
static void emit_cmp_jsonl(const std::string& case_id,
                           int nside, double pixfrac, double scale_arcsec,
                           const Comparison& cmp,
                           double precise_wall_ms, double fast_wall_ms,
                           double speedup) {
    std::ostringstream ss;
    ss << std::setprecision(15);
    ss << "{\"event\":\"comparison\","
       << "\"case_id\":\"" << case_id << "\","
       << "\"nside\":" << nside << ","
       << "\"pixfrac\":" << fmt_num(pixfrac) << ","
       << "\"source_pixel_scale_arcsec\":" << fmt_num(scale_arcsec) << ","
       << "\"overlap_pairs\":" << cmp.overlap_pairs << ","
       << "\"candidate_misses\":" << cmp.candidate_misses << ","
       << "\"extra_pixels\":" << cmp.extra_pixels << ","
       << "\"signal_mae\":" << fmt_num(cmp.signal_mae) << ","
       << "\"signal_rmse\":" << fmt_num(cmp.signal_rmse) << ","
       << "\"signal_max_abs\":" << fmt_num(cmp.signal_max_abs) << ","
       << "\"signal_p50\":" << fmt_num(cmp.signal_p50) << ","
       << "\"signal_p95\":" << fmt_num(cmp.signal_p95) << ","
       << "\"signal_p99\":" << fmt_num(cmp.signal_p99) << ","
       << "\"support_mae\":" << fmt_num(cmp.support_mae) << ","
       << "\"support_rmse\":" << fmt_num(cmp.support_rmse) << ","
       << "\"support_max_abs\":" << fmt_num(cmp.support_max_abs) << ","
       << "\"support_p50\":" << fmt_num(cmp.support_p50) << ","
       << "\"support_p95\":" << fmt_num(cmp.support_p95) << ","
       << "\"support_p99\":" << fmt_num(cmp.support_p99) << ","
       << "\"precise_wall_ms\":" << fmt_num(precise_wall_ms) << ","
       << "\"fast_wall_ms\":" << fmt_num(fast_wall_ms) << ","
       << "\"speedup\":" << fmt_num(speedup)
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
    double  max_signal_mae = 0.0, max_signal_max_abs = 0.0;
    double  max_support_mae = 0.0, max_support_max_abs = 0.0;
    double  max_flux_rel_err_precise = 0.0;
    double  max_flux_rel_err_fast = 0.0;
    double  sum_wall_precise = 0.0, sum_wall_fast = 0.0; // 不含 warmup
    double  max_speedup = 0.0, min_speedup = 1e30;
    double  sum_speedup = 0.0;
    int64_t speedup_count = 0;

    std::cerr << "=== benchmark_precise_fast: PRECISE vs FAST 开始 ===\n";

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

                    // ---- PRECISE 模式 (DrizzleEngine) ----
                    RunResult precise_runs[3];
                    for (int r = 0; r < kReps; ++r) {
                        precise_runs[r] = run_drizzle_precise(img, cfg);
                    }

                    // ---- FAST 模式 (手动调用 fast_overlap) ----
                    RunResult fast_runs[3];
                    for (int r = 0; r < kReps; ++r) {
                        fast_runs[r] = run_drizzle_fast(img, cfg);
                    }

                    ++total_cases;
                    const bool p_ok = precise_runs[kReps - 1].ok;
                    const bool f_ok = fast_runs[kReps - 1].ok;
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
                        if (r > 0) sum_wall_precise += rr.wall_ms;
                        if (r > 0) max_flux_rel_err_precise = std::max(max_flux_rel_err_precise, fre);
                        emit_run_jsonl(case_id, "PRECISE",
                                       sc.nside, pf.value, sc.arcsec,
                                       source_pixels,
                                       static_cast<int64_t>(rr.acc.size()),
                                       rr.wall_ms, rr.cpu_ms,
                                       in_flux, of, fre,
                                       rr.ok ? 0 : 1, r, /*warmup=*/(r == 0));
                    }

                    // 输出 FAST 运行行
                    for (int r = 0; r < kReps; ++r) {
                        const RunResult& rr = fast_runs[r];
                        const double of = output_flux(rr.acc);
                        const double fre = in_flux > 0.0
                            ? std::fabs(of - in_flux) / in_flux : 0.0;
                        if (r > 0) sum_wall_fast += rr.wall_ms;
                        if (r > 0) max_flux_rel_err_fast = std::max(max_flux_rel_err_fast, fre);
                        emit_run_jsonl(case_id, "FAST",
                                       sc.nside, pf.value, sc.arcsec,
                                       source_pixels,
                                       static_cast<int64_t>(rr.acc.size()),
                                       rr.wall_ms, rr.cpu_ms,
                                       in_flux, of, fre,
                                       rr.ok ? 0 : 1, r, /*warmup=*/(r == 0));
                    }

                    // ---- 比较 (基于最后一次非 warmup replicate) ----
                    Comparison cmp = compute_comparison(
                        precise_runs[kReps - 1], fast_runs[kReps - 1]);

                    if (cmp.valid) {
                        if (cmp.candidate_misses > 0) ++cases_with_misses;
                        max_signal_mae      = std::max(max_signal_mae, cmp.signal_mae);
                        max_signal_max_abs  = std::max(max_signal_max_abs, cmp.signal_max_abs);
                        max_support_mae     = std::max(max_support_mae, cmp.support_mae);
                        max_support_max_abs = std::max(max_support_max_abs, cmp.support_max_abs);

                        // 加速比 (用最后一次 replicate 的 wall_ms)
                        double p_wall = precise_runs[kReps - 1].wall_ms;
                        double f_wall = fast_runs[kReps - 1].wall_ms;
                        double sp = (f_wall > 0.0) ? p_wall / f_wall : 0.0;
                        if (sp > 0.0) {
                            max_speedup = std::max(max_speedup, sp);
                            min_speedup = std::min(min_speedup, sp);
                            sum_speedup += sp;
                            ++speedup_count;
                        }

                        emit_cmp_jsonl(case_id, sc.nside, pf.value, sc.arcsec,
                                       cmp, p_wall, f_wall, sp);
                    } else {
                        // 其中一个失败, 仍输出 comparison 行 (零值)
                        Comparison empty;
                        emit_cmp_jsonl(case_id, sc.nside, pf.value, sc.arcsec,
                                       empty, 0.0, 0.0, 0.0);
                    }
                }
            }
        }
    }

    std::cout.flush();

    // ---- 汇总到 stderr ----
    const double mean_speedup = (speedup_count > 0)
        ? sum_speedup / static_cast<double>(speedup_count) : 0.0;
    const bool pass = (both_ok == total_cases) && (cases_with_misses == 0);

    std::cerr << "\n=== BENCHMARK SUMMARY (PRECISE vs FAST) ===\n";
    std::cerr << "Total cases:               " << total_cases << "\n";
    std::cerr << "  Both modes OK:           " << both_ok << "\n";
    std::cerr << "  PRECISE-only OK:         " << precise_only_ok << "\n";
    std::cerr << "  FAST-only OK:            " << fast_only_ok << "\n";
    std::cerr << "  Both failed:             " << both_fail << "\n";
    std::cerr << "Cases with candidate_misses>0: " << cases_with_misses << "\n";
    std::cerr << "Max signal_mae:            " << max_signal_mae << "\n";
    std::cerr << "Max signal_max_abs:        " << max_signal_max_abs << "\n";
    std::cerr << "Max support_mae:           " << max_support_mae << "\n";
    std::cerr << "Max support_max_abs:       " << max_support_max_abs << "\n";
    std::cerr << "Max flux_rel_error (PRECISE): " << max_flux_rel_err_precise << "\n";
    std::cerr << "Max flux_rel_error (FAST):    " << max_flux_rel_err_fast << "\n";
    std::cerr << "Total wall_ms (PRECISE, ex warmup): " << sum_wall_precise << "\n";
    std::cerr << "Total wall_ms (FAST, ex warmup):    " << sum_wall_fast << "\n";
    std::cerr << "Mean speedup (PRECISE/FAST):        " << mean_speedup << "\n";
    std::cerr << "Min speedup:                        " << (min_speedup < 1e30 ? min_speedup : 0.0) << "\n";
    std::cerr << "Max speedup:                        " << max_speedup << "\n";
    std::cerr << "RESULT: " << (pass ? "PASS" : "FAIL") << "\n";
    std::cerr << "=== END SUMMARY ===\n";

    return pass ? 0 : 1;
}
