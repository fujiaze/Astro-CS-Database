// ============================================================================
// reverse_drizzle_test.cpp - Sphere -> Plane 真面积 Drizzle 验证 (R13 REV-001)
//
// 合成真值 (不依赖 Plane->Sphere->Plane 视觉相似):
//   1) 均匀平面源: 正向 -> 反向, 输出应恢复均匀 (rel_std 小)
//   2) 点源平面源: 正向 -> 反向, 输出质心 = 源质心, 总通量闭合
//   3) FP32/FP64 输出一致性
//   4) 通量闭合: Σ 反向输出 (图像内) ≈ Σ 输入
// ============================================================================
#include "drizzle_engine.h"
#include "hiss_format.h"
#include "reverse_drizzle.h"
#include "spherical_overlap.h"
#include "wcs_sip.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>

using namespace drizzle;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); ++g_pass; } \
    else { printf("  [FAIL] %s\n", msg); ++g_fail; } \
} while (0)

static WcsParams make_wcs(double ra0, double dec0, double scale, int size) {
    WcsParams w;
    w.has_wcs = true;
    std::strncpy(w.ctype1, "RA---TAN-SIP", 15);
    std::strncpy(w.ctype2, "DEC--TAN-SIP", 15);
    w.crval[0] = ra0; w.crval[1] = dec0;
    w.crpix[0] = size / 2.0 + 0.5; w.crpix[1] = size / 2.0 + 0.5;
    double s = scale / 3600.0;
    w.cd[0] = -s; w.cd[1] = 0.0; w.cd[2] = 0.0; w.cd[3] = s;
    return w;
}

// 正向 Plane->Sphere: 平面图 -> leaf signal
static void forward(const FitsImage& img, int nside, double pixfrac,
                    std::vector<uint64_t>& ipix, std::vector<double>& sig,
                    std::string& err) {
    DrizzleConfig cfg;
    cfg.nside = nside; cfg.nested = true; cfg.pixfrac = pixfrac;
    cfg.precision_mode = 1; cfg.threads = 16;
    DrizzleEngine engine;
    std::vector<TileAccumulatorT<double>> t64;
    DrizzleStats st;
    engine.drizzleTiled_f64(img, cfg, nullptr, nullptr, t64, st, err);
    uint32_t depth = hiss::compute_tile_depth((uint32_t)nside);
    int shift = 2 * (int)depth;
    for (const auto& t : t64)
        for (uint32_t local : t.touched) {
            ipix.push_back((t.parent_ipix << shift) | local);
            sig.push_back((double)t.pixels[local].sumFlux);
        }
}

