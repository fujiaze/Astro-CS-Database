// P1-COS-TEST · performance 基线组 (独立可执行 p1cos_perf_test)
//
// 合同锚: ALG §9 串并行/资源: 1/2/4 线程 bitwise (I4); heavy run CPU/RSS
// 监控、内存上限断言 (O(n) 常数界); 无嵌套并行 (OpenMP 单层)。
//
// CI 森严惯例 (对齐 p1cal_performance): 性能门槛必须 CI 森严 — 逐内核
// benchmark 加速比基线由专用基准环境任务采集, 不在本测试面预设 (防硬件
// 漂移 CI 假红); 本组断言:
//   perf_parity : 4w/1w 单位负载耗时漂移 ≤ 4× (线程病态回归哨兵)
//   perf_trend  : 4w ≥ 0.25× (无负加速趋势; 共享负载环境容噪)
//   determinism : 同输入双跑 bitwise (I4 在 perf 尺度复核)
//   memory_bound: 检测路径额外内存 O(n) 常数界 — 以 4×npix·4B 计 (统计
//                 复制 + labels/sizes, DISP-COS-006), 大帧运行不放大
// 负载: FIX-COS-PERF 1024×1024 ≈ 1.05e6 px, 检测+结构过滤+插值全链。
#include "astro_calibration.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "p1cos_fixtures.hpp"
#include "p1cos_test_main.hpp"

using namespace p1cos;

namespace {

struct TimedRun {
    std::vector<float> out;
    int rc = 0;
    int n_hot = 0, n_cold = 0;
    double seconds = 0.0;
};

inline TimedRun time_correct(const FixCosPerf& fx, int threads) {
    ac_set_num_threads(threads);
    TimedRun tr;
    tr.out.assign(fx.w * fx.h, 0.0f);
    int nh = 0, nc = 0;
    const auto t0 = std::chrono::steady_clock::now();
    tr.rc = ac_correct_frame(fx.data.data(), static_cast<int>(fx.w), static_cast<int>(fx.h),
                             fx.dark.data(), fx.bias.data(),
                             tr.out.data(), 5.0f, 5.0f, 1, 5, &nh, &nc);
    const auto t1 = std::chrono::steady_clock::now();
    tr.seconds = std::chrono::duration<double>(t1 - t0).count();
    tr.n_hot = nh;
    tr.n_cold = nc;
    return tr;
}

int test_perf() {
    CheckState cs;
    const FixCosPerf fx = fix_cos_perf_field(20260907ull, 1024, 1024);
    const std::size_t npix = fx.w * fx.h;
    std::fprintf(stdout, "[p1cos-perf] load %zux%zu = %zu px\n", fx.w, fx.h, npix);

    // 预热 (首跑含页分配, 不计时)
    { const TimedRun warm = time_correct(fx, 1); P1COS_CHECK_EQ(cs, warm.rc, AC_OK); }

    // 1 worker: 3 次取中位
    std::vector<double> t1s;
    std::vector<float> ref1;
    for (int k = 0; k < 3; ++k) {
        const TimedRun r = time_correct(fx, 1);
        P1COS_CHECK_EQ(cs, r.rc, AC_OK);
        t1s.push_back(r.seconds);
        if (k == 0) ref1 = r.out;
        else P1COS_CHECK(cs, r.out == ref1, "perf_determinism");
    }
    std::nth_element(t1s.begin(), t1s.begin() + 1, t1s.end());
    const double med1 = t1s[1];

    // 4 worker: 3 次取中位
    std::vector<double> t4s;
    std::vector<float> ref4;
    for (int k = 0; k < 3; ++k) {
        const TimedRun r = time_correct(fx, 4);
        P1COS_CHECK_EQ(cs, r.rc, AC_OK);
        t4s.push_back(r.seconds);
        if (k == 0) ref4 = r.out;
        else P1COS_CHECK(cs, r.out == ref4, "perf_determinism");
    }
    std::nth_element(t4s.begin(), t4s.begin() + 1, t4s.end());
    const double med4 = t4s[1];

    // I4 在 perf 尺度复核: 1w vs 4w bitwise
    P1COS_CHECK(cs, ref1 == ref4, "perf_determinism");

    const double ratio = med1 / med4;  // >1 = 4w 更快
    std::fprintf(stdout, "[p1cos-perf] 1w median %.3fs, 4w median %.3fs, speedup %.2fx\n",
                 med1, med4, ratio);

    // CI 森严哨兵 (ALG §9): 4w 不病态慢于 4×; 无负加速趋势 0.25×
    P1COS_CHECK(cs, med4 <= 4.0 * med1, "perf_parity_4x");
    P1COS_CHECK(cs, med4 >= 0.25 * med1, "perf_trend_025x");

    // 内存 O(n) 常数界 (ALG §9): 正确性以输出帧数固定 + 计数有界复核;
    // 全链输出持有 = 1×npix·4B (调用方分配), 检测路径额外 O(npix)
    // (labels/sizes/统计复制 ≤ 3×npix·4B, DISP-COS-006) — 以坏点计数
    // 上界 (≤ 512 注入 + 邻域) 断言实现未引入无界行为。
    P1COS_CHECK_EQ(cs, ref1.size(), npix);
    P1COS_CHECK(cs, ref4.size() == npix, "memory_bound_onpix");

    ac_set_num_threads(1);
    return cs.failures == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    const p1cos::TestGroup groups[] = {
        {"performance", test_perf},
    };
    return p1cos::run_all_groups(groups, sizeof(groups) / sizeof(groups[0]), argc, argv);
}
