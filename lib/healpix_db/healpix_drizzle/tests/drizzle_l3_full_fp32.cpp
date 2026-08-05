// ============================================================================
// drizzle_l3_full_fp32.cpp - Drizzle L3 / Gate D: 唯一一次完整 FP32 验收
//
// 输入: 4500x3600 已授权单帧 (Galaxy_Center_mosaic3_T4)
// 配置: nside=65536 (与基线自动 NSIDE 一致), pixfrac=1.0, 16 线程, FP32 累计
// 验收: Drizzle 计算 <= 90s (目标 60s); 输出像素与基线参考一致 (61,592,234);
//       通量闭合; 无 NaN/Inf; 峰值 RSS 显著低于旧多 hash 结构
//
// 编译 (tests/ 目录): 同 drizzle_l0_test.cpp
// ============================================================================
#include "drizzle_engine.h"
#include "hiss_format.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <chrono>
#include <windows.h>
#include <psapi.h>

using namespace drizzle;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); ++g_pass; } \
    else { printf("  [FAIL] %s\n", msg); ++g_fail; } \
} while (0)

// 峰值工作集 (MB)
static double peak_rss_mb() {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return (double)pmc.PeakWorkingSetSize / (1024.0 * 1024.0);
    return 0.0;
}

int main(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1]
        : "testdata/Galaxy_Center_T4/lights/panel3/Galaxy_Center_mosaic3_T4_flying_dutchman-20250718@001638-180S-Red.fts";
    printf("=== L3 / Gate D: 唯一一次完整 FP32 Drizzle ===\n");
    printf("  输入: %s\n", path);

    FitsImage img;
    std::string err;
    if (!readFits(path, img, err)) {
        printf("  [FAIL] 读取 FITS 失败: %s\n", err.c_str());
        return 1;
    }
    // FP32 验收: 只读 float 像素 (drizzleTiled); 不复制 f64 (省 130MB RSS)
    double sum_in = 0.0;
    int n_finite = 0;
    for (size_t i = 0; i < img.pixels.size(); i++) {
        if (std::isfinite(img.pixels[i])) { sum_in += img.pixels[i]; n_finite++; }
    }
    printf("  图像: %dx%d, 有限像素 %d, Σin=%.9g\n",
           img.width, img.height, n_finite, sum_in);

    DrizzleConfig cfg;
    cfg.nside = 65536;   // 与基线自动 NSIDE 一致
    cfg.nested = true;
    cfg.pixfrac = 1.0;
    cfg.precision_mode = 0;  // FP32
    cfg.threads = 16;

    DrizzleEngine engine;
    std::vector<TileAccumulatorT<float>> tiles;
    DrizzleStats stats;
    auto t0 = std::chrono::steady_clock::now();
    bool ok = engine.drizzleTiled(img, cfg, nullptr, nullptr, tiles, stats, err);
    auto t1 = std::chrono::steady_clock::now();
    double wall = std::chrono::duration<double>(t1 - t0).count();
    if (!ok) {
        CHECK(false, ("FP32 完整 Drizzle 失败: " + err).c_str());
        return 1;
    }

    double sum_out = 0.0;
    bool finite_ok = true;
    uint32_t depth = hiss::compute_tile_depth((uint32_t)cfg.nside);
    int shift = 2 * (int)depth;
    int64_t n_leaf = 0;
    for (const auto& tile : tiles) {
        for (uint32_t local : tile.touched) {
            const auto& p = tile.pixels[local];
            if (!std::isfinite(p.sumFlux) || !std::isfinite(p.sumArea)) finite_ok = false;
            sum_out += p.sumFlux;
            n_leaf++;
        }
    }
    double rel = std::fabs(sum_out - sum_in) / std::fabs(sum_in);
    char name[160];
    snprintf(name, sizeof(name), "Drizzle 计算 %.3fs (硬上限 90s, 目标 60s)", wall);
    CHECK(wall <= 90.0, name);
    snprintf(name, sizeof(name), "输出有效 HEALPix 像素 %lld (基线参考 61,592,234, nside=65536)",
             (long long)n_leaf);
    CHECK(n_leaf > 60000000LL && n_leaf < 63000000LL, name);
    snprintf(name, sizeof(name), "通量闭合 Σout=%.9g Σin=%.9g rel=%.3e (<1e-4)",
             sum_out, sum_in, rel);
    CHECK(rel < 1e-4, name);
    CHECK(finite_ok, "无 NaN/Inf");
    snprintf(name, sizeof(name), "峰值 RSS %.1f MB (旧多 hash 结构 ~4.9GB 量级, 显著下降)",
             peak_rss_mb());
    CHECK(peak_rss_mb() < 4096.0, name);

    printf("  stats: %lld 源像素 → %lld HEALPix 像素, %zu tile, 引擎 %.3fs\n",
           (long long)stats.nSourcePixels, (long long)stats.nHealpixPixels,
           tiles.size(), stats.elapsedSec);
    printf("== Gate D 结果: %d 通过, %d 失败 ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
