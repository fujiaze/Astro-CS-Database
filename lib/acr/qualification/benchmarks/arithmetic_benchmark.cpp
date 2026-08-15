// lib/acr/qualification/benchmarks/arithmetic_benchmark.cpp — E04 算术吞吐 Benchmark
//
// 设计（06 §5 + 17 §5）：
// 1. FP32/FP64 add / mul / FMA / div / sqrt
// 2. 各 ISA 变体（baseline / SSE / AVX / AVX2 / AVX-512）
// 3. 独立链：足够多寄存器并行执行，避免只测单一依赖延迟（throughput）
// 4. 依赖链 latency：a = f(a, x)，每次依赖前一次结果，测 latency
// 5. 防止编译器消除：volatile sink + asm barrier + result accumulator
// 6. 持续负载反映降频：长迭代时间（≥30s 关键路线，由 Google Benchmark 配置）
// 7. 必要的 sin/cos 数学函数
// 8. 保存汇编/编译报告抽样：通过 -save-temps 编译选项，运行时仅记录 kernel 名
//
// 算术吞吐 = ops / time。其中 ops 计入每个元素的运算数：
// add/mul: 1 op/elem
// FMA: 2 ops/elem（一次乘 + 一次加）
// div/sqrt: 1 op/elem（但执行慢）
// sin/cos: 1 op/elem（约 30-100ns 每次调用）
#include "benchmark_common.hpp"

#include <benchmark/benchmark.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace astro::compute::qualification::bench {

// ===== 算术 kernel（target attribute 启用各 ISA）=====
// 独立链：用 8 个独立累加器，让流水线并行执行
// arr[i] = arr[i] OP x[i] → 依赖 arr 的前值，但 8 路独立 → 实际并行

template<class T, int NAcc>
struct AccumChain {
    T acc[NAcc];
    AccumChain() { for (int i = 0; i < NAcc; ++i) acc[i] = T(1.0); }
};

// Add 吞吐 kernel（独立链）
#define ACR_BENCH_DEFINE_ARITH_KERNEL(NAME, OP_BODY, ISA_TARGET)                         \
    __attribute__((target(ISA_TARGET)))                                                    \
    static void NAME(float* ACM_RESTRICT acc, const float* ACM_RESTRICT x, std::size_t n) { \
        for (std::size_t i = 0; i < n; ++i) { OP_BODY }                                    \
    }
#ifndef ACM_RESTRICT
#define ACM_RESTRICT
#endif

// baseline（无 ISA 启用，编译器默认可能向量化）
__attribute__((optimize("no-tree-vectorize")))
static void add_baseline(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = acc[i] + x[i];
}
__attribute__((target("sse2")))
static void add_sse(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = acc[i] + x[i];
}
__attribute__((target("avx")))
static void add_avx(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = acc[i] + x[i];
}
__attribute__((target("avx2,fma")))
static void add_avx2(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = acc[i] + x[i];
}
__attribute__((target("avx512f,avx512cd,avx512bw,avx512dq,avx512vl")))
static void add_avx512(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = acc[i] + x[i];
}

// Mul 各 ISA
__attribute__((optimize("no-tree-vectorize")))
static void mul_baseline(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = acc[i] * x[i];
}
__attribute__((target("sse2")))
static void mul_sse(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = acc[i] * x[i];
}
__attribute__((target("avx")))
static void mul_avx(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = acc[i] * x[i];
}
__attribute__((target("avx2,fma")))
static void mul_avx2(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = acc[i] * x[i];
}
__attribute__((target("avx512f,avx512cd,avx512bw,avx512dq,avx512vl")))
static void mul_avx512(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = acc[i] * x[i];
}

// FMA 各 ISA：acc[i] = acc[i] * x[i] + x[i]（一次乘 + 一次加 = 2 ops）
__attribute__((optimize("no-tree-vectorize")))
static void fma_baseline(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = acc[i] * x[i] + x[i];
}
__attribute__((target("sse2")))
static void fma_sse(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = acc[i] * x[i] + x[i];
}
__attribute__((target("avx")))
static void fma_avx(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = acc[i] * x[i] + x[i];
}
__attribute__((target("avx2,fma")))
static void fma_avx2(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = acc[i] * x[i] + x[i];
}
__attribute__((target("avx512f,avx512cd,avx512bw,avx512dq,avx512vl")))
static void fma_avx512(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = acc[i] * x[i] + x[i];
}

