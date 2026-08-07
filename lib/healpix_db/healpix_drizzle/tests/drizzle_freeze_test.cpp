// ============================================================================
// drizzle_freeze_test.cpp - Drizzle Phase1 最终冻结验收 (合成真值, 代表点)
//
// 按 Phase1_Drizzle_Acceptance_Spec 分层框架, 全部合成数据、小图/代表点:
//   T1 coverage/hole Oracle: false hole = 0, false fill = 0
//   T2 主支持域采样率代表点 {0.2,0.5,1,2,5,10}"/px + 0.1" 扩展
//   T3 pixfrac 主域 {0.6,0.8,1.0} + 扩展 {0.5,0.25,0.1}
//   T4 视场代表点: 小 / 中 / 宽(15° 边缘 patch)
//   T5 Sphere -> Plane 双向底层最小闭合 (坐标往返 + leaf 覆盖一致)
//   T6 HISS Writer/Reader 往返
//   T7 科学保真 (Layer B): 均匀背景保持均匀 + 点源总通量守恒
// 硬门: FP64 通量闭合 < 1e-6 (主域), FP32/FP64 逐 leaf < 1e-5, missing=0
// ============================================================================
#include "drizzle_engine.h"
#include "hiss_format.h"
#include "spherical_overlap.h"
#include "aio_healpix_io.h"
#include "wcs_sip.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>

using namespace drizzle;

static const double PI_ = 3.14159265358979323846;
static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); ++g_pass; } \
    else { printf("  [FAIL] %s\n", msg); ++g_fail; } \
} while (0)

static double make_synth(FitsImage& img, const WcsParams& w, int size) {
    img.width = size; img.height = size; img.channels = 1;
    img.wcs = w;
    img.pixels.resize((size_t)size * size);
    img.pixels_f64.resize((size_t)size * size);
    const double cx = size * 0.5, cy = size * 0.5;
    const double sigma = size * 0.12;
    const double amp = 500.0;
    double total = 0.0;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            double base = 1000.0 + 0.01 * x + 0.005 * y;
            double dx = x - cx, dy = y - cy;
            double g = amp * std::exp(-(dx * dx + dy * dy) / (2.0 * sigma * sigma));
            double v = base + g;
            img.pixels[(size_t)y * size + x] = (float)v;
            img.pixels_f64[(size_t)y * size + x] = v;
            total += v;
        }
    return total;
}

static WcsParams make_wcs(double ra0, double dec0, double scale_arcsec,
                          int size) {
    WcsParams w;
    w.has_wcs = true;
    std::strncpy(w.ctype1, "RA---TAN-SIP", 15);
    std::strncpy(w.ctype2, "DEC--TAN-SIP", 15);
    w.crval[0] = ra0; w.crval[1] = dec0;
    w.crpix[0] = size / 2.0 + 0.5; w.crpix[1] = size / 2.0 + 0.5;
    double s = scale_arcsec / 3600.0;
    w.cd[0] = -s; w.cd[1] = 0.0; w.cd[2] = 0.0; w.cd[3] = s;
    return w;
}

// 从 tiles 重建 leaf ipix 集合 (parent << shift | local)
static void tile_leaf_set(const std::vector<TileAccumulatorT<double>>& tiles,
                          int shift, std::set<uint64_t>& out) {
    for (const auto& t : tiles)
        for (uint32_t local : t.touched)
            out.insert((t.parent_ipix << shift) | local);
}

// 单组合: FP64/FP32 drizzle + 闭合 + 逐 leaf 一致
struct CaseResult {
    double rel64 = 1.0, maxrel32 = 1.0;
    int missing = 1;
    double s_fp32 = 0, s_fp64 = 0;
    size_t nleaf64 = 0;
};

