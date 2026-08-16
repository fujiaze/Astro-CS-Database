// lib/acr/qualification/benchmarks/benchmark_common.hpp — Phase C Benchmark 公共 helper
//
// 设计（06_QUALIFICATION_BENCHMARK_SPEC.md / 17_CLASSIC_EXPERIMENT_SUITE.md）：
// 1. 公共头不暴露第三方类型（Google Benchmark 仅在 .cpp 内用）
// 2. 固定 seed 0xA57C5AC20260802（确定性、可复现）
// 3. 对数尺寸序列：覆盖 L1/L2/L3 和明显超出 LLC 的主存区间（4KB → 256MB）
// 4. 线程曲线点：1, 2, 4, 25%, 50%, 75%, 95%, 100%（去重）
// 5. 防止编译器消除：volatile sink + asm memory barrier
// 6. ISA 字符串与枚举转换（baseline/SSE/AVX/AVX2/AVX-512）
// 7. 吞吐量（GB/s, MOp/s）与中位/p95 统计
#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

// ===== ACR 公共 API（parallel_for / parallel_reduce）=====
#include "astro/compute/acr.hpp"
#include "astro/compute/topology.hpp"

namespace astro::compute::qualification::bench {

// ===== 固定 seed =====
constexpr std::uint64_t kBenchmarkSeed = 0xA57C5AC20260802ULL;

// ===== 确定性 PRNG：LCG（与 classic_common.hpp 一致常量）=====
class LCG {
public:
    explicit LCG(std::uint64_t seed = kBenchmarkSeed) noexcept : state_(seed) {}
    std::uint64_t next() noexcept {
        state_ = 6364136223846793005ULL * state_ + 1442695040888963407ULL;
        return state_;
    }
    float next_fp32() noexcept {
        // 取高 24 位，[-1, 1)
        std::uint64_t v = next();
        std::uint32_t bits = static_cast<std::uint32_t>(v >> 40) & 0xFFFFFF;
        return (static_cast<float>(bits) / static_cast<float>(0x1000000)) * 2.0f - 1.0f;
    }
    double next_fp64() noexcept {
        std::uint64_t v = next();
        std::uint64_t bits = v >> 11;  // 53 位尾数
        return (static_cast<double>(bits) * (1.0 / 9007199254740992.0)) * 2.0 - 1.0;
    }
    void reseed(std::uint64_t s) noexcept { state_ = s; }
private:
    std::uint64_t state_;
};

// ===== 防止编译器消除 =====
// 强制把变量写入外部可见位置，防止优化器删除"未使用"的计算。
// 注意：仅支持 8 字节以内的标量（足够 FP32/FP64/int 等吞吐基准）。
template<class T>
inline void do_not_optimize(T const& v) noexcept {
    static volatile std::uint64_t sink_storage = 0;
    std::uint64_t bits = 0;
    static_assert(sizeof(T) <= sizeof(std::uint64_t), "do_not_optimize: T too large");
    std::memcpy(&bits, &v, sizeof(T));
    sink_storage = sink_storage ^ bits;
}

// 数组版：异或所有元素到 sink，避免数组被消除（不读回的纯 store 路径已通过 volatile 保证）
template<class T>
inline void do_not_optimize_array(T const* p, std::size_t n) noexcept {
    static volatile std::uint64_t sink_storage = 0;
    std::uint64_t acc = 0x9E3779B97F4A7C15ULL;
    for (std::size_t i = 0; i < n; ++i) {
        std::uint64_t bits = 0;
        std::memcpy(&bits, p + i, sizeof(T));
        acc ^= bits + 0x9E3779B97F4A7C15ULL + (acc << 6) + (acc >> 2);
    }
    sink_storage = acc;
}

// 跨编译器 asm 内存屏障（防止重排）
#define ACR_BENCH_ASM_MEMORY_BARRIER() asm volatile("" ::: "memory")

// ===== ISA 标识（与 isa_kernels.hpp 对齐）=====
enum class IsaLabel : std::uint8_t {
    Baseline = 0,  // 标量无 ISA
    SSE       = 1,
    AVX       = 2,
    AVX2      = 3,  // + FMA
    AVX512    = 4,
};

inline const char* isa_label_str(IsaLabel l) noexcept {
    switch (l) {
        case IsaLabel::Baseline: return "baseline";
        case IsaLabel::SSE:      return "sse";
        case IsaLabel::AVX:      return "avx";
        case IsaLabel::AVX2:     return "avx2";
        case IsaLabel::AVX512:   return "avx512";
    }
    return "unknown";
}

// 查询当前硬件支持的 ISA 列表（按从高到低排序）
// 不支持的 ISA 不进入 benchmark（避免非法指令）
inline std::vector<IsaLabel> available_isa_labels(const CpuIsaCaps& caps) {
    std::vector<IsaLabel> out;
    out.push_back(IsaLabel::Baseline);
    if (caps.has(IsaLevel::SSE | IsaLevel::SSE2)) out.push_back(IsaLabel::SSE);
    if (caps.has(IsaLevel::AVX)) out.push_back(IsaLabel::AVX);
    if (caps.has(IsaLevel::AVX2 | IsaLevel::FMA)) out.push_back(IsaLabel::AVX2);
    // AVX-512 子集要求 F/CD/BW/DQ/VL（与 isa_kernels.hpp 一致）
    if (caps.has(IsaLevel::AVX512F | IsaLevel::AVX512CD | IsaLevel::AVX512BW |
                 IsaLevel::AVX512DQ | IsaLevel::AVX512VL)) {
        out.push_back(IsaLabel::AVX512);
    }
    return out;
}

// ===== 对数尺寸序列 =====
// 默认覆盖 4KB → 256MB（按 FP32 元素数 = 字节数 / 4）
// 用于 STREAM 风格 benchmark：L1(32K-64K) / L2(256K-1M) / L3(4M-16M) / MainMem(64M+)
inline std::vector<std::size_t> log2_size_sequence_bytes(std::size_t min_bytes = 4 * 1024,
                                                          std::size_t max_bytes = 256 * 1024 * 1024,
                                                          std::size_t elems_per_byte = 0) {
    // elems_per_byte 为 0 时按字节，否则按元素数（字节数 = 元素数 * sizeof(elem)）
    // 这里返回字节数；调用方按需转为元素数
    std::vector<std::size_t> sizes;
    std::size_t s = min_bytes;
    while (s <= max_bytes) {
        sizes.push_back(s);
        s <<= 1;  // ×2 对数序列
    }
    if (sizes.empty() || sizes.back() != max_bytes) {
        sizes.push_back(max_bytes);  // 包含上限点
    }
    return sizes;
}

// 元素数对数序列（按 sizeof(T) 转换）
template<class T>
inline std::vector<std::size_t> log2_size_sequence_elems(std::size_t min_bytes = 4 * 1024,
                                                         std::size_t max_bytes = 256 * 1024 * 1024) {
    auto bytes = log2_size_sequence_bytes(min_bytes, max_bytes);
    std::vector<std::size_t> elems;
    elems.reserve(bytes.size());
    for (auto b : bytes) {
        elems.push_back(b / sizeof(T));
    }
    return elems;
}

// 归约尺寸序列（按 17 §7：2^10 起逐级递增至资源预算上限，按 2 倍步进）
inline std::vector<std::size_t> reduction_size_sequence(std::size_t max_elems = 1u << 26) {
    std::vector<std::size_t> sizes;
    std::size_t s = 1u << 10;  // 1024
    while (s <= max_elems) {
        sizes.push_back(s);
        s <<= 1;
    }
    return sizes;
}

// ===== 线程曲线点=====
// 返回线程数列表：1, 2, 4, 约25%, 50%, 75%, 95%, 100%（去重）
inline std::vector<std::uint32_t> thread_curve_points(std::uint32_t hw_threads) {
    if (hw_threads == 0) hw_threads = static_cast<std::uint32_t>(std::thread::hardware_concurrency());
    if (hw_threads == 0) hw_threads = 4;
    std::vector<std::uint32_t> pts;
    auto add = [&](std::uint32_t v) {
        if (v == 0) v = 1;
        if (v > hw_threads) v = hw_threads;
        if (std::find(pts.begin(), pts.end(), v) == pts.end()) pts.push_back(v);
    };
    add(1);
    add(2);
    add(4);
    add(static_cast<std::uint32_t>(hw_threads * 25 / 100));
    add(static_cast<std::uint32_t>(hw_threads * 50 / 100));
    add(static_cast<std::uint32_t>(hw_threads * 75 / 100));
    add(static_cast<std::uint32_t>(hw_threads * 95 / 100));
    add(hw_threads);  // 100%
    std::sort(pts.begin(), pts.end());
    return pts;
}

// ===== 统计：median / p95 / mad / cv =====
struct SampleStats {
    double median{0.0};
    double p95{0.0};
    double mad{0.0};   // 中位绝对偏差
    double cv{0.0};     // 变异系数 = stddev / mean
    std::size_t n{0};
};

inline SampleStats compute_stats(std::vector<double> samples) {
    SampleStats s;
    s.n = samples.size();
    if (samples.empty()) return s;
    std::sort(samples.begin(), samples.end());
    std::size_t n = samples.size();
    s.median = (n % 2 == 1) ? samples[n / 2] : (samples[n / 2 - 1] + samples[n / 2]) * 0.5;
    // p95
    if (n == 1) {
        s.p95 = samples[0];
    } else {
        double idx = 0.95 * static_cast<double>(n - 1);
        std::size_t lo = static_cast<std::size_t>(idx);
        std::size_t hi = (lo + 1 < n) ? lo + 1 : lo;
        double frac = idx - static_cast<double>(lo);
        s.p95 = samples[lo] * (1.0 - frac) + samples[hi] * frac;
    }
    // MAD = median(|x_i - median|)
    std::vector<double> abs_dev;
    abs_dev.reserve(n);
    for (auto v : samples) abs_dev.push_back(std::fabs(v - s.median));
    std::sort(abs_dev.begin(), abs_dev.end());
    s.mad = (n % 2 == 1) ? abs_dev[n / 2] : (abs_dev[n / 2 - 1] + abs_dev[n / 2]) * 0.5;
    // CV = stddev / |mean|
    double mean = std::accumulate(samples.begin(), samples.end(), 0.0) / static_cast<double>(n);
    double sq_sum = 0.0;
    for (auto v : samples) sq_sum += (v - mean) * (v - mean);
    double stddev = std::sqrt(sq_sum / static_cast<double>(n > 1 ? n - 1 : 1));
    s.cv = (std::fabs(mean) > 1e-30) ? stddev / std::fabs(mean) : 0.0;
    return s;
}

// ===== 吞吐量计算 =====
// GB/s = bytes_total / time_seconds / 1e9
inline double gbps(double bytes_total, double ns) noexcept {
    if (ns <= 0.0) return 0.0;
    return bytes_total / ns;  // bytes/ns == GB/s（因为 1e9 bytes / 1e9 ns = 1 GB/s）
}

// MOp/s = ops_total / time_seconds / 1e6 = ops * 1e3 / ns
inline double mops(double ops_total, double ns) noexcept {
    if (ns <= 0.0) return 0.0;
    return ops_total * 1e3 / ns;  // ops/ns * 1e3 = ops/(us) = ops * 1e6/s 不对
    // 修正：ops/ns = ops * 1e9/s = 1e3 MOp/s
}

// ===== 输入数据生成（确定性，构造不计时）=====
template<class T>
inline void fill_uniform(T* dst, std::size_t n, std::uint64_t seed) {
    LCG rng(seed);
    for (std::size_t i = 0; i < n; ++i) dst[i] = static_cast<T>(rng.next_fp64());
}

template<class T>
inline void fill_positive(T* dst, std::size_t n, std::uint64_t seed) {
    LCG rng(seed);
    for (std::size_t i = 0; i < n; ++i) {
        double v = rng.next_fp64();  // [-1, 1)
        dst[i] = static_cast<T>(std::fabs(v) + 1e-3);  // (0, 1]
    }
}

template<class T>
inline void fill_alternating(T* dst, std::size_t n, std::uint64_t seed) {
    // 正负交替（绝对值 0.5-1.0）
    LCG rng(seed);
    for (std::size_t i = 0; i < n; ++i) {
        double v = rng.next_fp64();
        double sign = (i % 2 == 0) ? 1.0 : -1.0;
        dst[i] = static_cast<T>(sign * (0.5 + 0.5 * std::fabs(v)));
    }
}

template<class T>
inline void fill_dynamic_range(T* dst, std::size_t n, std::uint64_t seed) {
    // 动态范围（1e-3 到 1e3，对数分布）
    LCG rng(seed);
    for (std::size_t i = 0; i < n; ++i) {
        double v = rng.next_fp64();  // [0, 1)
        double mag = std::pow(10.0, -3.0 + 6.0 * v);  // 1e-3 ~ 1e3
        double sign = (rng.next() & 1) ? 1.0 : -1.0;
        dst[i] = static_cast<T>(sign * mag);
    }
}

// ===== 通用计时器（不依赖 Google Benchmark 时的简化路径）=====
struct Timer {
    using Clock = std::chrono::steady_clock;
    Clock::time_point t0;
    void start() { t0 = Clock::now(); }
    std::uint64_t elapsed_ns() const {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t0).count());
    }
};

