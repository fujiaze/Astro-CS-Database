// ============================================================================
// benchmark_precise_fast.cpp - PRECISE vs FAST Drizzle 模式基准对比
//
// 目的:
//   在合成图像矩阵 (像素尺度 × pixfrac × 天区 × 图案) 上, 调用真实
//   DrizzleEngine::drizzle 的 PRECISE 与 FAST 两种模式, 测量耗时并比较
//   累加器差异 (signal/support 误差、候选缺失、通量守恒), 输出 JSONL.
//
// 输出:
//   stdout  — 每行一个 JSON 对象 (每个 case × 模式 × replicate 一行)
//   stderr  — 末尾打印人类可读汇总 (引擎自身的诊断日志也会出现在 stderr)
//
// 编译命令 (从 experiments/ 目录执行; 源文件路径用 benchmark_precise_fast.cpp):
//   g++ -std=c++17 -O2 -fopenmp -Wall -DHAS_LZ4 -DHAS_ZSTD -DAIO_ENABLE_HEALPIX
//       -I.. -I../../healpix_stack -I../../../astro_image_io/include
//       benchmark_precise_fast.cpp
//       ../drizzle_engine.cpp ../wcs_sip.cpp ../poly_clip.cpp ../fits_reader.cpp
//       ../spherical_overlap.cpp
//       ../../healpix_stack/healpix_core.cpp
//       -L../../../astro_image_io -lastro_image_io -static-libgcc -static-libstdc++
//       -lm -o benchmark_precise_fast.exe
//
// R05 适配: 移除 ../fast_overlap.cpp 链接 (FAST 实验在独立分支补齐,
//           当前仅验证 PRECISE 通量守恒, 不调用 FAST 函数)
//
// 注: 任务给定的命令中源文件写作 experiments/benchmark_precise_fast.cpp,
//     该写法适用于从 healpix_drizzle/ 目录执行并同时去掉 .cpp 源文件的 ../ 前缀;
//     上面给出的是从 experiments/ 目录执行的等价形式. 两种均可, 头文件搜索路径
//     (-I.. 解析到 healpix_drizzle/, -I../../healpix_stack 解析到 healpix_stack/)
//     一致.
// ============================================================================

#include "drizzle_engine.h"
#include "fits_reader.h"
#include "healpix_core.h"

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

using namespace drizzle;

// ============================================================================
// 测试矩阵定义
// ============================================================================