static void run_case(const FitsImage& img, int nside, double pixfrac,
                     CaseResult& r) {
    DrizzleConfig cfg;
    cfg.nside = nside; cfg.nested = true; cfg.pixfrac = pixfrac;
    cfg.precision_mode = 1; cfg.threads = 16;
    DrizzleEngine engine;
    std::vector<TileAccumulatorT<double>> t64;
    std::vector<TileAccumulatorT<float>> t32;
    DrizzleStats st64, st32; std::string err;
    if (!engine.drizzleTiled_f64(img, cfg, nullptr, nullptr, t64, st64, err) ||
        !engine.drizzleTiled(img, cfg, nullptr, nullptr, t32, st32, err)) {
        printf("  [FAIL] drizzle 失败: %s\n", err.c_str());
        return;
    }
    double sum_in = 0, sum64 = 0;
    for (size_t i = 0; i < img.pixels_f64.size(); i++) sum_in += img.pixels_f64[i];
    uint32_t depth = hiss::compute_tile_depth((uint32_t)nside);
    int shift = 2 * (int)depth;
    std::map<uint64_t, double> ref;
    for (const auto& t : t64)
        for (uint32_t local : t.touched) {
            sum64 += t.pixels[local].sumFlux;
            ref[(t.parent_ipix << shift) | local] = t.pixels[local].sumFlux;
        }
    r.rel64 = std::fabs(sum64 - sum_in) / sum_in;
    r.s_fp64 = st64.elapsedSec;
    r.nleaf64 = ref.size();
    double maxrel = 0; int missing = 0;
    for (const auto& t : t32)
        for (uint32_t local : t.touched) {
            uint64_t ip = (t.parent_ipix << shift) | local;
            auto it = ref.find(ip);
            if (it == ref.end()) { missing++; continue; }
            double d = std::fabs((double)t.pixels[local].sumFlux - it->second) /
                       std::max(std::fabs(it->second), 1.0);
            if (d > maxrel) maxrel = d;
        }
    r.maxrel32 = maxrel;
    r.missing = missing;
    r.s_fp32 = st32.elapsedSec;
}

// ============================================================================
// T1: coverage / hole Oracle (false hole = 0, false fill = 0)
// ============================================================================
static void test_coverage_oracle() {
    printf("=== T1: coverage/hole Oracle (false hole=0, false fill=0) ===\n");
    const int size = 96, nside = 65536;
    for (double pf : {0.8, 1.0}) {
        WcsParams w = make_wcs(272.886595, -23.254083, 6.3, size);
        FitsImage img;
        make_synth(img, w, size);
        DrizzleConfig cfg;
        cfg.nside = nside; cfg.nested = true; cfg.pixfrac = pf;
        cfg.precision_mode = 1; cfg.threads = 16;
        DrizzleEngine engine;
        std::vector<TileAccumulatorT<double>> t64;
        DrizzleStats st; std::string err;
        engine.drizzleTiled_f64(img, cfg, nullptr, nullptr, t64, st, err);
        uint32_t depth = hiss::compute_tile_depth((uint32_t)nside);
        int shift = 2 * (int)depth;
        std::set<uint64_t> out;
        tile_leaf_set(t64, shift, out);
        // Oracle: 逐源像素 drop 候选 + overlap>0 真覆盖集
        WcsSip wcs(w);
        healpix::HealpixCore hp(nside, true);
        std::set<uint64_t> oracle;
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++) {
                double cr[4], cd[4];
                for (int i = 0; i < 4; i++) {
                    double ox = (i == 0 || i == 3) ? -0.5 : 0.5;
                    double oy = (i < 2) ? -0.5 : 0.5;
                    wcs.pixelToSky(x + ox * pf, y + oy * pf, cr[i], cd[i]);
                }
                std::vector<spherical::Vec3> corners;
                for (int i = 0; i < 4; i++)
                    corners.push_back(spherical::radec_to_vec<double>(cr[i], cd[i]));
                auto g = spherical::build_drop_geometry<double>(corners);
                std::vector<uint64_t> cands;
                spherical::query_candidate_pixels_fast<double>(corners, hp, cands);
                for (uint64_t ip : cands) {
                    if (spherical::compute_overlap_area_g<double>(g, hp, ip) > 0.0)
                        oracle.insert(ip);
                }
            }
        std::vector<uint64_t> fh, ff;
        for (uint64_t ip : oracle) if (!out.count(ip)) fh.push_back(ip);
        for (uint64_t ip : out)    if (!oracle.count(ip)) ff.push_back(ip);
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "[pf=%.1f] false hole=%zu false fill=%zu (oracle=%zu out=%zu)",
                 pf, fh.size(), ff.size(), oracle.size(), out.size());
        CHECK(fh.empty() && ff.empty(), msg);
    }
}

