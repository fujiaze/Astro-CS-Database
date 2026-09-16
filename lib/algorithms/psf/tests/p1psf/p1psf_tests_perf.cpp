// ============================================================================
// P1-PSF-TEST · performance 基线组 (独立可执行 p1psf_perf_test)
// ----------------------------------------------------------------------------
// 合同锚: ALG STAR_PSF_ALGORITHMS.md §11.4 (TEST-PSF-DESIGN-001 performance
// 组) + AstroCS_ENGINEERING_CONSTRAINTS §D.7 (多核加速比 ≥1.60, 2 核基准)。
//
// CI 森严惯例 (对齐 p1cal/p1cos/p1drz_performance): 绝对耗时门槛由专用基准
// 环境采集, 不在本测试面预设 (防硬件漂移 CI 假红); 本组断言:
//   perf_parity   : 4w/1w 单位负载耗时漂移 ≤ 4× (线程病态回归哨兵)
//   perf_speedup  : 4w 中位耗时 / 1w 中位耗时 ≥ 1.60 (E.7 加速比下限)
//   perf_determinism : 同配置双跑 bitwise + 4w vs 1w bitwise (I4 perf 复核)
// 负载: FIX-PSF-PERF 128x128, 5x5=25 星, N=300 复批 (7500 次拟合/配置)。
// ============================================================================
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "dynamic_psf.h"

#include "p1psf_fixtures.hpp"
#include "p1psf_oracle.hpp"
#include "p1psf_test_main.hpp"

using namespace p1psf;

namespace {

inline void set_threads(int n) {
#ifdef _OPENMP
    omp_set_num_threads(n);
#else
    (void)n;
#endif
}

struct TimedRun {
    std::vector<double> flat;   // [N,9] 扁平结果
    int rc = 0;
    int n_ok = 0;
    double seconds = 0.0;
};

// 一次 N=300 复批 (25 星基线重复 12 次, 扩大批量以稳定计时)
inline TimedRun time_batch(const FixPsfPerf& fx, int threads, int rep = 12) {
    set_threads(threads);
    const int N = fx.n * rep;
    std::vector<double> cx(N), cy(N);
    for (int r = 0; r < rep; ++r)
        for (int i = 0; i < fx.n; ++i) {
            cx[r * fx.n + i] = fx.cx[i];
            cy[r * fx.n + i] = fx.cy[i];
        }
    const DPSFFitParams p = default_params();
    TimedRun tr;
    const auto t0 = std::chrono::steady_clock::now();
    DPSFFitResult* rs = nullptr;
    tr.rc = dpsf_fit_batch_d(fx.f64.data(), fx.w, fx.h, cx.data(), cy.data(), N, &p, &rs);
    const auto t1 = std::chrono::steady_clock::now();
    tr.seconds = std::chrono::duration<double>(t1 - t0).count();
    if (rs) {
        tr.flat = flatten9(rs, N);
        for (int i = 0; i < N; ++i)
            if (rs[i].status == DPSF_FIT_OK) ++tr.n_ok;
        dpsf_free_results(rs);
    }
    return tr;
}

inline double median(std::vector<double> v) {
    std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
    return v[v.size() / 2];
}

int test_perf() {
    CheckState cs;
    const char* F = "perf";
    const FixPsfPerf fx = fix_psf_perf(20260909ull, 5, 128, 128);
    const int N = fx.n * 12;
    std::fprintf(stdout, "[p1psf-perf] load %dx%d, %d stars x12 rep = %d fits per config\n",
                 fx.w, fx.h, fx.n, N);

    // 预热 (首跑含页分配/分支预测, 不计时)
    { const TimedRun warm = time_batch(fx, 1); P1PSF_CHECK_EQ(cs, warm.rc, 0); }

    // 1 worker: 3 次取中位
    std::vector<double> t1s;
    std::vector<double> ref1;
    int ok1 = -1;
    for (int k = 0; k < 3; ++k) {
        const TimedRun r = time_batch(fx, 1);
        P1PSF_CHECK_EQ(cs, r.rc, 0);
        t1s.push_back(r.seconds);
        if (k == 0) { ref1 = r.flat; ok1 = r.n_ok; }
        else P1PSF_CHECK(cs, vec_bitwise_eq(ref1, r.flat), F);
    }
    const double med1 = median(t1s);

    // 4 worker: 3 次取中位
    std::vector<double> t4s;
    std::vector<double> ref4;
    int ok4 = -1;
    for (int k = 0; k < 3; ++k) {
        const TimedRun r = time_batch(fx, 4);
        P1PSF_CHECK_EQ(cs, r.rc, 0);
        t4s.push_back(r.seconds);
        if (k == 0) { ref4 = r.flat; ok4 = r.n_ok; }
        else P1PSF_CHECK(cs, vec_bitwise_eq(ref4, r.flat), F);
    }
    const double med4 = median(t4s);

    // I4 在 perf 尺度复核: 1w vs 4w bitwise + n_valid 一致
    P1PSF_CHECK_MSG(cs, ok1 == ok4 && ok1 == N && vec_bitwise_eq(ref1, ref4), F,
                    "perf: 1w/4w bitwise mismatch (ok1=%d ok4=%d N=%d)", ok1, ok4, N);

    const double ratio = med1 / med4;  // >1 = 4w 更快
    std::fprintf(stdout, "[p1psf-perf] 1w median %.3fs, 4w median %.3fs, speedup %.2fx\n",
                 med1, med4, ratio);

    // CI 森严哨兵: 病态回归 4× 上界 + E.7 加速比下限 1.60
    P1PSF_CHECK_MSG(cs, med4 <= 4.0 * med1, F,
                    "perf_parity: 4w %.3fs > 4x 1w %.3fs", med4, med1);
    P1PSF_CHECK_MSG(cs, ratio >= 1.60, F,
                    "perf_speedup(E.7): %.2fx < 1.60 (med1=%.3fs med4=%.3fs)", ratio, med1, med4);

    set_threads(1);
    return cs.failures == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    const p1psf::TestGroup groups[] = {
        {"performance", test_perf},
    };
    return p1psf::run_all_groups(groups, sizeof(groups) / sizeof(groups[0]), argc, argv);
}
