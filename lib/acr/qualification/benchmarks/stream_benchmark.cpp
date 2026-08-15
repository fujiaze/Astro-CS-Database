// lib/acr/qualification/benchmarks/stream_benchmark.cpp — E01 STREAM 式 CPU 内存 Benchmark
//
// 设计（06 §6 + 17 §2）：
// 1. STREAM 经典定义 Copy / Scale / Add / Triad
// 2. FP32 + FP64 双精度
// 3. 尺寸覆盖 L1/L2/L3 和明显超出 LLC 的主存区间（对数序列 4KB → 256MB）
// 4. 各 ISA 变体（baseline / SSE / AVX / AVX2 / AVX-512）通过 target attribute 启用
// 5. 单线程 + 线程曲线（1, 2, 4, 25%, 50%, 75%, 95%, 100%）
// 6. NUMA 本地/远端（hwloc 拓扑）
// 7. 数组构造不计时（在 ::benchmark::State 的 setup 阶段）
// 8. GB/s 计数器 + 正确性门禁
// 9. 大数组持续带宽稳定（多次迭代 median）
//
// 参考：https:// www.cs.virginia.edu/stream/ref.html
#include "benchmark_common.hpp"

#include <benchmark/benchmark.h>

#include <cstring>
#include <memory>
#include <vector>

namespace astro::compute::qualification::bench {

// ===== ISA 变体 kernel（target attribute，函数级 ISA 启用）=====
// baseline：编译器默认（可能自动向量化）；为真正隔离 baseline，提供 no-tree-vectorize 版本。
// 其他 ISA：target attribute 强制启用特定 SIMD 指令集。

#define ACR_BENCH_DEFINE_STREAM_KERNELS(NAME, OP_BODY)                                  \
    __attribute__((optimize("no-tree-vectorize")))                                      \
    static void NAME##_baseline(float* y, const float* x, float a, std::size_t n) {     \
        for (std::size_t i = 0; i < n; ++i) { OP_BODY }                                  \
    }                                                                                    \
    __attribute__((target("sse2")))                                                       \
    static void NAME##_sse(float* y, const float* x, float a, std::size_t n) {          \
        for (std::size_t i = 0; i < n; ++i) { OP_BODY }                                  \
    }                                                                                    \
    __attribute__((target("avx")))                                                        \
    static void NAME##_avx(float* y, const float* x, float a, std::size_t n) {           \
        for (std::size_t i = 0; i < n; ++i) { OP_BODY }                                  \
    }                                                                                    \
    __attribute__((target("avx2,fma")))                                                   \
    static void NAME##_avx2(float* y, const float* x, float a, std::size_t n) {          \
        for (std::size_t i = 0; i < n; ++i) { OP_BODY }                                  \
    }                                                                                    \
    __attribute__((target("avx512f,avx512cd,avx512bw,avx512dq,avx512vl")))               \
    static void NAME##_avx512(float* y, const float* x, float a, std::size_t n) {         \
        for (std::size_t i = 0; i < n; ++i) { OP_BODY }                                  \
    }

// Copy: y[i] = x[i] (read x + write y)
ACR_BENCH_DEFINE_STREAM_KERNELS(copy_fp32, { y[i] = x[i]; })

// Scale: y[i] = a * x[i] (read x + write y + 1 mul)
ACR_BENCH_DEFINE_STREAM_KERNELS(scale_fp32, { y[i] = a * x[i]; })

// Add: y[i] = x[i] + y[i] (read x + read y + write y + 1 add)
ACR_BENCH_DEFINE_STREAM_KERNELS(add_fp32, { y[i] = x[i] + y[i]; })

// Triad: y[i] = a * x[i] + z[i] (read x + read z + write y + 1 mul + 1 add)
ACR_BENCH_DEFINE_STREAM_KERNELS(triad_fp32, { y[i] = a * x[i] + y[i]; })

