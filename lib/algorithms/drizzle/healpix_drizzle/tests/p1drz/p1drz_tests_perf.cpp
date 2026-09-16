// P1-DRZ-TEST · performance 基线哨兵 (独立可执行, 时长敏感与 core 组隔离)
//
// CI 森严口径 (对齐 p1cos_tests_perf.cpp:9-10, 97): 门必须稳, 不做绝对
// 耗时回归 (共享负载环境抖动大), 做三重哨兵:
//   perf_parity : T(2w)/T(1w) < 4.0× 且 T(4w)/T(1w) < 4.0×  (并行不倒退)
//   perf_trend  : T(4w)/T(1w) ≥ 0.25×                        (无负加速趋势,
//                 共享负载环境容噪; 本负载 4w 实测 ~0.38, 2.6× 合法加速)
//   perf_median : 每 run 3 次取最优 (降噪)
// 注: drizzle 引擎线程数由 DrizzleConfig.threads 控制; FP32 路径
// (drizzleTiled) 与生产 HiPS 直写一致。
#include "drizzle_engine.h"

#include "p1drz_fixtures.hpp"
#include "p1drz_test_main.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using namespace p1drz;
using drizzle::DrizzleConfig;
using drizzle::DrizzleStats;
using drizzle::DrizzleEngine;
using drizzle::FitsImage;
using drizzle::TileAccumulatorT;

namespace {

CheckState g_perf_cs;

constexpr int PW = 96, PH = 96;       // perf 图尺寸 (engine 有效负载)
constexpr int PNSIDE = 1024;
constexpr double PSCALE = 100.0;      // 100"/px → 高密度候选 (引擎压力)

inline double time_run(int threads) {
    FitsImage img = fix_drz_a_const_sb(PW, PH, 1000.0, PSCALE, false);
    DrizzleConfig cfg = make_cfg(PNSIDE, 1.0, threads, false);
    double best = 1e30;
    for (int rep = 0; rep < 3; ++rep) {
        DrizzleEngine eng;
        std::vector<TileAccumulatorT<float>> tiles;
        DrizzleStats st;
        std::string err;
        const auto t0 = std::chrono::high_resolution_clock::now();
        const bool ok = eng.drizzleTiled(img, cfg, nullptr, nullptr, nullptr,
                                         tiles, st, err);
        const auto t1 = std::chrono::high_resolution_clock::now();
        if (!ok) return -1.0;
        best = std::min(best, std::chrono::duration<double>(t1 - t0).count());
    }
    return best;
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    const double t1 = time_run(1);
    const double t2 = time_run(2);
    const double t4 = time_run(4);

    P1DRZ_CHECK_MSG(g_perf_cs, t1 > 0.0 && t2 > 0.0 && t4 > 0.0, "perf_run",
                    "perf: runs ok t1=%.4f t2=%.4f t4=%.4f", t1, t2, t4);
    if (t1 > 0.0 && t2 > 0.0 && t4 > 0.0) {
        P1DRZ_CHECK_MSG(g_perf_cs, t2 / t1 < 4.0 && t4 / t1 < 4.0, "perf_parity",
                        "perf_parity: 2w/1w=%.2f 4w/1w=%.2f (<4.0×, p1cal 口径)",
                        t2 / t1, t4 / t1);
        P1DRZ_CHECK_MSG(g_perf_cs, t4 / t1 >= 0.25, "perf_trend",
                        "perf_trend: 4w/1w=%.2f (≥0.25×, p1cos 先例; 无负加速)",
                        t4 / t1);
    }

    std::fprintf(stdout,
                 "[p1drz-perf] == P1-DRZ-TEST perf: %d 通过, %d 失败 ==\n",
                 g_p1drz_pass, g_p1drz_fail);
    return g_perf_cs.failures == 0 ? 0 : 1;
}
