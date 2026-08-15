// lib/acr/qualification/benchmarks/transfer_benchmark.cpp — E02 CPU 内存层级传输带宽 Benchmark
//
// 设计（06 §6 + 17 §3）：
// 1. CPU 内存层级传输带宽测量（L1/L2/L3/MainMem）
// 2. 三种模式：
// - read-only: 累加读 x，不写 → 测纯读带宽
// - write-only: 写常量到 y，不读 → 测纯写带宽
// - copy: y = x → 测读+写带宽
// 3. FP32 主测，FP64 对照（read/write）
// 4. 尺寸覆盖 L1/L2/L3 和明显超出 LLC 的主存区间（对数序列 4KB → 256MB）
// 5. 单线程：隔离内存层级带宽，避免多线程竞争干扰
// 6. std::memcpy 与手动循环两种方式（copy 模式同时注册两种实现以对照）
// 7. 防止编译器消除：volatile sink + asm barrier + do_not_optimize_array
// 8. GB/s 计数器 + 缓存层级标签
//
// 与 stream_benchmark 的区别：
// - stream 测 STREAM 经典 Copy/Scale/Add/Triad（计算+访存混合）
// - transfer 测纯传输带宽（无计算），按 read/write/copy 拆分
// - transfer 同时对比 std::memcpy 与手动循环两种实现
#include "benchmark_common.hpp"

#include <benchmark/benchmark.h>

#include <cstring>
#include <string>
#include <vector>

namespace astro::compute::qualification::bench {

// ===== 传输模式 =====
enum class TransferMode { Read, Write, Copy };

// ===== 手动循环 kernel =====

// Read: 8 路展开累加，打破依赖链以测真实读带宽
// （单路 acc += x[i] 因 FP 依赖链限制 ILP，无法饱和带宽）
template<class T>
static void transfer_read_loop(const T* x, std::size_t n) {
    T a0 = T(0), a1 = T(0), a2 = T(0), a3 = T(0);
    T a4 = T(0), a5 = T(0), a6 = T(0), a7 = T(0);
    std::size_t i = 0;
    constexpr std::size_t kUnroll = 8;
    for (; i + kUnroll <= n; i += kUnroll) {
        a0 += x[i + 0]; a1 += x[i + 1]; a2 += x[i + 2]; a3 += x[i + 3];
        a4 += x[i + 4]; a5 += x[i + 5]; a6 += x[i + 6]; a7 += x[i + 7];
    }
    for (; i < n; ++i) a0 += x[i];
    T acc = (((a0 + a4) + (a1 + a5)) + ((a2 + a6) + (a3 + a7)));
    do_not_optimize(acc);
}

// Write: 写常量到 y（纯写，无读依赖）
template<class T>
static void transfer_write_loop(T* y, T val, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) y[i] = val;
}

// Copy (manual loop): y = x
template<class T>
static void transfer_copy_loop(T* y, const T* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) y[i] = x[i];
}

// Copy (std::memcpy): y = x
template<class T>
static void transfer_copy_memcpy(T* y, const T* x, std::size_t n) {
    std::memcpy(y, x, n * sizeof(T));
}

// 字节数计算：
// Read: n * sizeof(T) (只读)
// Write: n * sizeof(T) (只写)
// Copy: 2 * n * sizeof(T) (读 + 写)
template<class T>
inline std::size_t transfer_bytes(TransferMode mode, std::size_t n) noexcept {
    std::size_t factor = (mode == TransferMode::Copy) ? 2 : 1;
    return factor * n * sizeof(T);
}

