// ============================================================================
// drizzle_freeze_test.cpp - Drizzle Phase1 最终冻结验收 (合成真值, 代表点)
//
// 按 Phase1_Drizzle_Acceptance_Spec 分层框架, 全部合成数据、小图/代表点:
// T1 coverage/hole Oracle: false hole = 0, false fill = 0
// T2 主支持域采样率代表点 {0.2,0.5,1,2,5,10}"/px + 0.1" 扩展
// T3 pixfrac 主域 {0.6,0.8,1.0} + 扩展 {0.5,0.25,0.1}
// T4 视场代表点: 小 / 中 / 宽(15° 边缘 patch)
// T5 Sphere -> Plane 双向底层最小闭合 (坐标往返 + leaf 覆盖一致)
// T6 HISS Writer/Reader 往返
// T7 科学保真 (Layer B): 均匀背景保持均匀 + 点源总通量守恒
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
    // 的覆盖权重有几何涨落 (signal=1000 x Σweight), 但表面亮度应均匀:
    // signal / sumArea = 1000 / drop_area = 常数 (rel_std 小)。
    // 这验证 drizzle 不引入非物理的亮度不均匀 (无接缝/系统性偏差)。
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


// ============================================================================
// T8: 极区/接缝位置集守恒闭合（P3-01 / P3-06 订正；判据 = DRIZZLE_GEOMETRY §9）
// ----------------------------------------------------------------------------
// 背景: 原冻结验收的位置集不含极点与 u+v=1 接缝，而生产在 nside>=256 曾走
//   "叶多边形顶点全含 ⇒ 返回解析叶面积 π/(3N²)" 的旧快路径 —— 返回口径与裁剪
//   路径不同（弦表示亏缺至 ~9.97%/叶），极点栅格实测 376/1089 破门、最坏
//   2.5172e-02，本门却全绿。T8 把该位置集与订正后的混合判据锁进冻结门：
//     逐 drop |Σ_j a_jp − A_drop,p| ≤ max(1e-6·A_drop,p, 1e-15 sr)
//     帧级    |Σ_p (Σ_j a_jp − A_drop,p)| / Σ_p A_drop,p ≤ 1e-4
//   参考面积由本测试自算（long double Van Oosterom 扇形），不复用被测路径。
// 完备位置集（极点邻域 / 接缝 / face 角点 / HST 0.04″ 尺度 / 负例注入）与
//   逐组判定在 ctest 门 drizzle_p3_conservation（tests/p3_conservation_gate.cpp）；
//   本 T8 保留为冻结验收里的最小位置集与同判据抽查。
// ============================================================================
static double t8_ref_area_sr(const std::vector<spherical::Vec3>& poly) {
    const int n = (int)poly.size();
    if (n < 3) return 0.0;
    long double tot = 0.0L;
    for (int i = 1; i < n - 1; ++i) {
        const spherical::Vec3& A = poly[0];
        const spherical::Vec3& B = poly[i];
        const spherical::Vec3& C = poly[i + 1];
        const long double bx = (long double)B.y * C.z - (long double)B.z * C.y;
        const long double by = (long double)B.z * C.x - (long double)B.x * C.z;
        const long double bz = (long double)B.x * C.y - (long double)B.y * C.x;
        const long double det = (long double)A.x * bx + (long double)A.y * by +
                                (long double)A.z * bz;
        const long double den =
            1.0L + ((long double)A.x * B.x + (long double)A.y * B.y + (long double)A.z * B.z) +
                   ((long double)B.x * C.x + (long double)B.y * C.y + (long double)B.z * C.z) +
                   ((long double)C.x * A.x + (long double)C.y * A.y + (long double)C.z * A.z);
        tot += 2.0L * atan2l(det, den);
    }
    return std::fabs((double)tot);
}

