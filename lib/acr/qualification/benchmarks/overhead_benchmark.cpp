// lib/acr/qualification/benchmarks/overhead_benchmark.cpp — E05 ACR 运行时固定开销 Benchmark
//
// 设计（06 §8 + 17 §6）：
//   1. ACR 运行时固定开销测量（submit/launch/event/alloc/merge）
//   2. 纳秒级精度：用 Google Benchmark 的 kNanosecond 单位
//   3. 四类开销：
//      - submit: parallel_for 提交+等待开销（KernelId::Custom + 极小 Range1D{0,1}）
//      - event:  Event 创建/销毁开销（默认构造 + status 查询 + 析构）
//      - alloc:  std::vector 分配/释放开销（不同尺寸 64..65536）
//      - merge:  parallel_reduce 小规模合并开销（16 元素 sum）
//   4. 固定小尺寸 + 高迭代次数（->Iterations(10000) 或 ->MinTime(1.0)）
//   5. 单线程：隔离运行时固定开销，避免调度噪声
//   6. state.SetLabel 记录操作类型
//
// 用途：为 CostEstimator 提供 FixedOverhead 曲线数据
//       （hardware_profile.hpp::FixedOverhead: median_ns / p95_ns / cold_start_ns / warm_ns）
#include "benchmark_common.hpp"

#include "astro/compute/acr.hpp"

#include <benchmark/benchmark.h>

#include <string>
#include <vector>

namespace astro::compute::qualification::bench {

// ===== 1. parallel_for 提交开销 =====
// 用 KernelId::Custom 和最小 Range1D{0,1}（1 个元素，空 lambda）
// 测量 parallel_for 的固定提交+等待开销（调度 + 任务构造 + 同步）
// 注意：包含 ev.wait()，测量的是 submit + complete 全程开销（调用方视角的真实开销）
static void overhead_submit(::benchmark::State& state) {
    // 预热（触发 runtime lazy init，不计入测量）
    {
        auto ev = parallel_for(KernelId::Custom, Range1D{0, 1},
                               [](std::size_t) {});
        ev.wait();
    }
    for (auto _ : state) {
        auto ev = parallel_for(KernelId::Custom, Range1D{0, 1},
                               [](std::size_t) {});
        ev.wait();  // 确保任务完成，测量 submit + complete 全程开销
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }
    state.SetLabel("parallel_for/submit_wait");
}

// ===== 2. Event 创建/销毁开销 =====
// 测量默认构造 Event 的生命周期开销（构造 + status 查询 + 析构）
// 注意：默认构造的 Event 不持有 runtime event impl（impl_ == nullptr），
//       此处测的是 Event 对象本身的开销；
//       runtime event 的创建开销耦合在 parallel_for 内，由 overhead_submit 覆盖。
static void overhead_event(::benchmark::State& state) {
    // 预热
    {
        Event e;
        auto s = e.status();
        do_not_optimize(s);
    }
    for (auto _ : state) {
        Event e;                        // 构造（shared_ptr null init）
        auto s = e.status();            // 查询状态（防止消除）
        do_not_optimize(s);
        // e 析构在此（shared_ptr release）
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }
    state.SetLabel("event/create_destroy");
}

// ===== 3. std::vector 分配/释放开销 =====
// 不同尺寸（64, 256, 1024, 4096, 65536 个 float）
// 每次迭代构造 + 析构一个 vector，测量 malloc/free + value-init 开销
static void overhead_alloc(::benchmark::State& state) {
    std::size_t n = static_cast<std::size_t>(state.range(0));
    // 预热
    {
        std::vector<float> v(n);
        if (n > 0) do_not_optimize_array(v.data(), 1);
    }
    for (auto _ : state) {
        std::vector<float> v(n);
        // 读首元素防止编译器消除分配（最小读取开销）
        if (n > 0) do_not_optimize_array(v.data(), 1);
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }
    std::string label = "alloc/vector_float/n=" + std::to_string(n);
    state.SetLabel(label);
}

// ===== 4. parallel_reduce 合并开销 =====
// 小规模（16 元素）parallel_reduce，测量框架合并开销
// （分块 + 局部归约 + 合并 + 同步）
static void overhead_merge(::benchmark::State& state) {
    constexpr std::size_t n = 16;
    std::vector<float> x(n, 1.0f);
    // 预热
    {
        float r = parallel_reduce<float>(KernelId::Dot, Range1D{0, n}, 0.0f,
            [&](std::size_t i) { return x[i]; },
            [](float a, float b) { return a + b; });
        do_not_optimize(r);
    }
    for (auto _ : state) {
        float s = parallel_reduce<float>(KernelId::Dot, Range1D{0, n}, 0.0f,
            [&](std::size_t i) { return x[i]; },
            [](float a, float b) { return a + b; });
        do_not_optimize(s);
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }
    state.SetLabel("parallel_reduce/merge_small");
}

// ===== 注册 benchmark =====
// 固定小尺寸 + 高迭代次数（纳秒级开销需要大量采样）
// Repetitions 提供统计显著性（median + stddev）
BENCHMARK(overhead_submit)
    ->Iterations(10000)
    ->Unit(::benchmark::kNanosecond)
    ->Repetitions(9);

BENCHMARK(overhead_event)
    ->Iterations(10000)
    ->Unit(::benchmark::kNanosecond)
    ->Repetitions(9);

BENCHMARK(overhead_alloc)
    ->Arg(64)->Arg(256)->Arg(1024)->Arg(4096)->Arg(65536)
    ->MinTime(1.0)
    ->Unit(::benchmark::kNanosecond)
    ->Repetitions(5);

BENCHMARK(overhead_merge)
    ->Iterations(10000)
    ->Unit(::benchmark::kNanosecond)
    ->Repetitions(9);

} // namespace astro::compute::qualification::bench
