// lib/acr/qualification/benchmarks/atomic_benchmark.cpp — 原子操作吞吐 Benchmark
//
// 设计（06_QUALIFICATION_BENCHMARK_SPEC.md + 17_CLASSIC_EXPERIMENT_SUITE.md）：
// 1. 原子操作吞吐测量（fetch_add / CAS / atomic histogram，不同竞争级别）
// 2. 单线程基线 + 多线程竞争（1, 2, 4, 8, 16 线程）
// 3. fetch_add: 多线程对共享计数器执行 atomic fetch_add
// 4. CAS: 多线程对共享变量执行 compare_exchange 循环
// 5. atomic histogram: 用 atomic 计数器的 256 bin 直方图
// 6. 用 std::atomic<std::uint32_t>
// 7. 使用 Google Benchmark 的 ->Threads(n) 参数化线程数
// 8. 报告 MOp/s
// 9. 用于 CPU 硬件画像补全（CapabilityFamily::Irregular）
#include "benchmark_common.hpp"

#include <benchmark/benchmark.h>

#include <atomic>
#include <thread>
#include <vector>

namespace astro::compute::qualification::bench {

namespace {

// Histogram 输入映射：float (0,1] → [0, 255]
// fill_positive 生成 (0, 1]，v * 256 后可能 >= 256，需 clamp。
int to_bin256(float v) noexcept {
    int bin = static_cast<int>(v * 256.0f);
    if (bin < 0) bin = 0;
    else if (bin >= 256) bin = 255;
    return bin;
}

} // anonymous namespace

// ===== Atomic fetch_add benchmark body =====
// state.range(0) = 总操作数（跨所有线程）
// ->Threads(n) = 线程数（Google Benchmark 自动管理）
// 每线程执行 per_thread = total_ops / threads 次 fetch_add
static void atomic_fetch_add(::benchmark::State& state) {
    const std::size_t total_ops = static_cast<std::size_t>(state.range(0));
    const std::size_t num_threads = static_cast<std::size_t>(state.threads());
    const std::size_t per_thread = num_threads > 0 ? total_ops / num_threads : total_ops;

    // 共享原子计数器（static：所有线程共享，制造竞争）
    // 值跨 benchmark 实例累积——原子操作吞吐与值无关，uint32_t 回绕是良定义的。
    // Google Benchmark 顺序执行实例，不存在跨实例并发。
    static std::atomic<std::uint32_t> counter{0};

    for (auto _ : state) {
        for (std::size_t i = 0; i < per_thread; ++i) {
            counter.fetch_add(1, std::memory_order_relaxed);
        }
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }
    do_not_optimize(counter.load(std::memory_order_relaxed));

    // ops = per_thread（每线程），summed across threads = total_ops * iterations
    state.SetItemsProcessed(static_cast<int64_t>(per_thread) * state.iterations());
    state.counters["MOp/s"] = ::benchmark::Counter(
        static_cast<double>(per_thread) * 1e-6,
        ::benchmark::Counter::kIsIterationInvariantRate);
}

// ===== Atomic CAS (compare_exchange) benchmark body =====
// state.range(0) = 总操作数
// ->Threads(n) = 线程数
// 每线程执行 per_thread 次成功 CAS（自旋重试直到成功）
static void atomic_cas(::benchmark::State& state) {
    const std::size_t total_ops = static_cast<std::size_t>(state.range(0));
    const std::size_t num_threads = static_cast<std::size_t>(state.threads());
    const std::size_t per_thread = num_threads > 0 ? total_ops / num_threads : total_ops;

    // 共享原子变量（static：所有线程共享，CAS 竞争）
    static std::atomic<std::uint32_t> value{0};

    for (auto _ : state) {
        for (std::size_t i = 0; i < per_thread; ++i) {
            std::uint32_t expected = value.load(std::memory_order_relaxed);
            while (!value.compare_exchange_weak(expected, expected + 1,
                                                std::memory_order_relaxed,
                                                std::memory_order_relaxed)) {
                // CAS 失败时 expected 被自动更新为当前值，循环重试
            }
        }
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }
    do_not_optimize(value.load(std::memory_order_relaxed));

    state.SetItemsProcessed(static_cast<int64_t>(per_thread) * state.iterations());
    state.counters["MOp/s"] = ::benchmark::Counter(
        static_cast<double>(per_thread) * 1e-6,
        ::benchmark::Counter::kIsIterationInvariantRate);
}

// ===== Atomic histogram benchmark body =====
// state.range(0) = 元素数
// ->Threads(n) = 线程数
// 每线程处理输入数组的一个切片，对共享 atomic bins 执行 fetch_add
static void atomic_histogram256(::benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    const std::size_t num_threads = static_cast<std::size_t>(state.threads());
    const std::size_t tid = static_cast<std::size_t>(state.thread_index());

    // 每线程独立输入数组（确定性，相同 seed）
    std::vector<float> x(n);
    fill_positive(x.data(), n, kBenchmarkSeed);

    // 共享原子 bins（static：跨线程共享，制造 histogram bin 竞争）
    // 值累积不影响吞吐测量；uint32_t 回绕是良定义的。
    static std::atomic<std::uint32_t> bins[256];

    // 线程切片：每个线程处理 n/threads 个连续元素
    const std::size_t per_thread = num_threads > 0 ? n / num_threads : n;
    const std::size_t start = tid * per_thread;
    const std::size_t end = (tid == num_threads - 1) ? n : start + per_thread;

    for (auto _ : state) {
        for (std::size_t i = start; i < end; ++i) {
            bins[to_bin256(x[i])].fetch_add(1, std::memory_order_relaxed);
        }
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }
    do_not_optimize(bins[0].load(std::memory_order_relaxed));
    do_not_optimize(bins[255].load(std::memory_order_relaxed));

    // ops = (end - start)（每线程），summed across threads = n * iterations
    state.SetItemsProcessed(static_cast<int64_t>(end - start) * state.iterations());
    state.counters["MOp/s"] = ::benchmark::Counter(
        static_cast<double>(end - start) * 1e-6,
        ::benchmark::Counter::kIsIterationInvariantRate);
}

// ===== 注册 benchmark =====
// fetch_add: 总操作数 (1M, 10M) × 线程数 (1, 2, 4, 8, 16)
#define ACR_BENCH_REGISTER_ATOMIC_FETCH_ADD(OPS)                              \
    BENCHMARK(atomic_fetch_add)->Arg(OPS)->Threads(1)                         \
        ->Unit(::benchmark::kMillisecond)->UseRealTime();                     \
    BENCHMARK(atomic_fetch_add)->Arg(OPS)->Threads(2)                         \
        ->Unit(::benchmark::kMillisecond)->UseRealTime();                     \
    BENCHMARK(atomic_fetch_add)->Arg(OPS)->Threads(4)                         \
        ->Unit(::benchmark::kMillisecond)->UseRealTime();                     \
    BENCHMARK(atomic_fetch_add)->Arg(OPS)->Threads(8)                         \
        ->Unit(::benchmark::kMillisecond)->UseRealTime();                     \
    BENCHMARK(atomic_fetch_add)->Arg(OPS)->Threads(16)                        \
        ->Unit(::benchmark::kMillisecond)->UseRealTime();

ACR_BENCH_REGISTER_ATOMIC_FETCH_ADD(1 << 20)    // 1M
ACR_BENCH_REGISTER_ATOMIC_FETCH_ADD(10 << 20)   // 10M

// CAS: 总操作数 (1M, 10M) × 线程数 (1, 2, 4, 8, 16)
#define ACR_BENCH_REGISTER_ATOMIC_CAS(OPS)                                   \
    BENCHMARK(atomic_cas)->Arg(OPS)->Threads(1)                              \
        ->Unit(::benchmark::kMillisecond)->UseRealTime();                    \
    BENCHMARK(atomic_cas)->Arg(OPS)->Threads(2)                              \
        ->Unit(::benchmark::kMillisecond)->UseRealTime();                    \
    BENCHMARK(atomic_cas)->Arg(OPS)->Threads(4)                              \
        ->Unit(::benchmark::kMillisecond)->UseRealTime();                    \
    BENCHMARK(atomic_cas)->Arg(OPS)->Threads(8)                              \
        ->Unit(::benchmark::kMillisecond)->UseRealTime();                    \
    BENCHMARK(atomic_cas)->Arg(OPS)->Threads(16)                             \
        ->Unit(::benchmark::kMillisecond)->UseRealTime();

ACR_BENCH_REGISTER_ATOMIC_CAS(1 << 20)    // 1M
ACR_BENCH_REGISTER_ATOMIC_CAS(10 << 20)   // 10M

// atomic histogram: 元素数 (1M, 4M) × 线程数 (1, 4, 8)
#define ACR_BENCH_REGISTER_ATOMIC_HISTOGRAM(OPS)                             \
    BENCHMARK(atomic_histogram256)->Arg(OPS)->Threads(1)                     \
        ->Unit(::benchmark::kMillisecond)->UseRealTime();                    \
    BENCHMARK(atomic_histogram256)->Arg(OPS)->Threads(4)                     \
        ->Unit(::benchmark::kMillisecond)->UseRealTime();                    \
    BENCHMARK(atomic_histogram256)->Arg(OPS)->Threads(8)                     \
        ->Unit(::benchmark::kMillisecond)->UseRealTime();

ACR_BENCH_REGISTER_ATOMIC_HISTOGRAM(1 << 20)    // 1M
ACR_BENCH_REGISTER_ATOMIC_HISTOGRAM(4 << 20)    // 4M

} // namespace astro::compute::qualification::bench
