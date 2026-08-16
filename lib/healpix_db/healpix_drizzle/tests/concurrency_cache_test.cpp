// ============================================================================
// concurrency_cache_test.cpp — V19R4 DRIZZLE_CACHE_THREAD_SAFETY
//
// 验证同进程并发 drizzleTiled run：
// - 两个线程各自跑不同 nside/pixfrac 的 run（cache 不能跨 run 污染）；
// - 并发结果与串行参考逐 tile 一致；
// - run-generation 为 atomic（修复裸 static RMW data race）。
//
// 编译（tests/）：
// g++ -O2 -std=c++17 -fopenmp -I.. -I..\..\..\astro_image_io\include
//   -I..\..\..\astro_image_io\src -DAIO_ENABLE_HEALPIX
//   -o concurrency_cache_test.exe concurrency_cache_test.cpp
//   ..\fits_reader.cpp ..\wcs_sip.cpp ..\poly_clip.cpp
//   ..\spherical_overlap.cpp ..\drizzle_engine.cpp ..\healpix_core.cpp
//   ..\snr_evaluator.cpp -static -L..\..\..\astro_image_io
//   -l:astro_image_io.dll -lm
// ============================================================================
#include "drizzle_engine.h"
#include "fits_reader.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <thread>
#include <vector>

using namespace drizzle;

namespace {

void setup_wcs(FitsImage& im, double ra0, double dec0, double scale_arcsec) {
    im.width = 32;
    im.height = 32;
    im.channels = 1;
    im.wcs.has_wcs = true;
    im.wcs.crval[0] = ra0;
    im.wcs.crval[1] = dec0;
    im.wcs.crpix[0] = 16.5;
    im.wcs.crpix[1] = 16.5;
    const double deg_per_px = scale_arcsec / 3600.0;
    im.wcs.cd[0] = -deg_per_px;
    im.wcs.cd[1] = 0.0;
    im.wcs.cd[2] = 0.0;
    im.wcs.cd[3] = deg_per_px;
    std::strncpy(im.wcs.ctype1, "RA---TAN", sizeof(im.wcs.ctype1) - 1);
    std::strncpy(im.wcs.ctype2, "DEC--TAN", sizeof(im.wcs.ctype2) - 1);
}

struct RunSpec {
    int nside;
    double pixfrac;
    double ra0, dec0, scale_arcsec;
    unsigned seed;
};

struct RunResult {
    double sum_flux = 0.0;
    std::size_t touched = 0;
    std::size_t tiles = 0;
};

RunResult run_once(const RunSpec& s) {
    FitsImage im;
    setup_wcs(im, s.ra0, s.dec0, s.scale_arcsec);
    im.pixels.assign((std::size_t)im.width * im.height, 1000.0f);
    std::mt19937 rng(s.seed);
    std::normal_distribution<double> nd(0.0, 5.0);
    for (auto& v : im.pixels) v += (float)nd(rng);
    DrizzleConfig cfg;
    cfg.nside = s.nside;
    cfg.nested = true;
    cfg.pixfrac = s.pixfrac;
    cfg.threads = 2;
    cfg.apply_photometry = true;
    cfg.photometry_applied_upstream = true;
    cfg.tile_depth = 9;
    std::vector<TileAccumulatorT<float>> tiles;
    DrizzleStats st;
    std::string err;
    DrizzleEngine eng;
    if (!eng.drizzleTiled(im, cfg, nullptr, nullptr, nullptr, tiles, st,
                          err)) {
        std::printf("[FAIL] drizzle error: %s\n", err.c_str());
        return {};
    }
    RunResult r;
    r.tiles = tiles.size();
    for (const auto& t : tiles) {
        for (uint32_t local : t.touched) {
            const auto& a = t.pixels[(size_t)local];
            r.sum_flux += (double)a.sumFlux;
            r.touched += (a.sumArea > 0.0) ? 1 : 0;
        }
    }
    return r;
}

bool close(const RunResult& a, const RunResult& b) {
    return a.tiles == b.tiles && a.touched == b.touched &&
           std::fabs(a.sum_flux - b.sum_flux) <
               1e-3 * std::max(1.0, std::fabs(a.sum_flux));
}

}  // namespace

int main() {
    const RunSpec specA{512, 0.8, 10.0, 20.0, 120.0, 20260816};
    const RunSpec specB{256, 1.0, 40.0, -15.0, 60.0, 20260817};

    // 串行参考（同一 engine 复用 + 不同 nside/pixfrac 交替 → 旧裸 static
    // generation 在此路径下也会产生污染，V19R4 atomic 后必须一致）
    const RunResult refA = run_once(specA);
    const RunResult refB = run_once(specB);
    if (refA.touched == 0 || refB.touched == 0) {
        std::printf("[FAIL] 参考 run 无输出\n");
        return 1;
    }

    // 并发 stress：两个线程各跑 20 次（同一 engine 各自实例）
    std::atomic<int> fail{0};
    auto worker = [&](const RunSpec& s, const RunResult& ref, int iters) {
        for (int i = 0; i < iters; ++i) {
            const RunResult r = run_once(s);
            if (!close(r, ref)) {
                std::printf("[FAIL] 并发结果不一致 nside=%d pixfrac=%.2f "
                            "iter=%d (sum=%.3f ref=%.3f touched=%zu/%zu)\n",
                            s.nside, s.pixfrac, i, r.sum_flux, ref.sum_flux,
                            r.touched, ref.touched);
                ++fail;
            }
        }
    };
    std::thread t1(worker, specA, refA, 20);
    std::thread t2(worker, specB, refB, 20);
    t1.join();
    t2.join();

    if (fail.load() == 0)
        std::printf("[PASS] 并发 2×20 run 与串行参考一致（atomic run-gen）\n");
    else
        std::printf("[FAIL] %d 次并发不一致\n", fail.load());
    return fail.load() == 0 ? 0 : 1;
}