// ============================================================================
// T2: 主支持域采样率代表点
// ============================================================================
static void test_sampling_rates() {
    printf("=== T2: 采样率代表点 (0.1~10\"/px) ===\n");
    struct SR { double scale; int nside; bool extended; };
    const SR sr[] = {
        {0.1, 2097152, true},   // 扩展 (核心端点外扩)
        {0.2, 1048576, false},
        {0.5, 524288, false},
        {1.0, 262144, false},
        {2.0, 131072, false},
        {5.0, 65536, false},
        {10.0, 32768, false},
    };
    const int size = 48;
    for (const auto& s : sr) {
        WcsParams w = make_wcs(272.886595, -23.254083, s.scale, size);
        FitsImage img;
        make_synth(img, w, size);
        CaseResult r;
        run_case(img, s.nside, 0.8, r);
        char tag[64];
        snprintf(tag, sizeof(tag), "[%.1f\"/px N=%d]", s.scale, s.nside);
        char msg[160];
        double gate = s.extended ? 1e-5 : 1e-6;
        snprintf(msg, sizeof(msg), "%s FP64 闭合 %.3e (<%.0e)", tag, r.rel64, gate);
        CHECK(r.rel64 < gate, msg);
        snprintf(msg, sizeof(msg),
                 "%s FP32/FP64 %.3e (<1e-5, missing=%d)", tag, r.maxrel32, r.missing);
        CHECK(r.maxrel32 < 1e-5 && r.missing == 0, msg);
    }
}

// ============================================================================
// T3: pixfrac 主域 {0.6,0.8,1.0} + 扩展 {0.5,0.25,0.1}
// ============================================================================
static void test_pixfrac() {
    printf("=== T3: pixfrac 主域 {0.6,0.8,1.0} + 扩展 ===\n");
    const int size = 128, nside = 65536;
    const double pfs[] = {0.6, 0.8, 1.0, 0.5, 0.25, 0.1};
    for (double pf : pfs) {
        WcsParams w = make_wcs(272.886595, -23.254083, 6.3, size);
        FitsImage img;
        make_synth(img, w, size);
        CaseResult r;
        run_case(img, nside, pf, r);
        char msg[160];
        snprintf(msg, sizeof(msg), "[pf=%.2f] FP64 闭合 %.3e (<1e-6)", pf, r.rel64);
        CHECK(r.rel64 < 1e-6, msg);
        snprintf(msg, sizeof(msg),
                 "[pf=%.2f] FP32/FP64 %.3e (<1e-5, missing=%d)",
                 pf, r.maxrel32, r.missing);
        CHECK(r.maxrel32 < 1e-5 && r.missing == 0, msg);
    }
}

// ============================================================================
// T4: 视场代表点 (小 / 中 / 宽 15° 边缘 patch)
// ============================================================================
static void test_fov() {
    printf("=== T4: 视场代表点 ===\n");
    // 小视场: 64^2 @ 0.5"/px (~32")
    {
        const int size = 64;
        WcsParams w = make_wcs(272.886595, -23.254083, 0.5, size);
        FitsImage img; make_synth(img, w, size);
        CaseResult r; run_case(img, 524288, 0.8, r);
        char msg[160];
        snprintf(msg, sizeof(msg), "[small fov] FP64 闭合 %.3e (<1e-6)", r.rel64);
        CHECK(r.rel64 < 1e-6, msg);
        snprintf(msg, sizeof(msg), "[small fov] FP32/FP64 %.3e (<1e-5)",
                 r.maxrel32);
        CHECK(r.maxrel32 < 1e-5 && r.missing == 0, msg);
    }
    // 中视场: 128^2 @ 6.3"/px (1.79°)
    {
        const int size = 128;
        WcsParams w = make_wcs(272.886595, -23.254083, 6.3, size);
        FitsImage img; make_synth(img, w, size);
        CaseResult r; run_case(img, 65536, 0.8, r);
        char msg[160];
        snprintf(msg, sizeof(msg), "[mid fov] FP64 闭合 %.3e (<1e-6)", r.rel64);
        CHECK(r.rel64 < 1e-6, msg);
        snprintf(msg, sizeof(msg), "[mid fov] FP32/FP64 %.3e (<1e-5)",
                 r.maxrel32);
        CHECK(r.maxrel32 < 1e-5 && r.missing == 0, msg);
    }
    // 宽视场边缘 patch: 模拟 15° 视场边缘 (CRVAL 偏移 7°, 10"/px, TAN 边缘)
    {
        const int size = 128;
        WcsParams w = make_wcs(272.886595 + 7.0, -23.254083, 10.0, size);
        FitsImage img; make_synth(img, w, size);
        CaseResult r; run_case(img, 32768, 0.8, r);
        char msg[160];
        snprintf(msg, sizeof(msg), "[wide edge] FP64 闭合 %.3e (<1e-6)", r.rel64);
        CHECK(r.rel64 < 1e-6, msg);
        snprintf(msg, sizeof(msg), "[wide edge] FP32/FP64 %.3e (<1e-5)",
                 r.maxrel32);
        CHECK(r.maxrel32 < 1e-5 && r.missing == 0, msg);
    }
}