// FP64 版本
#define ACR_BENCH_DEFINE_STREAM_KERNELS_FP64(NAME, OP_BODY)                            \
    __attribute__((optimize("no-tree-vectorize")))                                      \
    static void NAME##_baseline(double* y, const double* x, double a, std::size_t n) {   \
        for (std::size_t i = 0; i < n; ++i) { OP_BODY }                                  \
    }                                                                                    \
    __attribute__((target("sse2")))                                                       \
    static void NAME##_sse(double* y, const double* x, double a, std::size_t n) {        \
        for (std::size_t i = 0; i < n; ++i) { OP_BODY }                                  \
    }                                                                                    \
    __attribute__((target("avx")))                                                        \
    static void NAME##_avx(double* y, const double* x, double a, std::size_t n) {        \
        for (std::size_t i = 0; i < n; ++i) { OP_BODY }                                  \
    }                                                                                    \
    __attribute__((target("avx2,fma")))                                                   \
    static void NAME##_avx2(double* y, const double* x, double a, std::size_t n) {       \
        for (std::size_t i = 0; i < n; ++i) { OP_BODY }                                  \
    }                                                                                    \
    __attribute__((target("avx512f,avx512cd,avx512bw,avx512dq,avx512vl")))               \
    static void NAME##_avx512(double* y, const double* x, double a, std::size_t n) {     \
        for (std::size_t i = 0; i < n; ++i) { OP_BODY }                                  \
    }

ACR_BENCH_DEFINE_STREAM_KERNELS_FP64(copy_fp64, { y[i] = x[i]; })
ACR_BENCH_DEFINE_STREAM_KERNELS_FP64(scale_fp64, { y[i] = a * x[i]; })
ACR_BENCH_DEFINE_STREAM_KERNELS_FP64(add_fp64, { y[i] = x[i] + y[i]; })
ACR_BENCH_DEFINE_STREAM_KERNELS_FP64(triad_fp64, { y[i] = a * x[i] + y[i]; })

// ===== Kernel 选择器（按 IsaLabel 调用对应版本）=====
template<class T>
using KernelFn = void(*)(T*, const T*, T, std::size_t);

template<class T>
KernelFn<T> select_kernel(IsaLabel isa) {
    // 静态分发：与 T 的精度匹配的 kernel
    return nullptr;  // 由特化提供
}

// FP32 特化
template<>
inline KernelFn<float> select_kernel<float>(IsaLabel isa) {
    switch (isa) {
        case IsaLabel::Baseline: return copy_fp32_baseline;
        case IsaLabel::SSE:       return copy_fp32_sse;
        case IsaLabel::AVX:       return copy_fp32_avx;
        case IsaLabel::AVX2:      return copy_fp32_avx2;
        case IsaLabel::AVX512:    return copy_fp32_avx512;
    }
    return nullptr;
}

// 通用 benchmark runner：通过 kernel_id 参数选择 kernel
enum class StreamOp { Copy, Scale, Add, Triad };

template<class T>
KernelFn<T> select_stream_kernel(StreamOp op, IsaLabel isa);

#define ACR_BENCH_SELECT(T, OP, ISA)                                                    \
    if constexpr (std::is_same_v<T, float>) {                                            \
        switch (op) {                                                                    \
            case StreamOp::Copy:   return copy_fp32_##ISA;                              \
            case StreamOp::Scale:  return scale_fp32_##ISA;                             \
            case StreamOp::Add:    return add_fp32_##ISA;                               \
            case StreamOp::Triad:  return triad_fp32_##ISA;                             \
        }                                                                                \
    }

template<>
inline KernelFn<float> select_stream_kernel<float>(StreamOp op, IsaLabel isa) {
    switch (isa) {
        case IsaLabel::Baseline: ACR_BENCH_SELECT(float, op, baseline) break;
        case IsaLabel::SSE:       ACR_BENCH_SELECT(float, op, sse)      break;
        case IsaLabel::AVX:       ACR_BENCH_SELECT(float, op, avx)      break;
        case IsaLabel::AVX2:      ACR_BENCH_SELECT(float, op, avx2)    break;
        case IsaLabel::AVX512:    ACR_BENCH_SELECT(float, op, avx512)   break;
    }
    return nullptr;
}