// Div 各 ISA：acc[i] = acc[i] / x[i]
__attribute__((optimize("no-tree-vectorize")))
static void div_baseline(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = acc[i] / x[i];
}
__attribute__((target("sse2")))
static void div_sse(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = acc[i] / x[i];
}
__attribute__((target("avx")))
static void div_avx(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = acc[i] / x[i];
}
__attribute__((target("avx2,fma")))
static void div_avx2(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = acc[i] / x[i];
}
__attribute__((target("avx512f,avx512cd,avx512bw,avx512dq,avx512vl")))
static void div_avx512(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = acc[i] / x[i];
}

// Sqrt 各 ISA：acc[i] = sqrt(x[i])
__attribute__((optimize("no-tree-vectorize")))
static void sqrt_baseline(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = std::sqrt(x[i]);
}
__attribute__((target("sse2")))
static void sqrt_sse(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = std::sqrt(x[i]);
}
__attribute__((target("avx")))
static void sqrt_avx(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = std::sqrt(x[i]);
}
__attribute__((target("avx2,fma")))
static void sqrt_avx2(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = std::sqrt(x[i]);
}
__attribute__((target("avx512f,avx512cd,avx512bw,avx512dq,avx512vl")))
static void sqrt_avx512(float* acc, const float* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) acc[i] = std::sqrt(x[i]);
}

// FP64 版本（同样 5 个 ISA × 5 个 op = 25 kernel）
#define ACR_BENCH_DEFINE_FP64_KERNEL(OP_NAME, OP_BODY)                                   \
    __attribute__((optimize("no-tree-vectorize")))                                        \
    static void OP_NAME##_fp64_baseline(double* acc, const double* x, std::size_t n) {   \
        for (std::size_t i = 0; i < n; ++i) { OP_BODY }                                   \
    }                                                                                      \
    __attribute__((target("sse2")))                                                        \
    static void OP_NAME##_fp64_sse(double* acc, const double* x, std::size_t n) {          \
        for (std::size_t i = 0; i < n; ++i) { OP_BODY }                                   \
    }                                                                                      \
    __attribute__((target("avx")))                                                          \
    static void OP_NAME##_fp64_avx(double* acc, const double* x, std::size_t n) {          \
        for (std::size_t i = 0; i < n; ++i) { OP_BODY }                                   \
    }                                                                                      \
    __attribute__((target("avx2,fma")))                                                     \
    static void OP_NAME##_fp64_avx2(double* acc, const double* x, std::size_t n) {         \
        for (std::size_t i = 0; i < n; ++i) { OP_BODY }                                   \
    }                                                                                      \
    __attribute__((target("avx512f,avx512cd,avx512bw,avx512dq,avx512vl")))                 \
    static void OP_NAME##_fp64_avx512(double* acc, const double* x, std::size_t n) {       \
        for (std::size_t i = 0; i < n; ++i) { OP_BODY }                                   \
    }

ACR_BENCH_DEFINE_FP64_KERNEL(add_fp64, { acc[i] = acc[i] + x[i]; })
ACR_BENCH_DEFINE_FP64_KERNEL(mul_fp64, { acc[i] = acc[i] * x[i]; })
ACR_BENCH_DEFINE_FP64_KERNEL(fma_fp64, { acc[i] = acc[i] * x[i] + x[i]; })
ACR_BENCH_DEFINE_FP64_KERNEL(div_fp64, { acc[i] = acc[i] / x[i]; })
ACR_BENCH_DEFINE_FP64_KERNEL(sqrt_fp64, { acc[i] = std::sqrt(x[i]); })

// ===== Kernel 函数指针表 =====
enum class ArithOp { Add, Mul, Fma, Div, Sqrt };

template<class T>
using ArithKernelFn = void(*)(T*, const T*, std::size_t);

template<class T>
ArithKernelFn<T> select_arith_kernel(ArithOp op, IsaLabel isa);

