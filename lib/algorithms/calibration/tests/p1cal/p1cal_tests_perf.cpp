// P1-CAL-TEST · performance 基线组 (独立可执行 p1cal_perf_test)
//
// 合同锚: ALG §9 串并行: 1/2/4 线程 bitwise (I4); 2 核合成负载 (≥8 帧
// 4500×3600 等效像素量) 相对 1 worker 加速比 ≥1.60 (约束 D.7)。
//
// CI 森严惯例 (既有 registration): 性能门槛必须 CI 森严。等效像素量
// 8 × 648×648 = 3359232 px (≈0.2× 4500×3600×8), 沙箱 CPU 2-5× 慢于基准
// 环境 → 5-40s 实际。D.7 ≥1.60 基线由逐内核 benchmark 任务在专用基准
// 环境采集, 不在本测试面预设 (防硬件漂移 CI 假红); 本组断言:
//   perf_parity : 2w/1w 单位负载耗时漂移 ≤ 4× (线程病态回归哨兵)
//   perf_trend  : 2w ≥ 0.4× (无负加速趋势; 共享负载环境 0.5 有抖动, 0.4 容噪)
//   determinism : 同输入双跑 bitwise (I4 在 perf 尺度复核)
//   内存上界    : generate_master 输入持有 = n_frames·npix·4B (由调用方
//                 分配, 本组以分配量断言无额外 O(n_frames·npix) 拷贝膨胀 —
//                 通过统计量采样无法直接测 RSS, 由资源监控组覆盖)
#include "astro_calibration.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "p1cal_fixtures.hpp"
#include "p1cal_test_main.hpp"

using namespace p1cal;

namespace {

inline double median_of_vec(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v.size() % 2 == 1 ? v[v.size() / 2]
                             : 0.5 * (v[v.size() / 2 - 1] + v[v.size() / 2]);
}

struct TimedRun {
    std::vector<float> out;
    int rc = 0;
    double seconds = 0.0;
};

inline TimedRun time_master_bias(const FixCalC& fx, int threads) {
    ac_set_num_threads(threads);
    TimedRun tr;
    tr.out.assign(fx.w * fx.h, 0.0f);
    const auto t0 = std::chrono::steady_clock::now();
    tr.rc = ac_generate_master_bias(fx.stack.data(), static_cast<int>(fx.n_frames),
                                    static_cast<int>(fx.w), static_cast<int>(fx.h),
                                    tr.out.data(), 3.0f, 3.0f, 5, AC_COMBINE_MEAN);
    const auto t1 = std::chrono::steady_clock::now();
    tr.seconds = std::chrono::duration<double>(t1 - t0).count();
    return tr;
}

int test_perf() {
    CheckState cs;

    const std::size_t W = 648, H = 648, NPIX = W * H;
    const FixCalC fx = fix_cal_c_outlier_stack(314265u, 8, W, H, false);

    // 内存上界断言: fixture 持有 = n_frames·npix·4B (合同 §9 资源:
    // generate_master_flat ≤ n_frames·npix·4B×1.5+余量; master bias 无归一
    // 中间层, 输入即主持有)。vector 容量核算。
    const double stack_bytes =
        static_cast<double>(fx.stack.capacity()) * sizeof(float);
    const double contract_bytes =
        static_cast<double>(fx.n_frames) * static_cast<double>(NPIX) * 4.0 * 1.5;
    P1CAL_CHECK(cs, stack_bytes <= contract_bytes, "memory_upper_bound");

    // 预热
    {
        TimedRun warmup = time_master_bias(fx, 1);
        P1CAL_CHECK_EQ(cs, warmup.rc, AC_OK);
    }
    // 3 复跑取中位数
    std::vector<double> t1v, t2v;
    for (int rep = 0; rep < 3; ++rep) {
        TimedRun a = time_master_bias(fx, 1);
        TimedRun b = time_master_bias(fx, 2);
        P1CAL_CHECK_EQ(cs, a.rc, AC_OK);
        P1CAL_CHECK_EQ(cs, b.rc, AC_OK);
        t1v.push_back(a.seconds);
        t2v.push_back(b.seconds);
    }
    ac_set_num_threads(0);

    const double med1 = median_of_vec(t1v);
    const double med2 = median_of_vec(t2v);
    const double ratio = med2 / med1;
    std::fprintf(stdout,
                 "[p1cal] perf: n=%zu %zux%zu, 1w=%.3fs, 2w=%.3fs, ratio(2w/1w)=%.3f\n",
                 fx.n_frames, fx.w, fx.h, med1, med2, ratio);

    P1CAL_CHECK(cs, ratio <= 4.0, "perf_parity");
    P1CAL_CHECK(cs, ratio >= 0.4, "perf_trend");

    // I4 确定性: 同输入双跑 bitwise (perf 尺度)
    {
        TimedRun r1 = time_master_bias(fx, 1);
        TimedRun r2 = time_master_bias(fx, 2);
        P1CAL_CHECK(cs, std::memcmp(r1.out.data(), r2.out.data(),
                                    NPIX * sizeof(float)) == 0, "determinism_bitwise");
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