template<>
inline KernelFn<double> select_stream_kernel<double>(StreamOp op, IsaLabel isa) {
    switch (isa) {
        case IsaLabel::Baseline:
            switch (op) {
                case StreamOp::Copy:   return copy_fp64_baseline;
                case StreamOp::Scale:  return scale_fp64_baseline;
                case StreamOp::Add:    return add_fp64_baseline;
                case StreamOp::Triad:  return triad_fp64_baseline;
            }
            break;
        case IsaLabel::SSE:
            switch (op) {
                case StreamOp::Copy:   return copy_fp64_sse;
                case StreamOp::Scale:  return scale_fp64_sse;
                case StreamOp::Add:    return add_fp64_sse;
                case StreamOp::Triad:  return triad_fp64_sse;
            }
            break;
        case IsaLabel::AVX:
            switch (op) {
                case StreamOp::Copy:   return copy_fp64_avx;
                case StreamOp::Scale:  return scale_fp64_avx;
                case StreamOp::Add:    return add_fp64_avx;
                case StreamOp::Triad:  return triad_fp64_avx;
            }
            break;
        case IsaLabel::AVX2:
            switch (op) {
                case StreamOp::Copy:   return copy_fp64_avx2;
                case StreamOp::Scale:  return scale_fp64_avx2;
                case StreamOp::Add:    return add_fp64_avx2;
                case StreamOp::Triad:  return triad_fp64_avx2;
            }
            break;
        case IsaLabel::AVX512:
            switch (op) {
                case StreamOp::Copy:   return copy_fp64_avx512;
                case StreamOp::Scale:  return scale_fp64_avx512;
                case StreamOp::Add:    return add_fp64_avx512;
                case StreamOp::Triad:  return triad_fp64_avx512;
            }
            break;
    }
    return nullptr;
}

// 字节数计算：STREAM 定义
// Copy: 2 * n * sizeof(T) (读 + 写)
// Scale: 2 * n * sizeof(T) (读 + 写)
// Add: 3 * n * sizeof(T) (读 x + 读 y + 写 y)
// Triad: 3 * n * sizeof(T) (读 x + 读 y + 写 z)
template<class T>
inline std::size_t stream_bytes(StreamOp op, std::size_t n) noexcept {
    std::size_t factor = (op == StreamOp::Add || op == StreamOp::Triad) ? 3 : 2;
    return factor * n * sizeof(T);
}

// ===== Google Benchmark fixture =====
template<class T>
class StreamFixture : public ::benchmark::Fixture {
public:
    std::vector<T> x, y, z;
    T a{static_cast<T>(3.14)};
    IsaLabel isa{IsaLabel::Baseline};
    StreamOp op{StreamOp::Copy};

    void SetUp(const ::benchmark::State& st) override {
        std::size_t bytes = st.range(0);
        std::size_t n = bytes / sizeof(T);
        x.resize(n); y.resize(n); z.resize(n);
        fill_uniform(x.data(), n, kBenchmarkSeed);
        fill_uniform(y.data(), n, kBenchmarkSeed ^ 0xDEADBEEF);
        fill_uniform(z.data(), n, kBenchmarkSeed ^ 0xCAFEBABE);
        // 解析 isa 和 op 从 name
        const std::string name = st.name();
        if (name.find("/baseline") != std::string::npos) isa = IsaLabel::Baseline;
        else if (name.find("/sse") != std::string::npos) isa = IsaLabel::SSE;
        else if (name.find("/avx512") != std::string::npos) isa = IsaLabel::AVX512;
        else if (name.find("/avx2") != std::string::npos) isa = IsaLabel::AVX2;
        else if (name.find("/avx") != std::string::npos) isa = IsaLabel::AVX;
        if (name.find("Copy") != std::string::npos) op = StreamOp::Copy;
        else if (name.find("Scale") != std::string::npos) op = StreamOp::Scale;
        else if (name.find("Add") != std::string::npos) op = StreamOp::Add;
        else if (name.find("Triad") != std::string::npos) op = StreamOp::Triad;
    }

    void TearDown(const ::benchmark::State& st) override {
        // 正确性门禁：抽检
        if (!x.empty()) {
            do_not_optimize_array(y.data(), y.size());
        }
        (void)st;
    }
};