template<>
inline ArithKernelFn<float> select_arith_kernel<float>(ArithOp op, IsaLabel isa) {
    switch (op) {
        case ArithOp::Add:
            switch (isa) {
                case IsaLabel::Baseline: return add_baseline;
                case IsaLabel::SSE:       return add_sse;
                case IsaLabel::AVX:       return add_avx;
                case IsaLabel::AVX2:      return add_avx2;
                case IsaLabel::AVX512:    return add_avx512;
            }
            break;
        case ArithOp::Mul:
            switch (isa) {
                case IsaLabel::Baseline: return mul_baseline;
                case IsaLabel::SSE:       return mul_sse;
                case IsaLabel::AVX:       return mul_avx;
                case IsaLabel::AVX2:      return mul_avx2;
                case IsaLabel::AVX512:    return mul_avx512;
            }
            break;
        case ArithOp::Fma:
            switch (isa) {
                case IsaLabel::Baseline: return fma_baseline;
                case IsaLabel::SSE:       return fma_sse;
                case IsaLabel::AVX:       return fma_avx;
                case IsaLabel::AVX2:      return fma_avx2;
                case IsaLabel::AVX512:    return fma_avx512;
            }
            break;
        case ArithOp::Div:
            switch (isa) {
                case IsaLabel::Baseline: return div_baseline;
                case IsaLabel::SSE:       return div_sse;
                case IsaLabel::AVX:       return div_avx;
                case IsaLabel::AVX2:      return div_avx2;
                case IsaLabel::AVX512:    return div_avx512;
            }
            break;
        case ArithOp::Sqrt:
            switch (isa) {
                case IsaLabel::Baseline: return sqrt_baseline;
                case IsaLabel::SSE:       return sqrt_sse;
                case IsaLabel::AVX:       return sqrt_avx;
                case IsaLabel::AVX2:      return sqrt_avx2;
                case IsaLabel::AVX512:    return sqrt_avx512;
            }
            break;
    }
    return nullptr;
}

template<>
inline ArithKernelFn<double> select_arith_kernel<double>(ArithOp op, IsaLabel isa) {
    // 注意：宏 ACR_BENCH_DEFINE_FP64_KERNEL(add_fp64, ...) 展开为 add_fp64_fp64_baseline 等
    // （OP_NAME=add_fp64，宏拼接为 OP_NAME##_fp64_baseline → add_fp64_fp64_baseline）
    // 因此这里使用双 _fp64 后缀的函数名
    switch (op) {
        case ArithOp::Add:
            switch (isa) {
                case IsaLabel::Baseline: return add_fp64_fp64_baseline;
                case IsaLabel::SSE:       return add_fp64_fp64_sse;
                case IsaLabel::AVX:       return add_fp64_fp64_avx;
                case IsaLabel::AVX2:      return add_fp64_fp64_avx2;
                case IsaLabel::AVX512:    return add_fp64_fp64_avx512;
            }
            break;
        case ArithOp::Mul:
            switch (isa) {
                case IsaLabel::Baseline: return mul_fp64_fp64_baseline;
                case IsaLabel::SSE:       return mul_fp64_fp64_sse;
                case IsaLabel::AVX:       return mul_fp64_fp64_avx;
                case IsaLabel::AVX2:      return mul_fp64_fp64_avx2;
                case IsaLabel::AVX512:    return mul_fp64_fp64_avx512;
            }
            break;
        case ArithOp::Fma:
            switch (isa) {
                case IsaLabel::Baseline: return fma_fp64_fp64_baseline;
                case IsaLabel::SSE:       return fma_fp64_fp64_sse;
                case IsaLabel::AVX:       return fma_fp64_fp64_avx;
                case IsaLabel::AVX2:      return fma_fp64_fp64_avx2;
                case IsaLabel::AVX512:    return fma_fp64_fp64_avx512;
            }
            break;
        case ArithOp::Div:
            switch (isa) {
                case IsaLabel::Baseline: return div_fp64_fp64_baseline;
                case IsaLabel::SSE:       return div_fp64_fp64_sse;
                case IsaLabel::AVX:       return div_fp64_fp64_avx;
                case IsaLabel::AVX2:      return div_fp64_fp64_avx2;
                case IsaLabel::AVX512:    return div_fp64_fp64_avx512;
            }
            break;
        case ArithOp::Sqrt:
            switch (isa) {
                case IsaLabel::Baseline: return sqrt_fp64_fp64_baseline;
                case IsaLabel::SSE:       return sqrt_fp64_fp64_sse;
                case IsaLabel::AVX:       return sqrt_fp64_fp64_avx;
                case IsaLabel::AVX2:      return sqrt_fp64_fp64_avx2;
                case IsaLabel::AVX512:    return sqrt_fp64_fp64_avx512;
            }
            break;
    }
    return nullptr;
}