// TAN (gnomonic) 足迹: p = normalize(T + ξ·e1 + η·e2)（不经 (ra,dec) 往返）
static std::vector<spherical::Vec3> t8_make_drop(double ra_deg, double dec_deg,
                                                 double arcsec_per_px,
                                                 double px, double py) {
    const double a = ra_deg * PI_ / 180.0, d = dec_deg * PI_ / 180.0;
    const spherical::Vec3 T{std::cos(d) * std::cos(a), std::cos(d) * std::sin(a), std::sin(d)};
    const spherical::Vec3 e1{-std::sin(a), std::cos(a), 0.0};
    const spherical::Vec3 e2{-std::sin(d) * std::cos(a), -std::sin(d) * std::sin(a), std::cos(d)};
    const double s = arcsec_per_px * PI_ / (180.0 * 3600.0);
    const double h = 0.5;
    const double c[4][2] = {{px - h, py - h}, {px + h, py - h}, {px + h, py + h}, {px - h, py + h}};
    std::vector<spherical::Vec3> out;
    out.reserve(4);
    for (int i = 0; i < 4; ++i) {
        const double xi = c[i][0] * s, eta = c[i][1] * s;
        spherical::Vec3 v{T.x + xi * e1.x + eta * e2.x,
                          T.y + xi * e1.y + eta * e2.y,
                          T.z + xi * e1.z + eta * e2.z};
        const double l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        out.push_back({v.x / l, v.y / l, v.z / l});
    }
    return out;
}

static void test_pole_seam_closure() {
    printf("=== T8: 极区/接缝位置集守恒闭合（冻结判据） ===\n");
    const double rel_budget = 1e-6, abs_budget = 1e-15, frame_budget = 1e-4;
    struct P { const char* tag; double ra, dec; };
    const P pos[2] = {{"极点(0,90)", 0.0, 90.0},
                      {"u+v=1 接缝(45,41.810315)", 45.0, 41.810315}};
    const int nside = 2097152;                       // 2^21, hp_res ≈ 0.1006"
    healpix::HealpixCore hp(nside);
    const double hp_res_as = hp.pixelResolutionArcsec();
    const double hp_res = hp_res_as * PI_ / (180.0 * 3600.0);
    for (int pi = 0; pi < 2; ++pi) {
        const P& p = pos[pi];
        spherical::TargetGeomCache cache(8192);
        double worst_abs = 0.0, worst_rel = 0.0, worst_ratio = 0.0, sum_da = 0.0, sum_a = 0.0;
        int n = 0;
        for (double k : {0.3, 1.0, 3.0}) {           // drop 尺度 0.3 / 1 / 3 × hp_res
            for (int ix = -2; ix <= 2; ++ix)
                for (int iy = -2; iy <= 2; ++iy) {
                    const std::vector<spherical::Vec3> drop =
                        t8_make_drop(p.ra, p.dec, k * hp_res_as, (double)ix, (double)iy);
                    const double a_ref = t8_ref_area_sr(drop);
                    std::vector<spherical::Vec3> d = drop, dd = drop;
                    spherical::DropGeometryT<double> pg;
                    spherical::build_drop_geometry_into<double>(pg, d, &dd);
                    std::vector<uint64_t> cands;
                    spherical::query_candidate_pixels<double>(dd, hp, cands);
                    double sum = 0.0;
                    for (uint64_t ipix : cands)
                        sum += spherical::compute_overlap_area_g_ctx_cached<double>(
                            pg, hp, ipix, hp_res, cache);
                    const double da = sum - a_ref;
                    const double lim = std::max(rel_budget * a_ref, abs_budget);
                    worst_abs = std::max(worst_abs, std::fabs(da));
                    worst_rel = std::max(worst_rel, std::fabs(da) / a_ref);
                    worst_ratio = std::max(worst_ratio, std::fabs(da) / lim);
                    sum_da += da; sum_a += a_ref; ++n;
                }
        }
        const double frame_rel = std::fabs(sum_da) / sum_a;
        char msg[256];
        std::snprintf(msg, sizeof(msg),
                      "T8 %s: n=%d 最坏|dA|=%.3e sr 最坏相对=%.3e |dA|/判据限=%.4f (须<=1) "
                      "帧级=%.3e (须<=%.1e, 判据 max(%.0e·A, %.0e sr))",
                      p.tag, n, worst_abs, worst_rel, worst_ratio,
                      frame_rel, frame_budget, rel_budget, abs_budget);
        CHECK(worst_ratio <= 1.0 && frame_rel <= frame_budget, msg);
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
    test_pole_seam_closure();
    printf("== 冻结验收结果: %d 通过, %d 失败 ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