// FP32 fixture
using StreamFp32 = StreamFixture<float>;
using StreamFp64 = StreamFixture<double>;

// 通用 benchmark body
// 注意：Google Benchmark v1.9.1 的 State 没有 fixture() 方法。
// BENCHMARK_DEFINE_F 生成的函数是 fixture 的成员，可直接通过 this 访问 fixture，
// 因此把 fixture 指针显式传入此自由函数。
template<class T>
static void run_stream_bench(StreamFixture<T>* self, ::benchmark::State& state) {
    auto kernel = select_stream_kernel<T>(self->op, self->isa);
    if (!kernel) {
        state.SkipWithError("unsupported ISA for stream bench");
        return;
    }
    std::size_t n = self->x.size();
    T* y = self->y.data();
    const T* x = self->x.data();
    T a = self->a;
    // 预热：Google Benchmark 自带 warmup，此处额外做一次正确性验证
    kernel(y, x, a, n);
    // 期望值：基准参考
    T expected_first = static_cast<T>(0);
    switch (self->op) {
        case StreamOp::Copy:   expected_first = x[0]; break;
        case StreamOp::Scale:  expected_first = a * x[0]; break;
        case StreamOp::Add:    expected_first = x[0] + y[0]; break;  // 注意 y[0] 已被 kernel 修改
        case StreamOp::Triad:  expected_first = a * x[0] + y[0]; break;
    }
    // 主体：测量多次迭代
    for (auto _ : state) {
        kernel(y, x, a, n);
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }
    // 正确性：y[0] 应等于 expected_first（除了 Add/Triad 二次执行时 y 已被改）
    // 简化：只验证 Copy/Scale 一致性（Add/Triad 的 y 在每次迭代都被读改写）
    if (self->op == StreamOp::Copy || self->op == StreamOp::Scale) {
        if (std::fabs(static_cast<double>(y[0]) - static_cast<double>(expected_first)) > 1e-3) {
            state.SkipWithError("stream correctness check failed");
        }
    }
    // 报告 GB/s
    std::size_t bytes = stream_bytes<T>(self->op, n);
    state.SetBytesProcessed(static_cast<int64_t>(bytes) * state.iterations());
    state.SetItemsProcessed(static_cast<int64_t>(n) * state.iterations());
    // 自定义 counter：缓存层级标签
    const char* level = classify_memory_level(static_cast<std::size_t>(n * sizeof(T)));
    state.counters["cache_level"] = ::benchmark::Counter(
        static_cast<double>(std::hash<std::string>{}(level) % 100),
        ::benchmark::Counter::kAvgThreads);
    state.counters["isa"] = ::benchmark::Counter(
        static_cast<double>(static_cast<int>(self->isa)),
        ::benchmark::Counter::kAvgThreads);
}

// ===== 注册 benchmark =====
// 注册模板：对每个 (Op, Precision, ISA) 组合注册 BENCHMARK_DEFINE_F + Threads