// ============================================================================
// T5: Sphere -> Plane 双向底层最小闭合
// ============================================================================
static void test_reverse() {
    printf("=== T5: Sphere -> Plane 双向底层最小闭合 ===\n");
    const int size = 64;
    WcsParams w = make_wcs(272.886595, -23.254083, 6.3, size);
    WcsSip wcs(w);
    // 坐标往返: 平面网格 -> 球面 -> 平面
    double max_px = 0.0, max_sky_arcsec = 0.0;
    int n = 0;
    for (int y = 0; y < size; y += 4)
        for (int x = 0; x < size; x += 4) {
            double ra, dec, x2, y2, ra2, dec2;
            wcs.pixelToSky((double)x, (double)y, ra, dec);
            wcs.skyToPixel(ra, dec, x2, y2);
            max_px = std::max(max_px, std::hypot(x2 - x, y2 - y));
            wcs.pixelToSky(x2, y2, ra2, dec2);
            max_sky_arcsec = std::max(max_sky_arcsec,
                std::hypot((ra2 - ra) * std::cos(dec * PI_ / 180.0),
                           dec2 - dec) * 3600.0);
            n++;
        }
    char msg[160];
    snprintf(msg, sizeof(msg),
             "TAN 坐标往返: max px err %.3e, max sky %.3e\" (n=%d)",
             max_px, max_sky_arcsec, n);
    CHECK(max_px < 1e-6 && max_sky_arcsec < 1e-4, msg);

    // leaf 覆盖一致: drizzle 输出 leaf 中心 -> skyToPixel 回平面,
    // 应落在源图像有效覆盖范围内 (局部几何一致)
    FitsImage img; make_synth(img, w, size);
    CaseResult r;
    run_case(img, 65536, 0.8, r);
    DrizzleConfig cfg;
    cfg.nside = 65536; cfg.nested = true; cfg.pixfrac = 0.8;
    cfg.precision_mode = 1; cfg.threads = 16;
    DrizzleEngine engine;
    std::vector<TileAccumulatorT<double>> t64;
    DrizzleStats st; std::string err;
    engine.drizzleTiled_f64(img, cfg, nullptr, nullptr, t64, st, err);
    healpix::HealpixCore hp(65536, true);
    int out_of_range = 0, total = 0;
    for (const auto& t : t64)
        for (uint32_t local : t.touched) {
            uint64_t ip = (t.parent_ipix << (2 * 9)) | local;
            double ra, dec;
            hp.pix2radec((int64_t)ip, &ra, &dec);
            double px, py;
            wcs.skyToPixel(ra, dec, px, py);
            total++;
            // 输出 leaf 中心回投影应接近源图像范围 (含 pixfrac 收缩余量)
            if (px < -2.0 || px > size + 1.0 || py < -2.0 || py > size + 1.0)
                out_of_range++;
        }
    snprintf(msg, sizeof(msg),
             "输出 leaf 中心回投影: 越界 %d/%d (pixfrac=0.8)", out_of_range, total);
    CHECK(out_of_range == 0, msg);
}

