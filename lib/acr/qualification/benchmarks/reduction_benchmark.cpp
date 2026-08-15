// lib/acr/qualification/benchmarks/reduction_benchmark.cpp — E06 归约画像 Benchmark
//
// 设计（06 §7 + 17 §7）：
// 1. 操作：sum / dot / min / max / sum_of_squares / mean / variance
// 2. 精度：
// - FP32 输入 + FP32 累加
// - FP32 输入 + FP64 累加
// - FP64 输入 + FP64 累加
// 3. 尺寸对数序列 2^10 → 资源预算上限（2^26 = 64M）
// 4. 输入模式：正值 / 正负交替 / 动态范围
// 5. 参考标量 FP64（ground truth）
// 6. 记录：启动成本、元素/秒、有效带宽、误差（max_abs, max_rel）
// 7. 通过 ACR parallel_reduce（用 oneTBB 内部并发）
#include "benchmark_common.hpp"

#include <benchmark/benchmark.h>

#include <cmath>
#include <limits>
#include <vector>

namespace astro::compute::qualification::bench {

// ===== 归约操作类型 =====
enum class ReduceOp { Sum, Dot, Min, Max, SumSq, Mean, Variance };
enum class InputMode { Positive, Alternating, DynamicRange };

inline const char* reduce_op_str(ReduceOp op) noexcept {
    switch (op) {
        case ReduceOp::Sum:        return "sum";
        case ReduceOp::Dot:        return "dot";
        case ReduceOp::Min:        return "min";
        case ReduceOp::Max:        return "max";
        case ReduceOp::SumSq:      return "sum_of_squares";
        case ReduceOp::Mean:       return "mean";
        case ReduceOp::Variance:   return "variance";
    }
    return "unknown";
}

inline const char* input_mode_str(InputMode m) noexcept {
    switch (m) {
        case InputMode::Positive:     return "positive";
        case InputMode::Alternating:  return "alternating";
        case InputMode::DynamicRange: return "dynamic_range";
    }
    return "unknown";
}

// ===== 输入生成 =====
template<class T>
void fill_input_mode(T* dst, std::size_t n, InputMode mode, std::uint64_t seed) {
    switch (mode) {
        case InputMode::Positive:     fill_positive(dst, n, seed); break;
        case InputMode::Alternating: fill_alternating(dst, n, seed); break;
        case InputMode::DynamicRange: fill_dynamic_range(dst, n, seed); break;
    }
}

// ===== Reference 实现（标量 FP64，ground truth）=====
// 用 double 精度作为参考，无论输入精度
template<class T>
double ref_reduce(ReduceOp op, const T* x, const T* y, std::size_t n) {
    double acc = 0.0;
    switch (op) {
        case ReduceOp::Sum:
            for (std::size_t i = 0; i < n; ++i) acc += static_cast<double>(x[i]);
            return acc;
        case ReduceOp::Dot:
            if (y == nullptr) return 0.0;
            for (std::size_t i = 0; i < n; ++i)
                acc += static_cast<double>(x[i]) * static_cast<double>(y[i]);
            return acc;
        case ReduceOp::Min: {
            double m = static_cast<double>(x[0]);
            for (std::size_t i = 1; i < n; ++i) {
                double v = static_cast<double>(x[i]);
                if (v < m) m = v;
            }
            return m;
        }
        case ReduceOp::Max: {
            double m = static_cast<double>(x[0]);
            for (std::size_t i = 1; i < n; ++i) {
                double v = static_cast<double>(x[i]);
                if (v > m) m = v;
            }
            return m;
        }
        case ReduceOp::SumSq:
            for (std::size_t i = 0; i < n; ++i) {
                double v = static_cast<double>(x[i]);
                acc += v * v;
            }
            return acc;
        case ReduceOp::Mean:
            for (std::size_t i = 0; i < n; ++i) acc += static_cast<double>(x[i]);
            return acc / static_cast<double>(n);
        case ReduceOp::Variance: {
            double mean = 0.0;
            for (std::size_t i = 0; i < n; ++i) mean += static_cast<double>(x[i]);
            mean /= static_cast<double>(n);
            double sq = 0.0;
            for (std::size_t i = 0; i < n; ++i) {
                double d = static_cast<double>(x[i]) - mean;
                sq += d * d;
            }
            return sq / static_cast<double>(n);
        }
    }
    return 0.0;
}

// ===== ACR parallel_reduce 实现 =====
// 模板参数 InT 输入精度，AccT 累加精度
template<class InT, class AccT>
AccT acr_reduce(ReduceOp op, const InT* x, const InT* y, std::size_t n) {
    switch (op) {
        case ReduceOp::Sum:
            return parallel_reduce<AccT>(KernelId::Dot, Range1D{0, n}, AccT(0),
                [&](std::size_t i) { return static_cast<AccT>(x[i]); },
                [](AccT a, AccT b) { return a + b; });
        case ReduceOp::Dot:
            return parallel_reduce<AccT>(KernelId::Dot, Range1D{0, n}, AccT(0),
                [&](std::size_t i) { return static_cast<AccT>(x[i]) * static_cast<AccT>(y[i]); },
                [](AccT a, AccT b) { return a + b; });
        case ReduceOp::Min:
            return parallel_reduce<AccT>(KernelId::Dot, Range1D{0, n},
                std::numeric_limits<AccT>::max(),
                [&](std::size_t i) { return static_cast<AccT>(x[i]); },
                [](AccT a, AccT b) { return a < b ? a : b; });
        case ReduceOp::Max:
            return parallel_reduce<AccT>(KernelId::Dot, Range1D{0, n},
                std::numeric_limits<AccT>::lowest(),
                [&](std::size_t i) { return static_cast<AccT>(x[i]); },
                [](AccT a, AccT b) { return a > b ? a : b; });
        case ReduceOp::SumSq:
            return parallel_reduce<AccT>(KernelId::Dot, Range1D{0, n}, AccT(0),
                [&](std::size_t i) { AccT v = static_cast<AccT>(x[i]); return v * v; },
                [](AccT a, AccT b) { return a + b; });
        case ReduceOp::Mean: {
            AccT s = parallel_reduce<AccT>(KernelId::Dot, Range1D{0, n}, AccT(0),
                [&](std::size_t i) { return static_cast<AccT>(x[i]); },
                [](AccT a, AccT b) { return a + b; });
            return s / static_cast<AccT>(n);
        }
        case ReduceOp::Variance: {
            AccT sum = parallel_reduce<AccT>(KernelId::Dot, Range1D{0, n}, AccT(0),
                [&](std::size_t i) { return static_cast<AccT>(x[i]); },
                [](AccT a, AccT b) { return a + b; });
            AccT mean = sum / static_cast<AccT>(n);
            AccT sq = parallel_reduce<AccT>(KernelId::Dot, Range1D{0, n}, AccT(0),
                [&](std::size_t i) { AccT d = static_cast<AccT>(x[i]) - mean; return d * d; },
                [](AccT a, AccT b) { return a + b; });
            return sq / static_cast<AccT>(n);
        }
    }
    return AccT(0);
}

// 误差统计
struct ReduceError {
    double max_abs{0.0};
    double max_rel{0.0};
};

template<class AccT>
ReduceError compute_reduce_error(AccT actual, double ref) {
    ReduceError e;
    e.max_abs = std::fabs(static_cast<double>(actual) - ref);
    e.max_rel = std::fabs(ref) > 1e-30 ? e.max_abs / std::fabs(ref) : 0.0;
    return e;
}

// ===== Benchmark body =====
// 输入：op, input_mode, InT 精度, AccT 精度
template<class InT, class AccT>
static void run_reduce_bench(::benchmark::State& state,
                              ReduceOp op, InputMode mode) {
    std::size_t n = static_cast<std::size_t>(state.range(0));
    std::vector<InT> x(n), y(n);
    fill_input_mode(x.data(), n, mode, kBenchmarkSeed);
    fill_input_mode(y.data(), n, mode, kBenchmarkSeed ^ 0x1234);

    // 参考值
    double ref = ref_reduce<InT>(op, x.data(), y.data(), n);
    // 预热一次（启动成本不计入测量）
    AccT warmup = acr_reduce<InT, AccT>(op, x.data(), y.data(), n);
    do_not_optimize(warmup);

    AccT actual = AccT(0);
    for (auto _ : state) {
        actual = acr_reduce<InT, AccT>(op, x.data(), y.data(), n);
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }
    do_not_optimize(actual);

    // 误差报告
    auto err = compute_reduce_error(actual, ref);
    state.counters["max_abs"] = err.max_abs;
    state.counters["max_rel"] = err.max_rel;

    // 元素/秒
    state.SetItemsProcessed(static_cast<int64_t>(n) * state.iterations());
    // 有效带宽：读 n 个 InT
    std::size_t bytes = n * sizeof(InT);
    if (op == ReduceOp::Dot) bytes *= 2;  // dot 读 2 个数组
    state.SetBytesProcessed(static_cast<int64_t>(bytes) * state.iterations());
    state.SetLabel(std::string(reduce_op_str(op)) + "/" + input_mode_str(mode));
}

// ===== 启动成本 benchmark（小尺寸，测 parallel_reduce 调度开销）=====
static void run_reduce_startup_cost(::benchmark::State& state) {
    std::size_t n = 1 << 4;  // 16 个元素
    std::vector<float> x(n);
    fill_positive(x.data(), n, kBenchmarkSeed);
    for (auto _ : state) {
        float s = parallel_reduce<float>(KernelId::Dot, Range1D{0, n}, 0.0f,
            [&](std::size_t i) { return x[i]; },
            [](float a, float b) { return a + b; });
        do_not_optimize(s);
    }
    state.SetLabel("startup_cost/sum_fp32");
}

// ===== 注册所有组合 =====
// 维度：op(7) × input_mode(3) × precision(3: fp32+fp32, fp32+fp64, fp64+fp64) × size(2^10..2^26)
// 为避免爆炸，size 用 RangeMultiplier(4)->Range(1024, 64M) → 5 个点
// 启动成本单独注册

// 启动成本
BENCHMARK(run_reduce_startup_cost)->Unit(::benchmark::kMicrosecond)->Repetitions(9);

// 注册宏：定义单个函数 + Range 注册
#define ACR_BENCH_DEFINE_REDUCE(OP_NAME, OP_ENUM, MODE_NAME, MODE_ENUM, IN_T, ACC_T, LABEL) \
    static void reduce_##OP_NAME##_##MODE_NAME##_##LABEL(::benchmark::State& st) {          \
        run_reduce_bench<IN_T, ACC_T>(st, OP_ENUM, MODE_ENUM);                              \
    }                                                                                        \
    BENCHMARK(reduce_##OP_NAME##_##MODE_NAME##_##LABEL)                                     \
        ->RangeMultiplier(4)->Range(1 << 10, 1 << 26)                                      \
        ->Unit(::benchmark::kMillisecond)                                                    \
        ->Repetitions(3);

// 7 op × 3 mode × 3 precision = 63 case
// 精度命名：f32f32 = FP32输入+FP32累加，f32f64 = FP32输入+FP64累加，f64f64 = FP64+FP64

#define ACR_BENCH_REGISTER_ALL_MODES(OP_NAME, OP_ENUM)                                   \
    ACR_BENCH_DEFINE_REDUCE(OP_NAME, OP_ENUM, positive, InputMode::Positive, float, float, f32f32)    \
    ACR_BENCH_DEFINE_REDUCE(OP_NAME, OP_ENUM, alternating, InputMode::Alternating, float, float, f32f32) \
    ACR_BENCH_DEFINE_REDUCE(OP_NAME, OP_ENUM, dynamic, InputMode::DynamicRange, float, float, f32f32) \
    ACR_BENCH_DEFINE_REDUCE(OP_NAME, OP_ENUM, positive, InputMode::Positive, float, double, f32f64)    \
    ACR_BENCH_DEFINE_REDUCE(OP_NAME, OP_ENUM, alternating, InputMode::Alternating, float, double, f32f64) \
    ACR_BENCH_DEFINE_REDUCE(OP_NAME, OP_ENUM, dynamic, InputMode::DynamicRange, float, double, f32f64) \
    ACR_BENCH_DEFINE_REDUCE(OP_NAME, OP_ENUM, positive, InputMode::Positive, double, double, f64f64)  \
    ACR_BENCH_DEFINE_REDUCE(OP_NAME, OP_ENUM, alternating, InputMode::Alternating, double, double, f64f64) \
    ACR_BENCH_DEFINE_REDUCE(OP_NAME, OP_ENUM, dynamic, InputMode::DynamicRange, double, double, f64f64)

ACR_BENCH_REGISTER_ALL_MODES(sum, ReduceOp::Sum)
ACR_BENCH_REGISTER_ALL_MODES(dot, ReduceOp::Dot)
ACR_BENCH_REGISTER_ALL_MODES(min, ReduceOp::Min)
ACR_BENCH_REGISTER_ALL_MODES(max, ReduceOp::Max)
ACR_BENCH_REGISTER_ALL_MODES(sumsq, ReduceOp::SumSq)
ACR_BENCH_REGISTER_ALL_MODES(mean, ReduceOp::Mean)
ACR_BENCH_REGISTER_ALL_MODES(variance, ReduceOp::Variance)

} // namespace astro::compute::qualification::bench
