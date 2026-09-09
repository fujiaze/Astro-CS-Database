// P1-PHOT-TEST · performance 基线哨兵 (独立可执行, 时长敏感与 core 组隔离)
//
// CI 森严口径 (对齐 p1hips_tests_perf.cpp / P1-NOISE-TEST 先例): 门必须稳,
// 不做绝对耗时回归 (共享负载环境抖动大), 比值哨兵:
//   perf_parity : 校正主导负载 T(threads=4)/T(threads=1) < 4.0× (并行不倒退;
//                 被测域并行轴=OpenMP 像素校正 static + F_syn dynamic 逐星,
//                 README §7/DISP: 线程数由 omp_get_max_threads() 取)
//   perf_trend  : T(threads=max)/T(threads=1) ≥ 0.25× (无负加速趋势, 容噪)
//   perf_median : 每 run 3 次取最优 (降噪)
// HIPS_WRITER §9 同型: "性能/资源 smoke 记录 (非冻结容差, 登记即可)" —
// 哨兵登记到 stdout, 不设绝对阈值。ALG 锚: PHOTOMETRIC_FIT.md §5c/§6 (无
// SIMD 冻结承诺, 比值哨兵仅防并行倒退, 不作 perf 合同)。
#include "p1phot_test_main.hpp"
#include "p1phot_fixtures.hpp"
#include "p1phot_oracle.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "photometric_calib.h"

namespace {

using namespace p1phot;
using fix::SplitMix64;

CheckState g_perf_cs;

// 校正主导负载: 1024² f64 帧 + 48 星 F1 场 (匹配/IRLS 微秒级, 像素校正
// OpenMP static 主导)
struct PerfFixture {
    fix::FrameGeom g;
    fix::StarField f;
    std::vector<double> pixels;
};

PerfFixture make_perf_fixture() {
    PerfFixture pf;
    pf.g.width = 1024;
    pf.g.height = 1024;
    pf.g.crpix1 = 512.5;
    pf.g.crpix2 = 512.5;
    pf.f = fix::fixture_f1(pf.g, 1.25, 48);
    pf.pixels.assign((std::size_t)pf.g.width * pf.g.height, 100.0);
    SplitMix64 rng(0x5EED00000000BE7FULL);
    for (auto& v : pf.pixels) v = rng.uniform(10.0, 2000.0);
    return pf;
}

double time_run(const PerfFixture& pf, int threads) {
    double best = 1e30;
    for (int rep = 0; rep < 3; ++rep) {
        std::vector<double> out((std::size_t)pf.g.width * pf.g.height, 0.0);
        int nm = -1;
        double sc = 0.0, sg = 0.0;
        PhotometricDiag dg;
        std::memset(&dg, 0, sizeof(dg));
#ifdef _OPENMP
        omp_set_num_threads(threads);
#endif
        const auto t0 = std::chrono::high_resolution_clock::now();
        const int rc = pc_calibrate_simple_f64(
            pf.pixels.data(), pf.g.width, pf.g.height,
            pf.f.gaia_ra.data(), pf.f.gaia_dec.data(), pf.f.gaia_mag.data(),
            pf.f.gaia_fsyn.data(), (int)pf.f.gaia_ra.size(),
            pf.f.psf_cx.data(), pf.f.psf_cy.data(), pf.f.psf_flux.data(),
            pf.f.psf_status.data(), (int)pf.f.psf_cx.size(),
            nullptr, nullptr, 0,
            pf.g.crval1, pf.g.crval2, pf.g.crpix1, pf.g.crpix2,
            pf.g.cd11, pf.g.cd12, pf.g.cd21, pf.g.cd22,
            0, nullptr, nullptr, nullptr, nullptr,
            out.data(), &nm, &sc, &sg, &dg);
        const auto t1 = std::chrono::high_resolution_clock::now();
        (void)rc;
        const double sec = std::chrono::duration<double>(t1 - t0).count();
        best = std::min(best, sec);
    }
    return best;
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    CheckState& cs = g_perf_cs;
    cs.failures = 0;
    cs.fault_reported = false;

    const PerfFixture pf = make_perf_fixture();
    const double t1w = time_run(pf, 1);
    const double t4w = time_run(pf, 4);
    int nmax = 4;
#ifdef _OPENMP
    nmax = omp_get_max_threads();
#endif
    const double tmw = (nmax > 4) ? time_run(pf, nmax) : t4w;
    P1PHOT_CHECK(cs, t1w > 0.0, "perf_baseline");

    const double parity4 = (t1w > 0.0) ? t4w / t1w : 1e30;
    const double paritym = (t1w > 0.0) ? tmw / t1w : 1e30;
    std::fprintf(stdout,
                 "[p1phot] perf: t1w=%.4fs t4w=%.4fs tmax(%d)w=%.4fs "
                 "parity4=%.2fx paritymax=%.2fx\n",
                 t1w, t4w, nmax, tmw, parity4, paritym);
    P1PHOT_CHECK(cs, parity4 < 4.0, "perf_parity_4w");
    P1PHOT_CHECK(cs, paritym < 4.0, "perf_parity_maxw");
    P1PHOT_CHECK(cs, paritym >= 0.25, "perf_trend_maxw");

    if (cs.failures == 0) {
        std::fprintf(stdout, "P1PHOT PERF PASS (parity4=%.2fx paritymax=%.2fx)\n",
                     parity4, paritym);
        return 0;
    }
    std::fprintf(stderr, "P1PHOT PERF FAIL (%d check(s))\n", cs.failures);
    return 1;
}