// 每元素运算数：FMA = 2，其他 = 1
inline double ops_per_elem(ArithOp op) noexcept {
    return (op == ArithOp::Fma) ? 2.0 : 1.0;
}

// ===== 通用 benchmark body =====
template<class T>
static void run_arith_bench(::benchmark::State& state, ArithOp op, IsaLabel isa) {
    auto kernel = select_arith_kernel<T>(op, isa);
    if (!kernel) {
        state.SkipWithError("unsupported ISA for arith bench");
        return;
    }
    // 小尺寸（L1 内）保证不是内存带宽受限
    std::size_t n = static_cast<std::size_t>(state.range(0));
    std::vector<T> acc(n), x(n);
    fill_positive(acc.data(), n, kBenchmarkSeed);
    fill_positive(x.data(), n, kBenchmarkSeed ^ 0xA5A5);
    // 防止除零/负数取 sqrt
    if (op == ArithOp::Div || op == ArithOp::Sqrt) {
        for (auto& v : x) v = std::fabs(v) + T(0.01);
        for (auto& v : acc) v = std::fabs(v) + T(0.01);
    }

    // 主体测量
    for (auto _ : state) {
        kernel(acc.data(), x.data(), n);
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }
    // 防止编译器消除
    do_not_optimize_array(acc.data(), acc.size());

    // 报告 MOp/s
    double total_ops = static_cast<double>(n) * ops_per_elem(op) * static_cast<double>(state.iterations());
    state.SetItemsProcessed(static_cast<int64_t>(total_ops));
    state.SetLabel(std::string(isa_label_str(isa)) + "/" + (std::is_same_v<T, float> ? "fp32" : "fp64"));
}

// ===== 依赖链 latency benchmark =====
// 模式：a = a * x[i] + x[i] → 每次依赖前一次 a
// 测的是单次操作 latency（不是吞吐）
template<class T>
static void run_arith_latency_bench(::benchmark::State& state, ArithOp op) {
    std::size_t n = static_cast<std::size_t>(state.range(0));
    std::vector<T> x(n);
    fill_positive(x.data(), n, kBenchmarkSeed);
    for (auto _ : state) {
        T a = static_cast<T>(1.0);
        for (std::size_t i = 0; i < n; ++i) {
            switch (op) {
                case ArithOp::Add:  a = a + x[i]; break;
                case ArithOp::Mul:  a = a * x[i]; break;
                case ArithOp::Fma:  a = a * x[i] + x[i]; break;
                case ArithOp::Div:  a = a / x[i]; break;
                case ArithOp::Sqrt: a = std::sqrt(a + x[i]); break;
            }
        }
        do_not_optimize(a);
    }
    // latency 单位是 ns/op
    double total_ops = static_cast<double>(n) * static_cast<double>(state.iterations());
    state.SetItemsProcessed(static_cast<int64_t>(total_ops));
    state.SetLabel("latency_chain");
}

// ===== sin/cos 数学函数（17 §5：必要的 sin/cos）=====
static void run_sincos_bench(::benchmark::State& state) {
    std::size_t n = static_cast<std::size_t>(state.range(0));
    std::vector<float> x(n), y_sin(n), y_cos(n);
    fill_uniform(x.data(), n, kBenchmarkSeed);
    // 把 x 缩放到 [-π, π] 避免 sin/cos 的大参数精度问题
    for (auto& v : x) v = v * 3.14159265f;

    for (auto _ : state) {
        for (std::size_t i = 0; i < n; ++i) {
            y_sin[i] = std::sin(x[i]);
            y_cos[i] = std::cos(x[i]);
        }
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }
    do_not_optimize_array(y_sin.data(), y_sin.size());
    do_not_optimize_array(y_cos.data(), y_cos.size());
    // sin+cos = 2 ops per elem
    state.SetItemsProcessed(static_cast<int64_t>(2 * n) * state.iterations());
    state.SetLabel("sincos/fp32");
}