// ============================================================================
// T6: HISS Writer/Reader 往返
// ============================================================================
static void test_hiss_roundtrip() {
    printf("=== T6: HISS Writer/Reader 往返 ===\n");
    const int size = 128, nside = 65536;
    WcsParams w = make_wcs(272.886595, -23.254083, 6.3, size);
    FitsImage img; make_synth(img, w, size);
    DrizzleConfig cfg;
    cfg.nside = nside; cfg.nested = true; cfg.pixfrac = 0.8;
    cfg.precision_mode = 0; cfg.threads = 16;
    cfg.photometry_applied_upstream = true;  // 合成数据模拟已测光校准
    DrizzleEngine engine;
    std::vector<TileAccumulatorT<float>> t32;
    DrizzleStats st; std::string err;
    if (!engine.drizzleTiled(img, cfg, nullptr, nullptr, t32, st, err)) {
        CHECK(false, ("drizzleTiled 失败: " + err).c_str());
        return;
    }
    DrizzleMeta meta;
    meta.filter = "R";
    meta.exposure_s = 180.0;
    const char* hiss_path = "run/temp/freeze_test.hiss";
    if (!engine.writeHisTilesT(t32, st, img.wcs, cfg, meta,
                               "", hiss_path, nullptr, nullptr, err)) {
        CHECK(false, ("writeHisTilesT 失败: " + err).c_str());
        return;
    }
    // 读回
    uint32_t rnside = 0; int rnested = 0; uint64_t rnpix = 0;
    uint64_t* ripix = nullptr; float* rpix = nullptr; float* rsnr = nullptr;
    char* rmeta = nullptr;
    int rc = aio_hiss_read(hiss_path, &rnside, &rnested, &rnpix,
                           &ripix, &rpix, &rsnr, &rmeta);
    char msg[160];
    snprintf(msg, sizeof(msg), "aio_hiss_read rc=%d", rc);
    CHECK(rc == 0, msg);
    if (rc != 0) return;
    snprintf(msg, sizeof(msg), "nside=%u (期望 %d), npix=%llu (期望 %lld)",
             rnside, nside, (unsigned long long)rnpix,
             (long long)st.nHealpixPixels);
    CHECK(rnside == (uint32_t)nside &&
          rnpix == (uint64_t)st.nHealpixPixels, msg);
    // signal 与 tile sumFlux 一致
    std::map<uint64_t, double> expect;
    for (const auto& t : t32)
        for (uint32_t local : t.touched) {
            uint64_t ip = (t.parent_ipix << (2 * 9)) | local;
            expect[ip] = (double)t.pixels[local].sumFlux;
        }
    int mism = 0;
    for (uint64_t i = 0; i < rnpix; i++) {
        auto it = expect.find(ripix[i]);
        if (it == expect.end() ||
            std::fabs((double)rpix[i] - it->second) >
                1e-4 * std::max(std::fabs(it->second), 1.0))
            mism++;
    }
    snprintf(msg, sizeof(msg), "HISS signal vs tile sumFlux: mismatch=%d/%llu",
             mism, (unsigned long long)rnpix);
    CHECK(mism == 0, msg);
    std::free(ripix); std::free(rpix); std::free(rsnr); std::free(rmeta);
}