// ===== 多线程范围执行（基于 parallel_for，按线程数 hint）=====
// 注意：parallel_for 内部用 oneTBB arena；ACR 公共 API 没有直接暴露线程数控制，
// 这里通过 ExecutionHints.grainsize 调节切分粒度。
// 线程数控制仅用于 benchmark 采样，正式运行不限制。
struct ThreadHint {
    std::uint32_t threads{0};     // 0 = 默认
    std::uint32_t grainsize{0};   // 0 = 自动
};

inline ExecutionHints make_hints(ThreadHint th) {
    ExecutionHints h;
    h.grainsize = th.grainsize;
    return h;
}

// ===== 内存层级识别（基于 size 与已知 cache 边界）=====
// 返回 "L1" / "L2" / "L3" / "MainMem" 标签
// 注意：阈值是经验值，实际 cache 大小由 hwloc 提供（此处用通用阈值）
inline const char* classify_memory_level(std::size_t bytes,
                                          std::size_t l1 = 32 * 1024,
                                          std::size_t l2 = 256 * 1024,
                                          std::size_t l3 = 8 * 1024 * 1024) noexcept {
    if (bytes <= l1) return "L1";
    if (bytes <= l2) return "L2";
    if (bytes <= l3) return "L3";
    return "MainMem";
}

// ===== 日志辅助 =====
inline void log_benchmark(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    std::fputc('\n', stderr);
    va_end(ap);
}

} // namespace astro::compute::qualification::bench