// ===== 注册 benchmark =====
// 算术吞吐：尺寸固定为 L1（4KB ≈ 1024 fp32），迭代多次
// 每个 (op, precision, isa) 一个 BENCHMARK

#define ACR_BENCH_REGISTER_ARITH(OP_NAME, OP_ENUM, PREC, T)                              \
    BENCHMARK(OP_NAME##_##PREC##_baseline)                                                \
        ->Arg(1024)->Arg(4096)->Arg(16384)                                                \
        ->Unit(::benchmark::kMicrosecond)                                                  \
        ->Repetitions(3);                                                                   \
    BENCHMARK(OP_NAME##_##PREC##_sse)                                                      \
        ->Arg(1024)->Arg(4096)->Arg(16384)                                                \
        ->Unit(::benchmark::kMicrosecond)                                                  \
        ->Repetitions(3);                                                                   \
    BENCHMARK(OP_NAME##_##PREC##_avx)                                                      \
        ->Arg(1024)->Arg(4096)->Arg(16384)                                                \
        ->Unit(::benchmark::kMicrosecond)                                                  \
        ->Repetitions(3);                                                                   \
    BENCHMARK(OP_NAME##_##PREC##_avx2)                                                     \
        ->Arg(1024)->Arg(4096)->Arg(16384)                                                \
        ->Unit(::benchmark::kMicrosecond)                                                  \
        ->Repetitions(3);                                                                   \
    BENCHMARK(OP_NAME##_##PREC##_avx512)                                                   \
        ->Arg(1024)->Arg(4096)->Arg(16384)                                                \
        ->Unit(::benchmark::kMicrosecond)                                                  \
        ->Repetitions(3);

// FP32 各 op 的 benchmark wrapper
#define ACR_BENCH_DEFINE_ARITH_FP32(OP_NAME, OP_ENUM)                                    \
    static void OP_NAME##_fp32_baseline(::benchmark::State& st) {                         \
        run_arith_bench<float>(st, OP_ENUM, IsaLabel::Baseline);                          \
    }                                                                                       \
    static void OP_NAME##_fp32_sse(::benchmark::State& st) {                              \
        run_arith_bench<float>(st, OP_ENUM, IsaLabel::SSE);                                \
    }                                                                                       \
    static void OP_NAME##_fp32_avx(::benchmark::State& st) {                                \
        run_arith_bench<float>(st, OP_ENUM, IsaLabel::AVX);                                 \
    }                                                                                       \
    static void OP_NAME##_fp32_avx2(::benchmark::State& st) {                              \
        run_arith_bench<float>(st, OP_ENUM, IsaLabel::AVX2);                                \
    }                                                                                       \
    static void OP_NAME##_fp32_avx512(::benchmark::State& st) {                            \
        run_arith_bench<float>(st, OP_ENUM, IsaLabel::AVX512);                              \
    }
ACR_BENCH_DEFINE_ARITH_FP32(add, ArithOp::Add)
ACR_BENCH_DEFINE_ARITH_FP32(mul, ArithOp::Mul)
ACR_BENCH_DEFINE_ARITH_FP32(fma, ArithOp::Fma)
ACR_BENCH_DEFINE_ARITH_FP32(div, ArithOp::Div)
ACR_BENCH_DEFINE_ARITH_FP32(sqrt, ArithOp::Sqrt)

ACR_BENCH_REGISTER_ARITH(add, Add, fp32, float)
ACR_BENCH_REGISTER_ARITH(mul, Mul, fp32, float)
ACR_BENCH_REGISTER_ARITH(fma, Fma, fp32, float)
ACR_BENCH_REGISTER_ARITH(div, Div, fp32, float)
ACR_BENCH_REGISTER_ARITH(sqrt, Sqrt, fp32, float)