int main() {
    printf("=== Sphere -> Plane 真面积 Drizzle 验证 ===\n");
    const int size = 64, nside = 65536;
    const double pf = 0.8;

    // ---- 1. 均匀平面源 (MICROFIX #4: pf=1.0 无空隙往返, rel_std ≤1e-4 冻结门) ----
    {
        const double pf1 = 1.0;
        WcsParams w = make_wcs(272.886595, -23.254083, 6.3, size);
        FitsImage img;
        img.width = size; img.height = size; img.channels = 1;
        img.wcs = w;
        img.pixels.resize((size_t)size * size, 1000.0f);
        img.pixels_f64.resize((size_t)size * size, 1000.0);
        std::vector<uint64_t> ipix; std::vector<double> sig; std::string err;
        forward(img, nside, pf1, ipix, sig, err);
        ReverseDrizzleInput rin;
        rin.nside = nside; rin.nested = true;
        rin.leaf_ipix = ipix; rin.leaf_signal = sig;
        rin.wcs = w; rin.target_width = size; rin.target_height = size;
        rin.pixfrac = pf1; rin.output_fp64 = true;
        ReverseDrizzle rdz;
        ReverseDrizzleOutput rout;
        if (!rdz.run(rin, rout, err)) {
            CHECK(false, ("反向失败: " + err).c_str());
        } else {
            double sum = 0, sum2 = 0; size_t n = 0;
            for (int y = 4; y < size - 4; y++)   // 避开边缘
                for (int x = 4; x < size - 4; x++) {
                    double v = rout.signal[(size_t)y * size + x];
                    sum += v; sum2 += v * v; n++;
                }
            double mean = sum / n;
            double sd = std::sqrt(std::max(0.0, sum2 / n - mean * mean));
            char msg[160];
            snprintf(msg, sizeof(msg),
                     "[均匀恢复 pf=1.0] mean=%.4f rel_std=%.3e (≤1e-4 冻结门)",
                     mean, sd / mean);
            CHECK(std::fabs(mean - 1000.0) / 1000.0 < 1e-3 && sd / mean < 1e-4, msg);
        }
    }

    // ---- 2. 点源: 质心 + 通量闭合 ----
    {
        WcsParams w = make_wcs(272.886595, -23.254083, 6.3, size);
        FitsImage img;
        img.width = size; img.height = size; img.channels = 1;
        img.wcs = w;
        img.pixels.resize((size_t)size * size);
        img.pixels_f64.resize((size_t)size * size);
        const double cx = 32.0, cy = 30.0, sigma = 2.5, amp = 1000.0;
        double F = 0;
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++) {
                double dx = x - cx, dy = y - cy;
                double v = amp * std::exp(-(dx*dx + dy*dy) / (2.0 * sigma * sigma));
                img.pixels[(size_t)y * size + x] = (float)v;
                img.pixels_f64[(size_t)y * size + x] = v;
                F += v;
            }
        std::vector<uint64_t> ipix; std::vector<double> sig; std::string err;
        forward(img, nside, pf, ipix, sig, err);
        ReverseDrizzleInput rin;
        rin.nside = nside; rin.nested = true;
        rin.leaf_ipix = ipix; rin.leaf_signal = sig;
        rin.wcs = w; rin.target_width = size; rin.target_height = size;
        rin.pixfrac = pf; rin.output_fp64 = true;
        ReverseDrizzle rdz;
        ReverseDrizzleOutput rout;
        rdz.run(rin, rout, err);
        // 通量闭合: Σ 反向输出 = Σ 输入 (图像内)
        double out_sum = 0;
        for (size_t i = 0; i < rout.signal.size(); i++) out_sum += rout.signal[i];
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "[通量闭合] in=%.6g out=%.6g rel=%.3e (<1e-4)",
                 F, out_sum, std::fabs(out_sum - F) / F);
        CHECK(std::fabs(out_sum - F) / F < 1e-4, msg);
        // 质心
        double sx = 0, sy = 0, sw = 0;
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++) {
                double v = rout.signal[(size_t)y * size + x];
                sx += v * x; sy += v * y; sw += v;
            }
        double off = std::sqrt((sx/sw - cx) * (sx/sw - cx) +
                               (sy/sw - cy) * (sy/sw - cy));
        // MICROFIX #4: 质心 ≤0.01 目标像素
        snprintf(msg, sizeof(msg), "[质心恢复] offset %.4f px (≤0.01px)", off);
        CHECK(off < 0.01, msg);
        // FP32 输出
        rin.output_fp64 = false;
        ReverseDrizzleOutput rout32;
        rdz.run(rin, rout32, err);
        double max_rel = 0;
        for (size_t i = 0; i < rout.signal.size(); i++) {
            double r = std::fabs((double)rout32.signal_f32[i] - rout.signal[i]) /
                       std::max(std::fabs(rout.signal[i]), 1.0);
            if (r > max_rel) max_rel = r;
        }
        snprintf(msg, sizeof(msg), "[FP32 vs FP64] max rel %.3e (<1e-5)", max_rel);
        CHECK(max_rel < 1e-5, msg);
        // coverage 合法
        bool cov_ok = true;
        for (size_t i = 0; i < rout.coverage.size(); i++)
            if (rout.coverage[i] < 0 || !std::isfinite(rout.coverage[i])) cov_ok = false;
        CHECK(cov_ok, "[coverage] 无负值/NaN");
    }
    printf("== 反向 Drizzle 结果: %d 通过, %d 失败 ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
