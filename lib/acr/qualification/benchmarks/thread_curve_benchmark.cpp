// lib/acr/qualification/benchmarks/thread_curve_benchmark.cpp — CPU 线程曲线 Benchmark
//
// 设计：
// 1. 线程点：1, 2, 4, 25%, 50%, 75%, 95%, 100%（去重）
// 2. 95% 是性能曲线采样点，不是正式运行少开线程
// 3. 对曲线拐点和前两名 ISA 有限补点
// 4. 记录 worker 参与情况
// 5. 测量目标：吞吐随线程数变化曲线
//
// 实现：用 ACR parallel_for 在不同线程数下运行相同 kernel
// 线程数通过 Google Benchmark 的 Threads(n) 控制（Google Benchmark 创建 n 个 worker
// 同时调用 lambda）。ACR arena 默认全线程，但 Google Benchmark Threads(n) 实际
// 让 n 个线程同时调用 → 测量多线程并发能力。
#include "benchmark_common.hpp"

#include <benchmark/benchmark.h>

#include <vector>

namespace astro::compute::qualification::bench {

// ===== 工作量参数 =====
// 固定大尺寸让并行化收益明显：16M 元素 (64MB FP32)，超出 LLC
constexpr std::size_t kThreadCurveN = 1u << 24;  // 16M elements

// 线程曲线工作 kernel：Triad 风格（memory + compute）
// 总工作量分摊到各 worker：每个 Google Benchmark 线程独立跑一遍
template<class T>
static void run_thread_curve_work(::benchmark::State& state) {
    std::size_t n = kThreadCurveN;
    std::vector<T> x(n), y(n);
    fill_uniform(x.data(), n, kBenchmarkSeed);
    fill_uniform(y.data(), n, kBenchmarkSeed ^ 0xDEADBEEF);
    T a = static_cast<T>(3.14);
    for (auto _ : state) {
        parallel_for(KernelId::Triad, Range1D{0, n}, [&](std::size_t i) {
            y[i] = a * x[i] + y[i];
        });
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }
    do_not_optimize_array(y.data(), y.size());
    // GB/s = 3 * n * sizeof(T) / time
    std::size_t bytes = 3 * n * sizeof(T);
    // Google Benchmark 在多线程模式下，state.SetBytesProcessed 自动累加所有线程
    state.SetBytesProcessed(static_cast<int64_t>(bytes) * state.iterations());
    state.SetItemsProcessed(static_cast<int64_t>(n) * state.iterations());
}

// 算术曲线工作 kernel（计算密集，用于线程曲线在算术 workload 下的表现）
static void run_thread_curve_arith(::benchmark::State& state) {
    std::size_t n = 1u << 20;  // 1M 元素
    std::vector<float> acc(n), x(n);
    fill_positive(acc.data(), n, kBenchmarkSeed);
    fill_positive(x.data(), n, kBenchmarkSeed ^ 0xA5A5);
    for (auto _ : state) {
        parallel_for(KernelId::AXPY, Range1D{0, n}, [&](std::size_t i) {
            acc[i] = acc[i] * x[i] + x[i];  // FMA
        });
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }
    do_not_optimize_array(acc.data(), acc.size());
    // 2 ops/elem
    state.SetItemsProcessed(static_cast<int64_t>(2 * n) * state.iterations());
}

// ===== 注册：固定工作量 × 各线程数 =====
// 使用 Google Benchmark 的 ->Threads(n) 让 n 个 worker 同时执行
// 我们手动列出线程点（与 06 §5 对齐）

// 计算硬件线程数（去重线程点）
inline std::vector<int> thread_curve_int_points() {
    auto hw = static_cast<std::uint32_t>(std::thread::hardware_concurrency());
    if (hw == 0) hw = 4;
    auto pts = thread_curve_points(hw);
    std::vector<int> out;
    out.reserve(pts.size());
    for (auto p : pts) out.push_back(static_cast<int>(p));
    return out;
}

// 用注册器为每个线程点单独注册
#define ACR_BENCH_REGISTER_THREAD_CURVE(NAME, FN)                                       \
    BENCHMARK(FN)->Threads(1)->Name(NAME "/threads:1")->Unit(::benchmark::kMillisecond); \
    BENCHMARK(FN)->Threads(2)->Name(NAME "/threads:2")->Unit(::benchmark::kMillisecond); \
    BENCHMARK(FN)->Threads(4)->Name(NAME "/threads:4")->Unit(::benchmark::kMillisecond);

// 内存密集型 Triad 各线程点
ACR_BENCH_REGISTER_THREAD_CURVE("thread_curve/triad_fp32", run_thread_curve_work<float>)
ACR_BENCH_REGISTER_THREAD_CURVE("thread_curve/triad_fp64", run_thread_curve_work<double>)

// 算术密集型 FMA 各线程点
ACR_BENCH_REGISTER_THREAD_CURVE("thread_curve/fma_fp32", run_thread_curve_arith)

// ===== 25%/50%/75%/95%/100% 线程点（动态计算后注册）=====
// Google Benchmark 不支持运行时动态 Threads(n)，必须编译时确定。
// 因此用 ::benchmark::RegisterBenchmark 在 main() 调用前注册（这里通过全局对象初始化）

// 注意：以下注册器在 main 之前执行（静态初始化阶段）
static const int kAutoRegisteredThreadCurve = []() {
    auto hw = static_cast<std::uint32_t>(std::thread::hardware_concurrency());
    if (hw == 0) hw = 8;
    auto pts = thread_curve_points(hw);
    // 已注册 1, 2, 4；这里补充 25%/50%/75%/95%/100%
    for (auto t : pts) {
        if (t == 1 || t == 2 || t == 4) continue;
        std::string name = "thread_curve/triad_fp32/threads:" + std::to_string(t);
        ::benchmark::RegisterBenchmark(name.c_str(), run_thread_curve_work<float>)
            ->Threads(t)
            ->Unit(::benchmark::kMillisecond);
    }
    // 算术曲线同样补点
    for (auto t : pts) {
        if (t == 1 || t == 2 || t == 4) continue;
        std::string name = "thread_curve/fma_fp32/threads:" + std::to_string(t);
        ::benchmark::RegisterBenchmark(name.c_str(), run_thread_curve_arith)
            ->Threads(t)
            ->Unit(::benchmark::kMillisecond);
    }
    return 0;
}();

// ===== worker 参与情况报告 =====
// 在每个 benchmark 结束后报告实际使用的 worker 数（通过 oneTBB）
// 这里通过 state.counters["threads"] 报告 Google Benchmark 的线程数
// 注意：实际 ACR arena 内部 worker 数与 Google Benchmark threads 不同
// 这个 counter 仅记录 Google Benchmark threads（用作曲线采样标签）
struct ThreadCurveReporter {
    ThreadCurveReporter() {
        // Google Benchmark 没有全局 install hook，这里仅作为占位
        // 实际线程数通过 case 名后缀 /threads:N 体现
    }
};
static ThreadCurveReporter g_thread_curve_reporter;

} // namespace astro::compute::qualification::bench
