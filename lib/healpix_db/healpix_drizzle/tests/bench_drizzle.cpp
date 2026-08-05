// ============================================================================
// bench_drizzle.cpp - 小图 Drizzle 基准 (剖析基线, 不写 HISS)
// 用法: bench_drizzle.exe <size> <nside> <precision 0|1> [OMP_NUM_THREADS]
// 输出: JSONL (DRIZZLE_PROFILE_SPEC)
// ============================================================================
#include "drizzle_engine.h"
#include "wcs_sip.h"
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <string>
#include <unordered_map>
#include <omp.h>

using namespace drizzle;

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: bench_drizzle.exe <size> <nside> <prec 0|1> [threads]\n");
        return 2;
    }
    int size = atoi(argv[1]);
    int nside = atoi(argv[2]);
    int prec = atoi(argv[3]);
    if (argc >= 5) {
        std::string env = std::string("OMP_NUM_THREADS=") + argv[4];
        putenv(const_cast<char*>(env.c_str()));
    }

    // 合成 TAN-SIP 图像: ~6.3"/px, 常量通量 + 轻梯度
    FitsImage img;
    img.width = size;
    img.height = size;
    img.channels = 1;
    img.wcs.has_wcs = true;
    std::strncpy(img.wcs.ctype1, "RA---TAN-SIP", 15);
    std::strncpy(img.wcs.ctype2, "DEC--TAN-SIP", 15);
    img.wcs.crval[0] = 272.886595;
    img.wcs.crval[1] = -23.254083;
    img.wcs.crpix[0] = size / 2.0 + 0.5;
    img.wcs.crpix[1] = size / 2.0 + 0.5;
    double scale = 6.3 / 3600.0;  // 度/像素
    img.wcs.cd[0] = -scale; img.wcs.cd[1] = 0.0;
    img.wcs.cd[2] = 0.0;  img.wcs.cd[3] = scale;
    img.wcs.sip.order = 1;  // 轻 SIP 畸变 (A11=1e-7)
    img.wcs.sip.a[0] = 1e-7;
    img.wcs.sip.b[3] = -1e-7;
    img.pixels.resize((size_t)size * size);
    img.pixels_f64.resize((size_t)size * size);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float v = 1000.0f + 0.01f * x + 0.005f * y;
            img.pixels[(size_t)y * size + x] = v;
            img.pixels_f64[(size_t)y * size + x] = v;
        }
    }
    img.use_f64 = (prec == 1);

    DrizzleConfig cfg;
    cfg.nside = nside;
    cfg.nested = true;
    cfg.pixfrac = 1.0;
    cfg.precision_mode = (uint8_t)prec;
    if (argc >= 5) cfg.threads = atoi(argv[4]);

    DrizzleEngine engine;
    std::unordered_map<uint64_t, PixelAccumulator> acc;
    DrizzleStats stats;
    std::string err;

    auto t0 = std::chrono::steady_clock::now();
    bool ok = (prec == 1)
        ? engine.drizzle_f64(img, cfg, nullptr, nullptr, acc, stats, err)
        : engine.drizzle(img, cfg, nullptr, nullptr, acc, stats, err);
    auto t1 = std::chrono::steady_clock::now();
    double wall = std::chrono::duration<double>(t1 - t0).count();

    if (!ok) {
        fprintf(stderr, "drizzle failed: %s\n", err.c_str());
        return 1;
    }
    double total_flux = 0.0;
    for (const auto& [ipix, pa] : acc) {
        total_flux += pa.sumFlux;
    }
    printf("{\"input_pixels\":%d,\"output_pixels\":%zu,\"nside\":%d,"
           "\"precision\":\"%s\",\"wall_s\":%.4f,\"elapsed_engine_s\":%.4f,"
           "\"threads\":%d,\"config_threads\":%d,\"total_flux\":%.6f,\"status\":\"%s\"}\n",
           size * size, acc.size(), stats.nside,
           prec ? "fp64" : "fp32", wall, stats.elapsedSec,
           omp_get_max_threads(), cfg.threads, total_flux, ok ? "PASS" : "FAIL");
    return 0;
}