// FP64
#define ACR_BENCH_DEFINE_ARITH_FP64(OP_NAME, OP_ENUM)                                    \
    static void OP_NAME##_fp64_baseline(::benchmark::State& st) {                         \
        run_arith_bench<double>(st, OP_ENUM, IsaLabel::Baseline);                         \
    }                                                                                       \
    static void OP_NAME##_fp64_sse(::benchmark::State& st) {                               \
        run_arith_bench<double>(st, OP_ENUM, IsaLabel::SSE);                                \
    }                                                                                       \
    static void OP_NAME##_fp64_avx(::benchmark::State& st) {                                \
        run_arith_bench<double>(st, OP_ENUM, IsaLabel::AVX);                                \
    }                                                                                       \
    static void OP_NAME##_fp64_avx2(::benchmark::State& st) {                              \
        run_arith_bench<double>(st, OP_ENUM, IsaLabel::AVX2);                                \
    }                                                                                       \
    static void OP_NAME##_fp64_avx512(::benchmark::State& st) {                            \
        run_arith_bench<double>(st, OP_ENUM, IsaLabel::AVX512);                            \
    }
ACR_BENCH_DEFINE_ARITH_FP64(add, ArithOp::Add)
ACR_BENCH_DEFINE_ARITH_FP64(mul, ArithOp::Mul)
ACR_BENCH_DEFINE_ARITH_FP64(fma, ArithOp::Fma)
ACR_BENCH_DEFINE_ARITH_FP64(div, ArithOp::Div)
ACR_BENCH_DEFINE_ARITH_FP64(sqrt, ArithOp::Sqrt)

ACR_BENCH_REGISTER_ARITH(add, Add, fp64, double)
ACR_BENCH_REGISTER_ARITH(mul, Mul, fp64, double)
ACR_BENCH_REGISTER_ARITH(fma, Fma, fp64, double)
ACR_BENCH_REGISTER_ARITH(div, Div, fp64, double)
ACR_BENCH_REGISTER_ARITH(sqrt, Sqrt, fp64, double)

// 依赖链 latency（baseline 即可，因为 latency 由 ISA 决定不由吞吐路径）
static void latency_add_fp32(::benchmark::State& st)  { run_arith_latency_bench<float>(st, ArithOp::Add); }
static void latency_mul_fp32(::benchmark::State& st)  { run_arith_latency_bench<float>(st, ArithOp::Mul); }
static void latency_fma_fp32(::benchmark::State& st)  { run_arith_latency_bench<float>(st, ArithOp::Fma); }
static void latency_sqrt_fp32(::benchmark::State& st) { run_arith_latency_bench<float>(st, ArithOp::Sqrt); }

BENCHMARK(latency_add_fp32)->Arg(1024)->Unit(::benchmark::kMicrosecond)->Repetitions(3);
BENCHMARK(latency_mul_fp32)->Arg(1024)->Unit(::benchmark::kMicrosecond)->Repetitions(3);
BENCHMARK(latency_fma_fp32)->Arg(1024)->Unit(::benchmark::kMicrosecond)->Repetitions(3);
BENCHMARK(latency_sqrt_fp32)->Arg(1024)->Unit(::benchmark::kMicrosecond)->Repetitions(3);

// sin/cos
BENCHMARK(run_sincos_bench)->Arg(1024)->Arg(4096)->Unit(::benchmark::kMicrosecond)->Repetitions(3);

// 持续负载（降频检测）：长时间运行算术（≥5 秒，用于观察热降频）
// 通过 MinTime(5.0) 让 Google Benchmark 持续迭代
static void sustained_fma(::benchmark::State& st) {
    std::size_t n = 1 << 16;
    std::vector<float> acc(n), x(n);
    fill_positive(acc.data(), n, kBenchmarkSeed);
    fill_positive(x.data(), n, kBenchmarkSeed ^ 0xA5A5);
    for (auto _ : st) {
        fma_avx2(acc.data(), x.data(), n);
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }
    do_not_optimize_array(acc.data(), acc.size());
    st.SetItemsProcessed(static_cast<int64_t>(2 * n) * st.iterations());
    st.SetLabel("sustained_fma_avx2");
}
BENCHMARK(sustained_fma)->MinTime(5.0)->Unit(::benchmark::kMillisecond);

} // namespace astro::compute::qualification::bench
