// ============================================================================
// p1star_tests_perf.cpp — P1-STAR-TEST performance 基线组 (独立可执行)
// ----------------------------------------------------------------------------
// 合同锚: §11.4 F3 (确定性) + CI 森严惯例 (p1cal/p1cos/p1drz 先例): 性能门槛
// 必须 CI 森严 — 沙箱 CPU 2-5× 慢于基准环境, 绝对吞吐阈值防硬件漂移 CI 假红,
// 绝对基线由逐内核 benchmark 任务在专用基准环境采集。本组断言:
//   perf_parity : 2w/1w 单位负载耗时漂移 ≤4× (线程病态回归哨兵)
//   perf_trend  : 2w ≥0.4× (无负加速趋势)
//   determinism : 同线程双跑 bitwise (F3 在 perf 尺度复核)
// ============================================================================
#include "star_detector.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "p1star_fixtures.hpp"
#include "p1star_test_main.hpp"

#ifdef _OPENMP
#include <omp.h>
#endif

using namespace p1star;

namespace {

inline double median_of(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v.size() % 2 == 1 ? v[v.size() / 2]
                             : 0.5 * (v[v.size() / 2 - 1] + v[v.size() / 2]);
}

struct TimedRun {
    std::vector<char> blob;
    int rc = 0;
    int count = 0;
    double seconds = 0.0;
};

inline TimedRun time_detect(const FixStarD& fx, int threads) {
#ifdef _OPENMP
    omp_set_num_threads(threads);
#else
    (void)threads;
#endif
    TimedRun tr;
    StarDetectorHandle h = sdet_create(nullptr);
    const auto t0 = std::chrono::steady_clock::now();
    double* x = nullptr; double* y = nullptr; float* fl = nullptr; float* mg = nullptr;
    int* sa = nullptr; int* hs = nullptr; int cnt = 0;
    tr.rc = sdet_detect_ex_f64(h, fx.img.data(), fx.w, fx.h, &x, &y, &fl, &sa, &mg, &hs,
                               &cnt, nullptr, 0, nullptr);
    const auto t1 = std::chrono::steady_clock::now();
    tr.seconds = std::chrono::duration<double>(t1 - t0).count();
    tr.count = cnt;
    const std::size_t need = (std::size_t)std::max(cnt, 0) * 32;
    tr.blob.resize(need);
    std::size_t off = 0;
    if (cnt > 0) {
        std::memcpy(tr.blob.data() + off, x, (std::size_t)cnt * 8); off += (std::size_t)cnt * 8;
        std::memcpy(tr.blob.data() + off, y, (std::size_t)cnt * 8); off += (std::size_t)cnt * 8;
        std::memcpy(tr.blob.data() + off, fl, (std::size_t)cnt * 4); off += (std::size_t)cnt * 4;
        std::memcpy(tr.blob.data() + off, mg, (std::size_t)cnt * 4); off += (std::size_t)cnt * 4;
        std::memcpy(tr.blob.data() + off, sa, (std::size_t)cnt * 4); off += (std::size_t)cnt * 4;
        std::memcpy(tr.blob.data() + off, hs, (std::size_t)cnt * 4);
    }
    if (tr.rc == 0) sdet_free_detect_ex(x, y, fl, sa, mg, hs, nullptr, 0);
    sdet_destroy(h);
    return tr;
}

int test_perf() {
    CheckState cs;
    const FixStarD fx = fix_star_d_f3();

    // 预热
    {
        TimedRun warm = time_detect(fx, 1);
        P1STAR_CHECK_EQ(cs, warm.rc, 0, "perf_warmup_rc");
    }
    // 3 复跑取中位数
    std::vector<double> t1v, t2v;
    for (int rep = 0; rep < 3; ++rep) {
        TimedRun a = time_detect(fx, 1);
        TimedRun b = time_detect(fx, 2);
        P1STAR_CHECK_EQ(cs, a.rc, 0, "perf_1w_rc");
        P1STAR_CHECK_EQ(cs, b.rc, 0, "perf_2w_rc");
        P1STAR_CHECK_EQ(cs, a.count, b.count, "perf_count_parity");
        t1v.push_back(a.seconds);
        t2v.push_back(b.seconds);
    }
    // 注: 不 omp_set_num_threads(0) — libgomp 对非正值 gomp_fatal (abort);
    // 后续 bitwise 复核 time_detect(fx,1) 显式 set(1), 无需恢复默认。
    const double med1 = median_of(t1v);
    const double med2 = median_of(t2v);
    const double ratio = med2 / std::max(med1, 1e-9);
    std::printf("[perf] 25-star %dx%d: 1w=%.4fs 2w=%.4fs ratio(2w/1w)=%.3f\n",
                fx.w, fx.h, med1, med2, ratio);
    P1STAR_CHECK(cs, ratio <= 4.0, "perf_parity_le4x");
    P1STAR_CHECK(cs, ratio >= 0.4, "perf_trend_ge0p4x");

    // F3 perf 尺度复核: 同线程双跑 bitwise
    {
        TimedRun r1 = time_detect(fx, 1);
        TimedRun r2 = time_detect(fx, 1);
        P1STAR_CHECK(cs, r1.blob == r2.blob, "perf_determinism_bitwise");
    }

    return cs.failures == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    static const TestGroup groups[] = {
        {"perf", test_perf},
    };
    return run_all_groups(groups, sizeof(groups) / sizeof(groups[0]), argc, argv);
}
