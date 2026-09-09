// P1-WCS-TEST · performance 基线哨兵 (独立可执行, 时长敏感与 core 组隔离)
//
// CI 森严口径 (对齐 p1hips_tests_perf.cpp / p1cos_tests_perf.cpp 先例):
// 门必须稳, 不做绝对耗时回归 (共享负载环境抖动大), 三重哨兵:
//   perf_parity_2t : 单帧 FIX-WCS-A 全链 (iter_trans + extract_wcs_sip,
//                    triangle 投票 OpenMP 区) T(2T)/T(1T) < 4.0× (并行不倒退;
//                    求解主体单线程合同, parity 上界哨兵防环境回退)
//   perf_parity_4t : T(4T)/T(1T) < 4.0×
//   perf_trend_4t  : T(4T)/T(1T) ≥ 0.25× (无负加速趋势, 共享负载容噪;
//                    拟合单线程 → 4T 不加速, 下界哨兵只防病态)
// 每 run 3 次取最优 (降噪)。§11.4 无冻结性能容差 — 哨兵登记到 stdout,
// 不设绝对阈值 (对齐 HIPS 先例口径)。
#include "p1wcs_test_main.hpp"
#include "p1wcs_fixtures.hpp"
#include "p1wcs_oracle.hpp"

#include <omp.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

#include "ipv_itertrans.h"
#include "ipv_solver.h"

using namespace p1wcs;

namespace {

CheckState g_perf_cs;

// 单帧全链求解负载 (FIX-WCS-A: 256 星, order=1)
double solve_frame(unsigned seed) {
    const FixWcsA fx = fix_wcs_a_linear(seed, 0.0, 0.0);
    const auto t0 = std::chrono::high_resolution_clock::now();
    const ipv::IterTransResult r =
        ipv::iter_trans_solve(fx.U, fx.W, fx.pairs, 5.0, 1);
    ipv::WcsFitResult w;
    extract_wcs_sip(r.trans, fx.truth.ra0, fx.truth.dec0, fx.width, fx.height,
                    fx.truth.s0, fx.U, fx.W, r.inliers, &w, nullptr);
    const auto t1 = std::chrono::high_resolution_clock::now();
    if (!w.success) return -1.0;
    return std::chrono::duration<double>(t1 - t0).count();
}

// 单线程基线; nT 线程 run (3 rep 取最优均值)
double time_run(int nthreads) {
    double best = 1e30;
    for (int rep = 0; rep < 3; ++rep) {
        omp_set_num_threads(nthreads);
        double total = 0.0;
        for (int k = 0; k < 8; ++k) {
            const double t = solve_frame(20260907u + static_cast<unsigned>(k));
            if (t < 0.0) return -1.0;
            total += t;
        }
        best = std::min(best, total);
    }
    omp_set_num_threads(1);
    return best;
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    CheckState& cs = g_perf_cs;
    cs.failures = 0;
    cs.fault_reported = false;
    p1wcs::init_fault_registry_from_env();

    const double t1 = time_run(1);
    const double t2 = time_run(2);
    const double t4 = time_run(4);
    P1WCS_CHECK(cs, t1 > 0.0 && t2 > 0.0 && t4 > 0.0, "perf_baseline");

    const double parity2 = (t1 > 0.0) ? t2 / t1 : 1e30;
    const double parity4 = (t1 > 0.0) ? t4 / t1 : 1e30;
    std::fprintf(stdout,
                 "[p1wcs] perf: t1t=%.4fs t2t=%.4fs t4t=%.4fs parity2=%.2fx "
                 "parity4=%.2fx\n",
                 t1, t2, t4, parity2, parity4);
    P1WCS_CHECK(cs, parity2 < 4.0, "perf_parity_2t");
    P1WCS_CHECK(cs, parity4 < 4.0, "perf_parity_4t");
    P1WCS_CHECK(cs, parity4 >= 0.25, "perf_trend_4t");

    if (cs.failures == 0) {
        std::fprintf(stdout, "P1WCS PERF PASS (parity2=%.2fx parity4=%.2fx)\n",
                     parity2, parity4);
        return 0;
    }
    std::fprintf(stderr, "P1WCS PERF FAIL (%d check(s))\n", cs.failures);
    return 1;
}
