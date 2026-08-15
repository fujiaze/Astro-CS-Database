// lib/acr/qualification/benchmarks/irregular_benchmark.cpp — 不规则访问模式 Benchmark
//
// 设计（06_QUALIFICATION_BENCHMARK_SPEC.md + 17_CLASSIC_EXPERIMENT_SUITE.md）：
// 1. 不规则访问模式测量（gather / scatter / histogram）
// 2. FP32 精度，单线程
// 3. Gather: y[i] = x[indices[i]]，参数化数组大小 + 稀疏度
// 4. Scatter: y[indices[i]] = x[i]，同 gather 参数化
// 5. Histogram: 256 bin 纯 uint32_t 计数（atomic 版本在 atomic_benchmark.cpp）
// 6. 用 LCG 生成确定性随机索引
// 7. 报告 GB/s（gather/scatter）或 MOp/s（histogram）
// 8. 用于 CPU 硬件画像补全（CapabilityFamily::Irregular）
#include "benchmark_common.hpp"

#include <benchmark/benchmark.h>

#include <string>
#include <vector>

namespace astro::compute::qualification::bench {

namespace {

// 生成随机索引：indices[i] ∈ [0, index_range)
// index_range = size * sparsity_pct / 100
void fill_random_indices(std::uint32_t* indices, std::size_t n,
                         std::size_t index_range, std::uint64_t seed) {
    LCG rng(seed);
    if (index_range == 0) index_range = 1;
    for (std::size_t i = 0; i < n; ++i) {
        std::uint64_t v = rng.next();
        indices[i] = static_cast<std::uint32_t>(v % index_range);
    }
}

// Gather: y[i] = x[indices[i]]
void gather_fp32(float* y, const float* x, const std::uint32_t* indices, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        y[i] = x[indices[i]];
    }
}

// Scatter: y[indices[i]] = x[i]
void scatter_fp32(float* y, const float* x, const std::uint32_t* indices, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        y[indices[i]] = x[i];
    }
}

// Histogram 256 bin：用纯 uint32_t 计数（非原子，单线程安全）
// 输入为 float（fill_positive 生成 (0, 1]），映射到 [0, 256) bin
void histogram256_fp32(std::uint32_t* bins, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        float v = x[i];
        int bin = static_cast<int>(v * 256.0f);
        if (bin < 0) bin = 0;
        else if (bin >= 256) bin = 255;
        ++bins[bin];
    }
}

} // anonymous namespace

// ===== Gather benchmark body =====
// state.range(0) = 数组大小（元素数）
// state.range(1) = 稀疏度（百分比 10/50/100，索引范围占数组大小的比例）
static void gather_random_fp32(::benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    const int sparsity_pct = static_cast<int>(state.range(1));
    const std::size_t index_range = (n * static_cast<std::size_t>(sparsity_pct)) / 100u;
    if (index_range == 0) {
        state.SkipWithError("invalid sparsity");
        return;
    }

    std::vector<float> x(n);
    std::vector<std::uint32_t> indices(n);
    std::vector<float> y(n);
    fill_uniform(x.data(), n, kBenchmarkSeed);
    fill_random_indices(indices.data(), n, index_range, kBenchmarkSeed ^ 0xCAFE0001ULL);

    // 预热（构造不计时）
    gather_fp32(y.data(), x.data(), indices.data(), n);

    for (auto _ : state) {
        gather_fp32(y.data(), x.data(), indices.data(), n);
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }
    do_not_optimize_array(y.data(), y.size());

    // bytes = n * sizeof(float) * 2（读 indices + 读 data）
    const std::size_t bytes = n * sizeof(float) * 2;
    state.SetBytesProcessed(static_cast<int64_t>(bytes) * state.iterations());
    state.SetItemsProcessed(static_cast<int64_t>(n) * state.iterations());
    state.counters["sparsity_pct"] = ::benchmark::Counter(
        static_cast<double>(sparsity_pct), ::benchmark::Counter::kAvgThreads);
    state.SetLabel(std::string("gather/") + std::to_string(sparsity_pct) + "%");
}

// ===== Scatter benchmark body =====
// state.range(0) = 数组大小（元素数）
// state.range(1) = 稀疏度（百分比 10/50/100）
static void scatter_random_fp32(::benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    const int sparsity_pct = static_cast<int>(state.range(1));
    const std::size_t index_range = (n * static_cast<std::size_t>(sparsity_pct)) / 100u;
    if (index_range == 0) {
        state.SkipWithError("invalid sparsity");
        return;
    }

    std::vector<float> x(n);
    std::vector<std::uint32_t> indices(n);
    std::vector<float> y(n);
    fill_uniform(x.data(), n, kBenchmarkSeed);
    fill_random_indices(indices.data(), n, index_range, kBenchmarkSeed ^ 0xCAFE0001ULL);

    // 预热
    scatter_fp32(y.data(), x.data(), indices.data(), n);

    for (auto _ : state) {
        scatter_fp32(y.data(), x.data(), indices.data(), n);
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }
    do_not_optimize_array(y.data(), y.size());

    // bytes = n * sizeof(float) * 2（读 indices + 读 source data）
    const std::size_t bytes = n * sizeof(float) * 2;
    state.SetBytesProcessed(static_cast<int64_t>(bytes) * state.iterations());
    state.SetItemsProcessed(static_cast<int64_t>(n) * state.iterations());
    state.counters["sparsity_pct"] = ::benchmark::Counter(
        static_cast<double>(sparsity_pct), ::benchmark::Counter::kAvgThreads);
    state.SetLabel(std::string("scatter/") + std::to_string(sparsity_pct) + "%");
}

// ===== Histogram benchmark body =====
// state.range(0) = 元素数
static void histogram256_uniform_fp32(::benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));

    std::vector<float> x(n);
    std::vector<std::uint32_t> bins(256, 0);
    fill_positive(x.data(), n, kBenchmarkSeed);

    // 预热
    histogram256_fp32(bins.data(), x.data(), n);

    for (auto _ : state) {
        histogram256_fp32(bins.data(), x.data(), n);
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }
    do_not_optimize_array(bins.data(), bins.size());

    // ops = n（每个元素一次 bin 递增）
    state.SetItemsProcessed(static_cast<int64_t>(n) * state.iterations());
    state.counters["MOp/s"] = ::benchmark::Counter(
        static_cast<double>(n) * 1e-6,
        ::benchmark::Counter::kIsIterationInvariantRate);
}

// ===== 注册 benchmark =====
// Gather/Scatter: 参数化 size (64K, 1M, 16M) × sparsity (10%, 50%, 100%)
BENCHMARK(gather_random_fp32)
    ->ArgsProduct({
        {64 * 1024, 1024 * 1024, 16 * 1024 * 1024},
        {10, 50, 100}
    })
    ->Unit(::benchmark::kMillisecond);

BENCHMARK(scatter_random_fp32)
    ->ArgsProduct({
        {64 * 1024, 1024 * 1024, 16 * 1024 * 1024},
        {10, 50, 100}
    })
    ->Unit(::benchmark::kMillisecond);

// Histogram: 参数化 size (1M, 4M, 16M)
BENCHMARK(histogram256_uniform_fp32)
    ->Arg(1024 * 1024)
    ->Arg(4 * 1024 * 1024)
    ->Arg(16 * 1024 * 1024)
    ->Unit(::benchmark::kMillisecond);

} // namespace astro::compute::qualification::bench