// ===== 通用 benchmark body =====
// 通过 mode 和 use_memcpy 参数选择实现
template<class T>
static void run_transfer_bench(::benchmark::State& state,
                                 TransferMode mode, bool use_memcpy) {
    std::size_t bytes = static_cast<std::size_t>(state.range(0));
    std::size_t n = bytes / sizeof(T);
    if (n == 0) {
        state.SkipWithError("transfer: size too small for element type");
        return;
    }

    std::vector<T> x(n), y(n);
    fill_uniform(x.data(), n, kBenchmarkSeed);
    fill_uniform(y.data(), n, kBenchmarkSeed ^ 0xDEADBEEF);
    T val = static_cast<T>(3.14);

    // 预热一次（构造不计时）
    if (mode == TransferMode::Read) {
        transfer_read_loop<T>(x.data(), n);
    } else if (mode == TransferMode::Write) {
        transfer_write_loop<T>(y.data(), val, n);
    } else if (use_memcpy) {
        transfer_copy_memcpy<T>(y.data(), x.data(), n);
    } else {
        transfer_copy_loop<T>(y.data(), x.data(), n);
    }

    // 主体测量
    for (auto _ : state) {
        if (mode == TransferMode::Read) {
            transfer_read_loop<T>(x.data(), n);
        } else if (mode == TransferMode::Write) {
            transfer_write_loop<T>(y.data(), val, n);
        } else if (use_memcpy) {
            transfer_copy_memcpy<T>(y.data(), x.data(), n);
        } else {
            transfer_copy_loop<T>(y.data(), x.data(), n);
        }
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }

    // 防止编译器消除输出数组
    do_not_optimize_array(x.data(), x.size());
    do_not_optimize_array(y.data(), y.size());

    // GB/s 计数器
    std::size_t bytes_per_iter = transfer_bytes<T>(mode, n);
    state.SetBytesProcessed(static_cast<int64_t>(bytes_per_iter) * state.iterations());
    state.SetItemsProcessed(static_cast<int64_t>(n) * state.iterations());

    // 标签：模式 + 精度 + 实现方式 + 缓存层级
    const char* prec = (sizeof(T) == 4) ? "fp32" : "fp64";
    const char* impl = use_memcpy ? "memcpy" : "loop";
    const char* mode_str = (mode == TransferMode::Read) ? "read"
                         : (mode == TransferMode::Write) ? "write" : "copy";
    const char* level = classify_memory_level(n * sizeof(T));
    std::string label = std::string(mode_str) + "/" + prec + "/" + impl + "/" + level;
    state.SetLabel(label);
}

// ===== BENCHMARK wrapper 函数 =====
// 用户指定命名：transfer_read_fp32, transfer_write_fp32, transfer_copy_fp32,
// transfer_read_fp64, transfer_write_fp64
// 额外注册：transfer_copy_fp32_loop / transfer_copy_fp64 / transfer_copy_fp64_loop
// （对照 memcpy 与手动循环两种实现）

static void transfer_read_fp32(::benchmark::State& st) {
    run_transfer_bench<float>(st, TransferMode::Read, false);
}
static void transfer_write_fp32(::benchmark::State& st) {
    run_transfer_bench<float>(st, TransferMode::Write, false);
}
static void transfer_copy_fp32(::benchmark::State& st) {
    run_transfer_bench<float>(st, TransferMode::Copy, true);   // std::memcpy
}
static void transfer_copy_fp32_loop(::benchmark::State& st) {
    run_transfer_bench<float>(st, TransferMode::Copy, false);  // 手动循环
}
static void transfer_read_fp64(::benchmark::State& st) {
    run_transfer_bench<double>(st, TransferMode::Read, false);
}
static void transfer_write_fp64(::benchmark::State& st) {
    run_transfer_bench<double>(st, TransferMode::Write, false);
}
static void transfer_copy_fp64(::benchmark::State& st) {
    run_transfer_bench<double>(st, TransferMode::Copy, true);   // std::memcpy
}
static void transfer_copy_fp64_loop(::benchmark::State& st) {
    run_transfer_bench<double>(st, TransferMode::Copy, false);  // 手动循环
}

// ===== 注册 benchmark =====
// 尺寸：4KB → 256MB 对数序列（与 stream_benchmark 一致）
// 单线程：隔离内存层级带宽
#define ACR_BENCH_REGISTER_TRANSFER(NAME)                                       \
    BENCHMARK(NAME)                                                              \
        ->RangeMultiplier(2)->Range(4 * 1024, 256 * 1024 * 1024)                 \
        ->Unit(::benchmark::kMillisecond)                                        \
        ->Repetitions(3);

ACR_BENCH_REGISTER_TRANSFER(transfer_read_fp32)
ACR_BENCH_REGISTER_TRANSFER(transfer_write_fp32)
ACR_BENCH_REGISTER_TRANSFER(transfer_copy_fp32)
ACR_BENCH_REGISTER_TRANSFER(transfer_copy_fp32_loop)
ACR_BENCH_REGISTER_TRANSFER(transfer_read_fp64)
ACR_BENCH_REGISTER_TRANSFER(transfer_write_fp64)
ACR_BENCH_REGISTER_TRANSFER(transfer_copy_fp64)
ACR_BENCH_REGISTER_TRANSFER(transfer_copy_fp64_loop)

} // namespace astro::compute::qualification::bench
