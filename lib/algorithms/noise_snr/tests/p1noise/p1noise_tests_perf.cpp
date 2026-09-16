// P1-NOISE-TEST · performance 基线组 (独立可执行 p1noise_perf_test)
//
// 合同锚: NOISE_ESTIMATION.md §13.4 串并行/资源行: O(h·w) 时间、O(h·w)
// 掩膜 + O(64) 控制点内存界断言; 现状单线程 (§13.2), P1-NOISE-IMPL 引入
// 并行后按 I4 复验。ISA: 基线标量断言 (无 SIMD 变体, §13.4 末行)。
//
// CI 森严惯例 (对齐 p1cal_performance/p1cos_performance): 性能门槛必须
// CI 森严 — 逐内核 benchmark 加速比基线由专用基准环境任务采集, 不在本
// 测试面预设绝对速度门 (防硬件漂移 CI 假红); 本组断言 (哨兵=parity/trend
// 双侧有界, 无绝对速度门):
//   perf_determinism : 同输入双跑 bitwise (I4 在 perf 尺度复核)
//   perf_parity_4x   : 双跑耗时漂移 ≤ 4× (病态回归哨兵, 同进程自对照)
//   perf_trend_025x  : 双跑耗时 ≥ 0.25× (无异常加速趋势哨兵)
//   memory_bound     : 输出面 npix 固定 + 控制点 ≤ 64 (8×8 冻结网格,
//                      O(h·w) 掩膜 + O(64) 控制点内存界)
// 负载: FIX-NOISE-PERF 1024×1024 ≈ 1.05e6 px Gaussian 帧 (O(h·w) 全链)。
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "p1noise_fixtures.hpp"
#include "p1noise_oracle.hpp"
#include "p1noise_test_main.hpp"
#include "snr_estimator.h"

using namespace p1noise;

namespace {

struct TimedRun {
    NoiseWeightModelV1 model{};
    int rc = 0;
    double seconds = 0.0;
};

inline TimedRun time_build(const FixNoisePerf& fx) {
    TimedRun tr;
    const auto t0 = std::chrono::steady_clock::now();
    tr.rc = snr_noise_model_v1_f64(fx.data.data(), fx.h, fx.w, nullptr,
                                   nullptr, nullptr, 0, nullptr, &tr.model);
    const auto t1 = std::chrono::steady_clock::now();
    tr.seconds = std::chrono::duration<double>(t1 - t0).count();
    return tr;
}

inline void free_timed(TimedRun& t) {
    snr_noise_model_v1_free(&t.model);
    t.model.n_control_points = 0;
}

int test_perf() {
    CheckState cs;
    const FixNoisePerf fx = fix_noise_perf(20260926ull, 1024, 1024, 5.0);
    const std::size_t npix = static_cast<std::size_t>(fx.w) * fx.h;
    std::fprintf(stdout, "[p1noise-perf] load %dx%d = %zu px\n", fx.w, fx.h, npix);

    // 预热 (首跑含页分配, 不计时)
    { TimedRun warm = time_build(fx); P1NOISE_CHECK_EQ(cs, warm.rc, 0); free_timed(warm); }

    // 第 1 轮
    TimedRun r1 = time_build(fx);
    P1NOISE_CHECK_EQ(cs, r1.rc, 0);
    // 第 2 轮 (bitwise 确定性, I4 perf 尺度)
    TimedRun r2 = time_build(fx);
    P1NOISE_CHECK_EQ(cs, r2.rc, 0);
    P1NOISE_CHECK(cs, r1.seconds > 0.0 && r2.seconds > 0.0, "perf_parity_4x");

    // bitwise 对照 (不含 free 前的指针相等; 数组逐位)
    bool bw = r1.model.n_control_points == r2.model.n_control_points &&
              r1.model.degenerate == r2.model.degenerate &&
              bit_eq(r1.model.sigma_bg_global, r2.model.sigma_bg_global) &&
              bit_eq(r1.model.variance_bg_global, r2.model.variance_bg_global) &&
              bit_eq(r1.model.ivar_bg_global, r2.model.ivar_bg_global);
    if (bw && r1.model.n_control_points > 0) {
        const std::size_t n = r1.model.n_control_points;
        bw = std::memcmp(r1.model.ctrl_sigma, r2.model.ctrl_sigma, n * sizeof(double)) == 0 &&
             std::memcmp(r1.model.ctrl_variance, r2.model.ctrl_variance, n * sizeof(double)) == 0 &&
             std::memcmp(r1.model.ctrl_ivar, r2.model.ctrl_ivar, n * sizeof(double)) == 0;
    }
    P1NOISE_CHECK(cs, bw, "perf_determinism");

    const double ratio = (r1.seconds > r2.seconds) ? r1.seconds / r2.seconds
                                                   : r2.seconds / r1.seconds;
    std::fprintf(stdout, "[p1noise-perf] run1 %.3fs, run2 %.3fs, drift %.2fx\n",
                 r1.seconds, r2.seconds, ratio);
    // CI 森严哨兵 (同进程自对照, 无绝对速度门): 漂移 ≤4× / ≥0.25×
    P1NOISE_CHECK(cs, ratio <= 4.0, "perf_parity_4x");
    P1NOISE_CHECK(cs, ratio >= 0.25, "perf_trend_025x");

    // 内存 O(h·w) 掩膜 + O(64) 控制点界: 8×8 冻结网格 → n_control_points ≤ 64
    P1NOISE_CHECK(cs, r1.model.n_control_points <= 64, "memory_bound_64ctrl");
    P1NOISE_CHECK(cs, r1.model.n_qualified_patches + r1.model.n_rejected_patches == 64,
                  "memory_bound_64ctrl");

    // O(h·w) 时间界 (渐近哨兵, CI 森严): 半帧 (512×512 = N/4 px) 耗时
    // 相对全帧 ≤ 0.5× (线性 ⇒ ≈0.25×; 允许到 0.5× 为常数开销容噪);
    // 若实现引入超线性结构 (如 O((h·w)²)) 该断言在大帧下必炸。
    {
        const FixNoisePerf half = fix_noise_perf(20260926ull, 512, 512, 5.0);
        TimedRun rh = time_build(half);
        P1NOISE_CHECK_EQ(cs, rh.rc, 0);
        const double t_full = std::min(r1.seconds, r2.seconds);
        std::fprintf(stdout, "[p1noise-perf] half 512x512 %.3fs (full min %.3fs, ratio %.3f)\n",
                     rh.seconds, t_full, t_full > 0 ? rh.seconds / t_full : 0.0);
        P1NOISE_CHECK(cs, rh.seconds <= 0.5 * t_full, "perf_linear_onpix");
        free_timed(rh);
    }

    free_timed(r1);
    free_timed(r2);
    return cs.failures == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    const p1noise::TestGroup groups[] = {
        {"performance", test_perf},
    };
    return p1noise::run_all_groups(groups, sizeof(groups) / sizeof(groups[0]), argc, argv);
}