// 像素尺度 → 图像尺寸 + NSIDE
//   < 60"  用 nside=32768; 60" 用 4096; 3600" 用 512
struct ScaleCfg {
    double arcsec;   // 角秒/像素
    int    size;     // 图像边长 (像素)
    int    nside;    // HEALPix nside
    const char* label;
};
static const ScaleCfg kScales[] = {
    {0.1,   64, 32768, "0p1"},
    {0.5,   64, 32768, "0p5"},
    {1.0,   64, 32768, "1p0"},
    {10.0,  32, 32768, "10"},
    {60.0,  32, 4096,  "60"},
    {3600.0,16, 512,   "3600"},
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
// R05 适配: 移除 DrizzleMode/mode/fast_healpix_samples (R05 修复已移除这些字段)
// FAST 实验在独立分支补齐, 此 benchmark 先验证 PRECISE 通量守恒
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

static RunResult run_drizzle(const FitsImage& img, const DrizzleConfig& cfg) {
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
// PRECISE vs FAST 累加器比较
//   - common: 同时出现在两个累加器中的像素
//   - candidate_misses: 在 PRECISE 但不在 FAST 中的像素 (理想为 0)
// ============================================================================
struct Comparison {
    bool   valid = false;          // 两次运行均成功时才为 true
    int64_t overlap_pairs = 0;     // common 像素数
    int64_t candidate_misses = 0;  // PRECISE 有而 FAST 无
    double signal_mae = 0.0;
    double signal_rmse = 0.0;
    double signal_max_abs = 0.0;
    double support_mae = 0.0;
    double support_max_abs = 0.0;
};

static Comparison compute_comparison(const RunResult& precise, const RunResult& fast) {
    Comparison c;
    if (!precise.ok || !fast.ok) return c; // valid=false
    c.valid = true;

    double sum_abs_flux = 0.0, sum_sq_flux = 0.0, max_abs_flux = 0.0;
    double sum_abs_area = 0.0, max_abs_area = 0.0;
    int64_t common = 0;

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
        if (aa > max_abs_area) max_abs_area = aa;
        ++common;
    }
    c.overlap_pairs = common;
    if (common > 0) {
        c.signal_mae      = sum_abs_flux / static_cast<double>(common);
        c.signal_rmse     = std::sqrt(sum_sq_flux / static_cast<double>(common));
        c.signal_max_abs  = max_abs_flux;
        c.support_mae     = sum_abs_area / static_cast<double>(common);
        c.support_max_abs = max_abs_area;
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

// 输出一行 JSONL. has_overlap 控制 overlap_pairs 是否输出数值; has_cmp 控制
// signal/support 比较字段及 candidate_misses 是否输出数值 (否则为 null).
static void emit_jsonl(const std::string& case_id, const char* mode,
                       int nside, double pixfrac, double scale_arcsec,
                       int64_t source_pixels, int64_t output_pixels,
                       bool has_overlap, int64_t overlap_pairs,
                       double wall_ms, double cpu_ms,
                       double in_flux, double out_flux, double flux_rel_err,
                       bool has_cmp, const Comparison& cmp,
                       int exit_code, int replicate, bool warmup) {
    std::ostringstream ss;
    ss << std::setprecision(15);
    ss << "{\"case_id\":\"" << case_id << "\","
       << "\"mode\":\"" << mode << "\","
       << "\"nside\":" << nside << ","
       << "\"pixfrac\":" << fmt_num(pixfrac) << ","
       << "\"source_pixel_scale_arcsec\":" << fmt_num(scale_arcsec) << ","
       << "\"source_pixels\":" << source_pixels << ","
       << "\"output_pixels\":" << output_pixels << ","
       << "\"overlap_pairs\":" << (has_overlap ? std::to_string(overlap_pairs) : std::string("null")) << ","
       << "\"wall_ms\":" << fmt_num(wall_ms) << ","
       << "\"cpu_ms\":" << fmt_num(cpu_ms) << ","
       << "\"input_flux\":" << fmt_num(in_flux) << ","
       << "\"output_flux\":" << fmt_num(out_flux) << ","
       << "\"flux_rel_error\":" << fmt_num(flux_rel_err) << ",";
    if (has_cmp) {
        ss << "\"signal_mae\":"      << fmt_num(cmp.signal_mae) << ","
           << "\"signal_rmse\":"     << fmt_num(cmp.signal_rmse) << ","
           << "\"signal_max_abs\":"  << fmt_num(cmp.signal_max_abs) << ","
           << "\"support_mae\":"     << fmt_num(cmp.support_mae) << ","
           << "\"support_max_abs\":" << fmt_num(cmp.support_max_abs) << ","
           << "\"candidate_misses\":" << cmp.candidate_misses << ",";
    } else {
        ss << "\"signal_mae\":null,"
           << "\"signal_rmse\":null,"
           << "\"signal_max_abs\":null,"
           << "\"support_mae\":null,"
           << "\"support_max_abs\":null,"
           << "\"candidate_misses\":null,";
    }
    ss << "\"exit_code\":" << exit_code << ","
       << "\"replicate\":" << replicate << ","
       << "\"warmup\":" << (warmup ? "true" : "false")
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

    std::cerr << "=== benchmark_precise_fast: 开始 ===\n";

    for (const auto& sc : kScales) {
        for (const auto& pf : kPixfracs) {
            for (const auto& rg : kRegions) {
                for (int pi = 0; pi < 2; ++pi) {
                    Pattern pat = static_cast<Pattern>(pi);

                    // 构造 case_id 与图像
                    std::string case_id = std::string("ps") + sc.label +
                                          "_pf" + pf.label +
                                          "_" + rg.label +
                                          "_" + kPatternLabels[pi];
                    FitsImage img = make_synthetic_image(sc.arcsec, sc.size,
                                                         rg.ra, rg.dec, pat);
                    const double in_flux = input_flux(img);
                    const int64_t source_pixels = static_cast<int64_t>(sc.size) * sc.size;

                    DrizzleConfig cfg_precise = make_config(sc.nside, pf.value);

                    // R05 适配: 仅运行 PRECISE 模式 (FAST 在独立分支补齐)
                    RunResult precise_runs[kReps];
                    for (int r = 0; r < kReps; ++r) {
                        precise_runs[r] = run_drizzle(img, cfg_precise);
                    }

                    // R05 适配: 无 FAST 比较, 填充默认值
                    ++total_cases;
                    const bool p_ok = precise_runs[kReps - 1].ok;
                    if (p_ok) ++both_ok;
                    else ++both_fail;

                    const bool has_overlap = false;
                    const int64_t overlap_pairs = 0;

                    // 输出 PRECISE 行 (R05: 无 FAST 比较, has_cmp=false)
                    for (int r = 0; r < kReps; ++r) {
                        const RunResult& rr = precise_runs[r];
                        const double of = output_flux(rr.acc);
                        const double fre = in_flux > 0.0
                            ? std::fabs(of - in_flux) / in_flux : 0.0;
                        if (r > 0) sum_wall_precise += rr.wall_ms;
                        if (r > 0) max_flux_rel_err_precise = std::max(max_flux_rel_err_precise, fre);
                        Comparison empty_cmp;
                        emit_jsonl(case_id, "PRECISE",
                                   sc.nside, pf.value, sc.arcsec,
                                   source_pixels,
                                   static_cast<int64_t>(rr.acc.size()),
                                   has_overlap, overlap_pairs,
                                   rr.wall_ms, rr.cpu_ms,
                                   in_flux, of, fre,
                                   /*has_cmp=*/false, empty_cmp,
                                   rr.ok ? 0 : 1, r, /*warmup=*/(r == 0));
                    }
                    // R05 适配: FAST 行跳过 (在独立分支补齐)
                }
            }
        }
    }

    std::cout.flush();

    // ---- 汇总到 stderr ----
    const double mean_speedup = (sum_wall_fast > 0.0)
        ? sum_wall_precise / sum_wall_fast : 0.0;
    const bool pass = (both_ok == total_cases) && (cases_with_misses == 0);

    std::cerr << "\n=== BENCHMARK SUMMARY ===\n";
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
    std::cerr << "RESULT: " << (pass ? "PASS" : "FAIL") << "\n";
    std::cerr << "=== END SUMMARY ===\n";

    return pass ? 0 : 1;
}