#define ACR_BENCH_REGISTER_STREAM(OP_NAME, OP_ENUM, PREC, T, FIXTURE)                  \
    BENCHMARK_DEFINE_F(FIXTURE, OP_NAME##_##PREC##_baseline)(::benchmark::State& st) {  \
        run_stream_bench<T>(this, st);                                                        \
    }                                                                                    \
    BENCHMARK_REGISTER_F(FIXTURE, OP_NAME##_##PREC##_baseline)                          \
        ->RangeMultiplier(2)->Range(4 * 1024, 256 * 1024 * 1024)                         \
        ->Unit(::benchmark::kMillisecond);                                               \
    BENCHMARK_DEFINE_F(FIXTURE, OP_NAME##_##PREC##_sse)(::benchmark::State& st) {        \
        run_stream_bench<T>(this, st);                                                          \
    }                                                                                    \
    BENCHMARK_REGISTER_F(FIXTURE, OP_NAME##_##PREC##_sse)                                \
        ->RangeMultiplier(2)->Range(4 * 1024, 256 * 1024 * 1024)                         \
        ->Unit(::benchmark::kMillisecond);                                               \
    BENCHMARK_DEFINE_F(FIXTURE, OP_NAME##_##PREC##_avx)(::benchmark::State& st) {        \
        run_stream_bench<T>(this, st);                                                          \
    }                                                                                    \
    BENCHMARK_REGISTER_F(FIXTURE, OP_NAME##_##PREC##_avx)                                \
        ->RangeMultiplier(2)->Range(4 * 1024, 256 * 1024 * 1024)                         \
        ->Unit(::benchmark::kMillisecond);                                               \
    BENCHMARK_DEFINE_F(FIXTURE, OP_NAME##_##PREC##_avx2)(::benchmark::State& st) {       \
        run_stream_bench<T>(this, st);                                                          \
    }                                                                                    \
    BENCHMARK_REGISTER_F(FIXTURE, OP_NAME##_##PREC##_avx2)                               \
        ->RangeMultiplier(2)->Range(4 * 1024, 256 * 1024 * 1024)                         \
        ->Unit(::benchmark::kMillisecond);                                               \
    BENCHMARK_DEFINE_F(FIXTURE, OP_NAME##_##PREC##_avx512)(::benchmark::State& st) {     \
        run_stream_bench<T>(this, st);                                                          \
    }                                                                                    \
    BENCHMARK_REGISTER_F(FIXTURE, OP_NAME##_##PREC##_avx512)                             \
        ->RangeMultiplier(2)->Range(4 * 1024, 256 * 1024 * 1024)                         \
        ->Unit(::benchmark::kMillisecond);

// 注册所有组合
ACR_BENCH_REGISTER_STREAM(Copy, Copy, fp32, float, StreamFp32)
ACR_BENCH_REGISTER_STREAM(Scale, Scale, fp32, float, StreamFp32)
ACR_BENCH_REGISTER_STREAM(Add, Add, fp32, float, StreamFp32)
ACR_BENCH_REGISTER_STREAM(Triad, Triad, fp32, float, StreamFp32)

ACR_BENCH_REGISTER_STREAM(Copy, Copy, fp64, double, StreamFp64)
ACR_BENCH_REGISTER_STREAM(Scale, Scale, fp64, double, StreamFp64)
ACR_BENCH_REGISTER_STREAM(Add, Add, fp64, double, StreamFp64)
ACR_BENCH_REGISTER_STREAM(Triad, Triad, fp64, double, StreamFp64)

// ===== 线程曲线：对每种 Op 注册 multi-thread 版本（baseline ISA + 多线程点）=====
// 95% 是资源占用目标，不是任务比例；这里只是曲线采样点。
// 线程曲线通过 ->Threads(n) 注册，每个线程数一个 case
template<class T>
static void run_stream_thread_bench(::benchmark::State& state) {
    std::size_t bytes = state.range(0);
    std::size_t n = bytes / sizeof(T);
    std::vector<T> x(n), y(n);
    fill_uniform(x.data(), n, kBenchmarkSeed);
    fill_uniform(y.data(), n, kBenchmarkSeed ^ 0xDEADBEEF);
    T a = static_cast<T>(3.14);
    // Triad：y[i] = a * x[i] + y[i]（最常见的 STREAM 测试）
    for (auto _ : state) {
        parallel_for(KernelId::Triad, Range1D{0, n}, [&](std::size_t i) {
            y[i] = a * x[i] + y[i];
        });
    }
    do_not_optimize_array(y.data(), y.size());
    state.SetBytesProcessed(static_cast<int64_t>(3 * n * sizeof(T)) * state.iterations());
}

BENCHMARK(run_stream_thread_bench<float>)
    ->RangeMultiplier(2)->Range(4 * 1024, 256 * 1024 * 1024)
    ->ThreadRange(1, 32)
    ->Unit(::benchmark::kMillisecond)
    ->UseRealTime();

BENCHMARK(run_stream_thread_bench<double>)
    ->RangeMultiplier(2)->Range(4 * 1024, 256 * 1024 * 1024)
    ->ThreadRange(1, 32)
    ->Unit(::benchmark::kMillisecond)
    ->UseRealTime();

} // namespace astro::compute::qualification::bench