// ============================================================================
// T7: 科学保真 (Layer B)
// ============================================================================
static void test_science_fidelity() {
    printf("=== T7: 科学保真 (均匀背景 + 点源总通量) ===\n");
    const int size = 128, nside = 65536;
    // 1) 均匀背景: 常数 1000。源像素网格与 HEALPix 网格未对齐导致每个 leaf
    //    的覆盖权重有几何涨落 (signal=1000 x Σweight), 但表面亮度应均匀:
    //    signal / sumArea = 1000 / drop_area = 常数 (rel_std 小)。
    //    这验证 drizzle 不引入非物理的亮度不均匀 (无接缝/系统性偏差)。
    {
        WcsParams w = make_wcs(272.886595, -23.254083, 6.3, size);
        FitsImage img;
        img.width = size; img.height = size; img.channels = 1;
        img.wcs = w;
        img.pixels.resize((size_t)size * size, 1000.0f);
        img.pixels_f64.resize((size_t)size * size, 1000.0);
        DrizzleConfig cfg;
        cfg.nside = nside; cfg.nested = true; cfg.pixfrac = 0.8;
        cfg.precision_mode = 1; cfg.threads = 16;
        DrizzleEngine engine;
        std::vector<TileAccumulatorT<double>> t64;
        DrizzleStats st; std::string err;
        engine.drizzleTiled_f64(img, cfg, nullptr, nullptr, t64, st, err);
        double sum = 0, sum2 = 0; size_t n = 0;
        for (const auto& t : t64)
            for (uint32_t local : t.touched) {
                double v = (double)t.pixels[local].sumFlux;
                double a = (double)t.pixels[local].sumArea;
                if (a <= 0) continue;
                double surf = v / a;   // 表面亮度 = signal / 覆盖面积
                sum += surf; sum2 += surf * surf; n++;
            }
        double mean = sum / n;
        double stddev = std::sqrt(std::max(0.0, sum2 / n - mean * mean));
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "[uniform bg] signal/sumArea n=%zu mean=%.6g rel_std=%.3e (<1e-3)",
                 n, mean, stddev / mean);
        CHECK(n > 0 && stddev / mean < 1e-3, msg);
    }
    // 2) 点源总通量: 背景 0 + 单高斯星 (离散总通量 F), 输出全部 leaf 积分 ≈ F
    {
        WcsParams w = make_wcs(272.886595, -23.254083, 6.3, size);
        FitsImage img;
        img.width = size; img.height = size; img.channels = 1;
        img.wcs = w;
        img.pixels.resize((size_t)size * size, 0.0f);
        img.pixels_f64.resize((size_t)size * size, 0.0);
        const double cx = size * 0.5, cy = size * 0.5, sigma = 2.5, amp = 1000.0;
        double F = 0.0;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x) {
                double dx = x - cx, dy = y - cy;
                double v = amp * std::exp(-(dx * dx + dy * dy) / (2.0 * sigma * sigma));
                img.pixels[(size_t)y * size + x] = (float)v;
                img.pixels_f64[(size_t)y * size + x] = v;
                F += v;
            }
        DrizzleConfig cfg;
        cfg.nside = nside; cfg.nested = true; cfg.pixfrac = 0.8;
        cfg.precision_mode = 1; cfg.threads = 16;
        DrizzleEngine engine;
        std::vector<TileAccumulatorT<double>> t64;
        DrizzleStats st; std::string err;
        engine.drizzleTiled_f64(img, cfg, nullptr, nullptr, t64, st, err);
        double out = 0;
        for (const auto& t : t64)
            for (uint32_t local : t.touched)
                out += (double)t.pixels[local].sumFlux;
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "[point source] F=%.6g out=%.6g rel=%.3e (<1e-6)",
                 F, out, std::fabs(out - F) / F);
        CHECK(std::fabs(out - F) / F < 1e-6, msg);
    }
    // 3) 梯度背景 (make_synth 含常数底+梯度+高斯): 无 NaN/负值, 能量守恒
    {
        WcsParams w = make_wcs(272.886595, -23.254083, 6.3, size);
        FitsImage img;
        double sum_in = make_synth(img, w, size);
        DrizzleConfig cfg;
        cfg.nside = nside; cfg.nested = true; cfg.pixfrac = 0.8;
        cfg.precision_mode = 1; cfg.threads = 16;
        DrizzleEngine engine;
        std::vector<TileAccumulatorT<double>> t64;
        DrizzleStats st; std::string err;
        engine.drizzleTiled_f64(img, cfg, nullptr, nullptr, t64, st, err);
        double sum_out = 0; bool bad = false;
        for (const auto& t : t64)
            for (uint32_t local : t.touched) {
                double v = (double)t.pixels[local].sumFlux;
                if (!std::isfinite(v) || v < 0) bad = true;
                sum_out += v;
            }
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "[gradient bg] 无 NaN/负值, 闭合 rel=%.3e (<1e-6)",
                 std::fabs(sum_out - sum_in) / sum_in);
        CHECK(!bad && std::fabs(sum_out - sum_in) / sum_in < 1e-6, msg);
    }
}

int main() {
    printf("=== Drizzle Phase1 最终冻结验收 (合成真值) ===\n");
    test_coverage_oracle();
    test_sampling_rates();
    test_pixfrac();
    test_fov();
    test_reverse();
    test_hiss_roundtrip();
    test_science_fidelity();
    printf("== 冻结验收结果: %d 通过, %d 失败 ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
