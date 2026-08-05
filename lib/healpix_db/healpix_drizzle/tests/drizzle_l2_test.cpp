// ============================================================================
// drizzle_l2_test.cpp - Drizzle L2 门 (控制包 Validation_Strategy L2)
//
// 真实单帧固定裁剪 (Galaxy_Center_mosaic3_T4, 中心 1024x1024, SIP order=3):
//   FP32 与 FP64 各一次科学比较:
//     1. 无 NaN/Inf
//     2. FP64 通量闭合 (Σout ≈ Σin, 按有限像素)
//     3. FP32 vs FP64 逐 leaf 最大相对差 < 1e-5
//
// 编译 (tests/ 目录): 同 drizzle_l0_test.cpp
// ============================================================================
#include "drizzle_engine.h"
#include "hiss_format.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace drizzle;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); ++g_pass; } \
    else { printf("  [FAIL] %s\n", msg); ++g_fail; } \
} while (0)

int main(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1]
        : "run/temp/l2_crop/galaxy_crop_1024.fits";
    printf("=== Drizzle L2 门: %s ===\n", path);

    FitsImage img;
    std::string err;
    if (!readFits(path, img, err)) {
        printf("  [FAIL] 读取 FITS 失败: %s\n", err.c_str());
        return 1;
    }
    // fits_reader 只填充 float 像素; FP64 路径需要 pixels_f64 (FITS 16-bit 源在
    // float 中精确, 转 double 无损)
    if (img.pixels_f64.empty()) {
        img.pixels_f64.resize(img.pixels.size());
        for (size_t i = 0; i < img.pixels.size(); i++)
            img.pixels_f64[i] = img.pixels[i];
    }
    printf("  WCS: %dx%d, SIP order=%d, has_wcs=%d\n",
           img.width, img.height, img.wcs.sip.order, (int)img.wcs.has_wcs);
    // 输入总通量 (有限像素)
    double sum_in = 0.0;
    int n_finite = 0;
    for (size_t i = 0; i < img.pixels_f64.size(); i++) {
        if (std::isfinite(img.pixels_f64[i])) { sum_in += img.pixels_f64[i]; n_finite++; }
    }
    printf("  输入: %d 像素, 有限 %d, Σin=%.9g\n",
           (int)img.pixels_f64.size(), n_finite, sum_in);

    DrizzleConfig cfg;
    cfg.nside = 65536;   // 生产 NSIDE (控制包: 不再用 2048 替代生产条件)
    cfg.nested = true;
    cfg.pixfrac = 1.0;
    cfg.precision_mode = 1; cfg.threads = 16;
    DrizzleEngine engine;
    DrizzleStats stats; 
    std::vector<TileAccumulatorT<double>> t64;
    if (!engine.drizzleTiled_f64(img, cfg, nullptr, nullptr, t64, stats, err)) {
        CHECK(false, ("FP64 drizzle 失败: " + err).c_str());
        return 1;
    }
    double sum_out = 0.0;
    bool finite_ok = true;
    uint32_t depth = hiss::compute_tile_depth((uint32_t)cfg.nside);
    int shift = 2 * (int)depth;
    std::map<uint64_t, double> ref;
    for (const auto& tile : t64) {
        for (uint32_t local : tile.touched) {
            const auto& p = tile.pixels[local];
            if (!std::isfinite(p.sumFlux) || !std::isfinite(p.sumArea)) finite_ok = false;
            sum_out += p.sumFlux;
            ref[(tile.parent_ipix << shift) | local] = p.sumFlux;
        }
    }
    double rel = std::fabs(sum_out - sum_in) / std::fabs(sum_in);
    char name[160];
    snprintf(name, sizeof(name), "FP64 通量闭合 Σout=%.9g Σin=%.9g rel=%.3e (<1e-8)",
             sum_out, sum_in, rel);
    CHECK(rel < 1e-8, name);
    CHECK(finite_ok, "FP64 无 NaN/Inf");
    printf("  FP64: %lld 源像素 → %lld HEALPix 像素 (%zu tile), %.3fs\n",
           (long long)stats.nSourcePixels, (long long)stats.nHealpixPixels,
           t64.size(), stats.elapsedSec);

    // FP32 vs FP64
    cfg.precision_mode = 0;
    std::vector<TileAccumulatorT<float>> t32;
    if (!engine.drizzleTiled(img, cfg, nullptr, nullptr, t32, stats, err)) {
        CHECK(false, "FP32 drizzle 失败");
        return 1;
    }
    double max_rel = 0.0;
    int n32 = 0, n_missing = 0;
    std::vector<double> rel_diffs;
    for (const auto& tile : t32) {
        for (uint32_t local : tile.touched) {
            uint64_t ipix = (tile.parent_ipix << shift) | local;
            n32++;
            auto it = ref.find(ipix);
            if (it == ref.end()) { n_missing++; continue; }
            double r = std::fabs((double)tile.pixels[local].sumFlux - it->second) /
                       std::max(std::fabs(it->second), 1.0);
            if (r > max_rel) max_rel = r;
            rel_diffs.push_back(r);
        }
    }
    snprintf(name, sizeof(name), "FP32 vs FP64 逐 leaf 最大相对差 %.3e (<1e-5, n32=%d)",
             max_rel, n32);
    CHECK(max_rel < 1e-5, name);
    snprintf(name, sizeof(name), "FP32 leaf 均可在 FP64 找到 (missing=%d)", n_missing);
    CHECK(n_missing == 0, name);

    // R11: FP32 ULP/相对误差分布报告 (控制包 B 门: FP32 误差分布)
    if (!rel_diffs.empty()) {
        std::sort(rel_diffs.begin(), rel_diffs.end());
        auto pct = [&](double p) {
            size_t idx = (size_t)(p * (double)rel_diffs.size());
            if (idx >= rel_diffs.size()) idx = rel_diffs.size() - 1;
            return rel_diffs[idx];
        };
        double sum = 0.0;
        for (double v : rel_diffs) sum += v;
        double mean = sum / (double)rel_diffs.size();
        // float32 相对精度 ULP ≈ 2^-24 ≈ 5.96e-8 (归一化到 [0.5,1) 时 1 ULP)
        // 相对误差 / 1e-7 近似为 ULP 倍数 (保守, 1e-7 ≈ 1.68 ULP)
        printf("  FP32 vs FP64 逐 leaf 误差分布 (n=%zu):\n", rel_diffs.size());
        printf("    mean=%.3e  p50=%.3e  p90=%.3e  p95=%.3e  p99=%.3e  max=%.3e\n",
               mean, pct(0.50), pct(0.90), pct(0.95), pct(0.99), rel_diffs.back());
        printf("    近似 ULP (相对 1e-7): p50=%.1f  p95=%.1f  max=%.1f\n",
               pct(0.50) / 1e-7, pct(0.95) / 1e-7, rel_diffs.back() / 1e-7);
        char name2[160];
        snprintf(name2, sizeof(name2), "误差分布: p95=%.3e < 1e-5 (float 累计精度内)", pct(0.95));
        CHECK(pct(0.95) < 1e-5, name2);
    }

    printf("== L2 结果: %d 通过, %d 失败 ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
