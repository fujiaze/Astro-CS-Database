// lib/acr/qualification/benchmark_driver.cpp — 微基准框架实现
// Phase E：CPU AXPY/Triad/Copy 微基准；GPU benchmark 占位（ACR_BUILD_CUDA=OFF 时不执行）。
#include "benchmark_driver.hpp"

#include "../backends/cuda/bridge/cuda_bridge_api.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <atomic>
#include <numeric>
#include <thread>
#include <sstream>
#include <vector>

#include "astro/compute/acr.hpp"

namespace astro::compute::qualification {

namespace {

// 高分辨率时钟别名
using SteadyClock = std::chrono::steady_clock;
using Nanos = std::chrono::nanoseconds;

template<class T>
std::uint64_t elapsed_nanos(T start, T end) {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<Nanos>(end - start).count());
}

// 简单 LCG（线性同余生成器），确定性可复现
// 用固定 seed 0xA57C5AC20260802，不依赖 std::random_device
struct LCG {
    std::uint64_t state;
    explicit LCG(std::uint64_t seed) : state(seed) {}
    std::uint64_t next() {
        // Numerical Recipes 常数
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state;
    }
    float next_float() {
        // 取高 24 位作为尾数，构造 [0,1) float
        std::uint64_t v = next();
        std::uint32_t bits = static_cast<std::uint32_t>(v >> 40) & 0xFFFFFF;
        return static_cast<float>(bits) / static_cast<float>(0x1000000);
    }
    double next_double() {
        std::uint64_t v = next();
        std::uint64_t bits = v >> 11;  // 53 位尾数
        return static_cast<double>(bits) / static_cast<double>(1ULL << 53);
    }
};

// KernelId 名称表
const char* kernel_name(std::uint32_t id) {
    switch (static_cast<KernelId>(id)) {
        case KernelId::Copy:          return "Copy";
        case KernelId::Triad:         return "Triad";
        case KernelId::AXPY:          return "AXPY";
        case KernelId::Dot:           return "Dot";
        case KernelId::Transpose:     return "Transpose";
        case KernelId::Convolution2D: return "Convolution2D";
        case KernelId::Histogram256:  return "Histogram256";
        case KernelId::Scan:          return "Scan";
        case KernelId::Gather:        return "Gather";
        case KernelId::Scatter:       return "Scatter";
        case KernelId::Mandelbrot:    return "Mandelbrot";
        case KernelId::Gemm:          return "Gemm";
        case KernelId::Fft:           return "Fft";
        case KernelId::Custom:        return "Custom";
    }
    return "Custom";
}

} // anonymous namespace

// ===== profile_kind_str / parse_profile_kind =====
const char* profile_kind_str(ProfileKind k) noexcept {
    switch (k) {
        case ProfileKind::Quick:    return "quick";
        case ProfileKind::Standard: return "standard";
        case ProfileKind::Full:     return "full";
    }
    return "unknown";
}

bool parse_profile_kind(const std::string& s, ProfileKind& out) noexcept {
    if (s == "quick")    { out = ProfileKind::Quick;    return true; }
    if (s == "standard") { out = ProfileKind::Standard; return true; }
    if (s == "full")     { out = ProfileKind::Full;     return true; }
    return false;
}

const char* profile_state_str(ProfileState s) noexcept {
    switch (s) {
        case ProfileState::Missing: return "missing";
        case ProfileState::Stale:   return "stale";
        case ProfileState::Corrupt: return "corrupt";
        case ProfileState::Valid:   return "valid";
    }
    return "unknown";
}

// ===== make_default_config =====
BenchmarkConfig make_default_config(ProfileKind kind, bool enable_gpu) noexcept {
    BenchmarkConfig cfg;
    cfg.profile_kind = kind;
    cfg.enable_gpu = enable_gpu;
    switch (kind) {
        case ProfileKind::Quick:
            cfg.problem_sizes = { 1u << 16 };                    // 64K
            cfg.warmup_rounds = 0;
            cfg.measure_rounds = 1;
            cfg.collect_resident = false;
            break;
        case ProfileKind::Standard:
            cfg.problem_sizes = { 1u << 16, 1u << 20 };          // 64K, 1M
            cfg.warmup_rounds = 1;
            cfg.measure_rounds = 3;
            cfg.collect_resident = false;
            break;
        case ProfileKind::Full:
            // 25 §3.2：覆盖 L1/L2、L3、主存区间（64K/1M/4M 元素）
            cfg.problem_sizes = { 1u << 16, 1u << 20, 1u << 22 }; // 64K, 1M, 4M
            cfg.warmup_rounds = 3;
            cfg.measure_rounds = 10;
            cfg.collect_resident = true;
            break;
    }
    return cfg;
}

// ===== BenchmarkDriver =====
BenchmarkDriver::BenchmarkDriver() = default;
BenchmarkDriver::~BenchmarkDriver() {
    if (gpu_handle_) {
        auto& api = cuda::bridge::api();
        if (api.executor_destroy) api.executor_destroy(gpu_handle_);
        gpu_handle_ = nullptr;
    }
}

void BenchmarkDriver::configure(const BenchmarkConfig& cfg) {
    cfg_ = cfg;
    log_.clear();
}

void BenchmarkDriver::log(const std::string& line) {
    log_.append(line);
    log_.push_back('\n');
}

void BenchmarkDriver::fill_input(float* dst, std::size_t n, std::uint64_t seed) {
    LCG rng(seed);
    for (std::size_t i = 0; i < n; ++i) {
        dst[i] = rng.next_float() * 2.0f - 1.0f;  // [-1, 1)
    }
}

void BenchmarkDriver::fill_input(double* dst, std::size_t n, std::uint64_t seed) {
    LCG rng(seed);
    for (std::size_t i = 0; i < n; ++i) {
        dst[i] = rng.next_double() * 2.0 - 1.0;
    }
}

std::uint64_t BenchmarkDriver::run_cpu_axpy(std::size_t n) {
    std::vector<float> x(n), y(n);
    fill_input(x.data(), n, BENCHMARK_FIXED_SEED);
    fill_input(y.data(), n, BENCHMARK_FIXED_SEED ^ 0x9E3779B97F4A7C15ULL);
    const float a = 2.0f;
    auto t0 = SteadyClock::now();
    astro::compute::parallel_for(
        KernelId::AXPY, Range1D{0, n},
        [&](std::size_t i) { y[i] = a * x[i] + y[i]; });
    auto t1 = SteadyClock::now();
    // 防止优化器消除：用 y 的校验和作为 sink
    volatile float sink = 0.0f;
    for (std::size_t i = 0; i < n; i += (n > 16 ? n / 16 : 1)) sink = sink + y[i];
    (void)sink;
    return elapsed_nanos(t0, t1);
}

std::uint64_t BenchmarkDriver::run_cpu_triad(std::size_t n) {
    std::vector<float> x(n), y(n), z(n);
    fill_input(x.data(), n, BENCHMARK_FIXED_SEED);
    fill_input(y.data(), n, BENCHMARK_FIXED_SEED ^ 0x9E3779B97F4A7C15ULL);
    const float a = 3.14f;
    auto t0 = SteadyClock::now();
    astro::compute::parallel_for(
        KernelId::Triad, Range1D{0, n},
        [&](std::size_t i) { z[i] = a * x[i] + y[i]; });
    auto t1 = SteadyClock::now();
    volatile float sink = 0.0f;
    for (std::size_t i = 0; i < n; i += (n > 16 ? n / 16 : 1)) sink = sink + z[i];
    (void)sink;
    return elapsed_nanos(t0, t1);
}

std::uint64_t BenchmarkDriver::run_cpu_copy(std::size_t n) {
    std::vector<float> x(n), y(n);
    fill_input(x.data(), n, BENCHMARK_FIXED_SEED);
    auto t0 = SteadyClock::now();
    astro::compute::parallel_for(
        KernelId::Copy, Range1D{0, n},
        [&](std::size_t i) { y[i] = x[i]; });
    auto t1 = SteadyClock::now();
    volatile float sink = 0.0f;
    for (std::size_t i = 0; i < n; i += (n > 16 ? n / 16 : 1)) sink = sink + y[i];
    (void)sink;
    return elapsed_nanos(t0, t1);
}

// ===== Commit E：补充 kernel 实现 =====
// 这些 kernel 供 profile_generator 生成多维能力曲线（reduction/convolution/irregular/branch/transfer/overhead）。
// 实现保持简洁：用 parallel_for 调度，返回 kernel 执行时间（纳秒）。

std::uint64_t BenchmarkDriver::run_cpu_dot(std::size_t n,
                                            const std::string& variant) {
    std::vector<float> x(n), y(n);
    fill_input(x.data(), n, BENCHMARK_FIXED_SEED);
    fill_input(y.data(), n, BENCHMARK_FIXED_SEED ^ 0x9E3779B97F4A7C15ULL);
    // 25 §1.1：每 chunk 独立 FP64 partial + 唯一槽位 + 串行 merge
    // （禁止多线程并发写共享 float dot）
    const std::size_t chunk = 4096;
    const std::size_t max_slots = (n + chunk - 1) / chunk;
    std::vector<double> partials(max_slots, 0.0);
    std::atomic<std::size_t> slots{0};
    auto t0 = SteadyClock::now();
    astro::compute::parallel_chunks(
        KernelId::Dot, Range1D{0, n}, chunk,
        [&](std::size_t b, std::size_t e) {
            double local = 0.0;
            if (variant == "sum") {
                for (std::size_t i = b; i < e; ++i) {
                    local += static_cast<double>(x[i]);
                }
            } else {
                for (std::size_t i = b; i < e; ++i) {
                    local += static_cast<double>(x[i]) * static_cast<double>(y[i]);
                }
            }
            const std::size_t slot =
                slots.fetch_add(1, std::memory_order_relaxed);
            if (slot < partials.size()) partials[slot] = local;
        });
    auto t1 = SteadyClock::now();
    double dot = 0.0;
    for (double v : partials) dot += v;
    // 25 §1.1：与串行 FP64 参考比较（计时外校验）
    {
        double ref = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            ref += (variant == "sum")
                ? static_cast<double>(x[i])
                : static_cast<double>(x[i]) * static_cast<double>(y[i]);
        }
        const double rel = std::fabs(ref) > 1e-30
            ? std::fabs(dot - ref) / std::fabs(ref) : std::fabs(dot - ref);
        if (rel > 1e-6) {
            std::fprintf(stderr,
                "[benchmark] DOT correctness FAILED: dot=%.9f ref=%.9f rel=%.3g\n",
                dot, ref, rel);
        }
    }
    volatile double sink = dot;
    (void)sink;
    return elapsed_nanos(t0, t1);
}

std::uint64_t BenchmarkDriver::run_cpu_conv2d(std::size_t n) {
    // 25 §1.3/§3.3：n = 总输出元素数（与 GPU conv3x3 语义一致），
    // 图像宽 w = ceil(sqrt(n))，只测 [0, n) 个输出像素
    std::size_t w = static_cast<std::size_t>(std::sqrt(static_cast<double>(n)));
    if (w * w < n) ++w;
    const std::size_t total = w * w;
    const std::size_t ks = 3;
    const std::size_t half = ks / 2;
    std::vector<float> img(total), out(n), kernel(ks * ks);
    fill_input(img.data(), total, BENCHMARK_FIXED_SEED);
    fill_input(kernel.data(), ks * ks, BENCHMARK_FIXED_SEED ^ 0xCAFEBABE);
    auto t0 = SteadyClock::now();
    astro::compute::parallel_for(
        KernelId::Convolution2D, Range1D{0, n},
        [&](std::size_t idx) {
            std::size_t r = idx / w, c = idx % w;
            float sum = 0.0f;
            for (std::size_t ki = 0; ki < ks; ++ki) {
                for (std::size_t kj = 0; kj < ks; ++kj) {
                    std::size_t ri = (r + ki >= half) ? r + ki - half : 0;
                    std::size_t ci = (c + kj >= half) ? c + kj - half : 0;
                    if (ri < w && ci < w) {  // clamp 边界
                        sum += img[ri * w + ci] * kernel[ki * ks + kj];
                    }
                }
            }
            out[idx] = sum;
        });
    auto t1 = SteadyClock::now();
    volatile float sink = out[0];
    (void)sink;
    return elapsed_nanos(t0, t1);
}

std::uint64_t BenchmarkDriver::run_cpu_histogram(std::size_t n,
                                                  const std::string& variant) {
    std::vector<float> data(n);
    fill_input(data.data(), n, BENCHMARK_FIXED_SEED);
    const std::size_t chunk = 4096;
    const std::size_t max_slots = (n + chunk - 1) / chunk;
    auto bin_of = [&](std::size_t i) -> std::uint32_t {
        std::uint32_t bin = static_cast<std::uint32_t>((data[i] + 1.0f) * 128.0f);
        if (bin >= 256) bin = 255;
        return bin;
    };
    auto t0 = SteadyClock::now();
    std::vector<std::uint32_t> bins(256, 0);
    if (variant == "hist_atomic") {
        // 共享 atomic bins（可定义、无竞争）
        std::vector<std::atomic<std::uint32_t>> abins(256);
        for (auto& b : abins) b.store(0, std::memory_order_relaxed);
        astro::compute::parallel_for(
            KernelId::Histogram256, Range1D{0, n},
            [&](std::size_t i) {
                abins[bin_of(i)].fetch_add(1, std::memory_order_relaxed);
            });
        for (int k = 0; k < 256; ++k) {
            bins[k] = abins[k].load(std::memory_order_relaxed);
        }
    } else {
        // hist_tls：每 chunk 局部 bins + 唯一槽位 + 串行 merge（无竞争）
        std::vector<std::uint32_t> partials(max_slots * 256, 0);
        std::atomic<std::size_t> slots{0};
        astro::compute::parallel_chunks(
            KernelId::Histogram256, Range1D{0, n}, chunk,
            [&](std::size_t b, std::size_t e) {
                std::uint32_t local[256] = {0};
                for (std::size_t i = b; i < e; ++i) ++local[bin_of(i)];
                const std::size_t slot =
                    slots.fetch_add(1, std::memory_order_relaxed);
                if (slot < max_slots) {
                    for (int k = 0; k < 256; ++k) {
                        partials[slot * 256 + k] = local[k];
                    }
                }
            });
        for (std::size_t s = 0; s < max_slots; ++s) {
            for (int k = 0; k < 256; ++k) {
                bins[k] += partials[s * 256 + k];
            }
        }
    }
    auto t1 = SteadyClock::now();
    // 25 §1.2：确定性整数参考校验（总和必须 == n）
    {
        std::uint64_t total = 0;
        for (int k = 0; k < 256; ++k) total += bins[k];
        if (total != n) {
            std::fprintf(stderr,
                "[benchmark] HISTOGRAM correctness FAILED: total=%llu expected=%zu\n",
                static_cast<unsigned long long>(total), n);
        }
    }
    volatile std::uint32_t sink = bins[0];
    (void)sink;
    return elapsed_nanos(t0, t1);
}

std::uint64_t BenchmarkDriver::run_cpu_gather(std::size_t n) {
    std::vector<float> src(n), dst(n);
    std::vector<std::size_t> indices(n);
    fill_input(src.data(), n, BENCHMARK_FIXED_SEED);
    // 生成确定性随机索引
    LCG rng(BENCHMARK_FIXED_SEED ^ 0x6A741234ULL);
    for (std::size_t i = 0; i < n; ++i) {
        indices[i] = rng.next() % n;
    }
    auto t0 = SteadyClock::now();
    astro::compute::parallel_for(
        KernelId::Gather, Range1D{0, n},
        [&](std::size_t i) { dst[i] = src[indices[i]]; });
    auto t1 = SteadyClock::now();
    volatile float sink = dst[0];
    (void)sink;
    return elapsed_nanos(t0, t1);
}

std::uint64_t BenchmarkDriver::run_cpu_scatter(std::size_t n,
                                                const std::string& variant) {
    std::vector<float> src(n);
    fill_input(src.data(), n, BENCHMARK_FIXED_SEED);
    LCG rng(BENCHMARK_FIXED_SEED ^ 0x5C415678ULL);
    auto t0 = SteadyClock::now();
    if (variant == "scatter_atomic") {
        // atomic scatter：索引可重复，atomic fetch_add 保证正确性
        std::vector<std::size_t> indices(n);
        for (std::size_t i = 0; i < n; ++i) indices[i] = rng.next() % n;
        std::vector<std::atomic<std::uint32_t>> dst(n);
        for (auto& d : dst) d.store(0, std::memory_order_relaxed);
        astro::compute::parallel_for(
            KernelId::Scatter, Range1D{0, n},
            [&](std::size_t i) {
                dst[indices[i]].fetch_add(1, std::memory_order_relaxed);
            });
        volatile std::uint32_t sink = dst[0].load(std::memory_order_relaxed);
        (void)sink;
        // 校验：总写入次数 == n
        std::uint64_t total = 0;
        for (auto& d : dst) total += d.load(std::memory_order_relaxed);
        if (total != n) {
            std::fprintf(stderr,
                "[benchmark] SCATTER_ATOMIC correctness FAILED: total=%llu expected=%zu\n",
                static_cast<unsigned long long>(total), n);
        }
    } else {
        // scatter_perm：无冲突确定性置换（每个目标恰好写一次，无数据竞争）
        std::vector<std::size_t> indices(n);
        std::iota(indices.begin(), indices.end(), std::size_t{0});
        for (std::size_t i = n; i > 1; --i) {  // Fisher-Yates（确定性 LCG）
            std::size_t j = static_cast<std::size_t>(rng.next() % i);
            std::swap(indices[i - 1], indices[j]);
        }
        std::vector<float> dst(n, 0.0f);
        astro::compute::parallel_for(
            KernelId::Scatter, Range1D{0, n},
            [&](std::size_t i) { dst[indices[i]] = src[i]; });
        volatile float sink = dst[0];
        (void)sink;
        // 校验：置换 scatter 每个目标恰好写一次（dst[pos] == src[inv[pos]]）
        {
            std::vector<std::size_t> inv(n);
            for (std::size_t i = 0; i < n; ++i) inv[indices[i]] = i;
            bool ok = true;
            for (std::size_t pos = 0; pos < n; ++pos) {
                if (dst[pos] != src[inv[pos]]) { ok = false; break; }
            }
            if (!ok) {
                std::fprintf(stderr,
                    "[benchmark] SCATTER_PERM correctness FAILED\n");
            }
        }
    }
    auto t1 = SteadyClock::now();
    return elapsed_nanos(t0, t1);
}

std::uint64_t BenchmarkDriver::run_cpu_mandelbrot(std::size_t n) {
    // 25 §1.3：n = 总网格元素数（与 problem_size 语义一致）
    std::size_t w = static_cast<std::size_t>(std::sqrt(static_cast<double>(n)));
    if (w * w < n) ++w;
    std::vector<std::uint32_t> result(n);
    const std::uint32_t max_iter = 256;
    auto t0 = SteadyClock::now();
    astro::compute::parallel_for(
        KernelId::Mandelbrot, Range1D{0, n},
        [&](std::size_t idx) {
            std::size_t r = idx / w, c = idx % w;
            double cre = -2.0 + 3.0 * static_cast<double>(c) / static_cast<double>(w);
            double cim = -1.5 + 3.0 * static_cast<double>(r) / static_cast<double>(w);
            double zr = 0.0, zi = 0.0;
            std::uint32_t iter = 0;
            while (iter < max_iter && zr * zr + zi * zi < 4.0) {
                double new_zr = zr * zr - zi * zi + cre;
                zi = 2.0 * zr * zi + cim;
                zr = new_zr;
                ++iter;
            }
            result[idx] = iter;
        });
    auto t1 = SteadyClock::now();
    volatile std::uint32_t sink = result[0];
    (void)sink;
    return elapsed_nanos(t0, t1);
}

std::uint64_t BenchmarkDriver::run_cpu_transfer(std::size_t n) {
    // memcpy 传输带宽（CPU 内存层级）
    std::vector<float> src(n), dst(n);
    fill_input(src.data(), n, BENCHMARK_FIXED_SEED);
    auto t0 = SteadyClock::now();
    std::memcpy(dst.data(), src.data(), n * sizeof(float));
    auto t1 = SteadyClock::now();
    volatile float sink = dst[0];
    (void)sink;
    return elapsed_nanos(t0, t1);
}

std::uint64_t BenchmarkDriver::run_cpu_overhead_submit(std::size_t n) {
    // parallel_for 提交开销：最小工作负载（单元素），测量调度固定开销
    auto t0 = SteadyClock::now();
    astro::compute::parallel_for(
        KernelId::Custom, Range1D{0, 1},
        [n](std::size_t) { (void)n; });
    auto t1 = SteadyClock::now();
    return elapsed_nanos(t0, t1);
}

RawBenchmarkSample BenchmarkDriver::measure_once(std::uint32_t kernel_id,
                                                  const std::string& variant,
                                                  const std::string& backend,
                                                  std::size_t problem_size,
                                                  std::size_t bytes_per_elem,
                                                  bool measure_resident) {
    RawBenchmarkSample s;
    if (backend == "cpu") {
        std::uint64_t k = 0;
        switch (static_cast<KernelId>(kernel_id)) {
            case KernelId::AXPY:         k = run_cpu_axpy(problem_size);           break;
            case KernelId::Triad:        k = run_cpu_triad(problem_size);          break;
            case KernelId::Copy:         k = run_cpu_copy(problem_size);          break;
            case KernelId::Dot:          k = run_cpu_dot(problem_size, variant);  break;
            case KernelId::Convolution2D: k = run_cpu_conv2d(problem_size);        break;
            case KernelId::Histogram256: k = run_cpu_histogram(problem_size, variant); break;
            case KernelId::Gather:       k = run_cpu_gather(problem_size);         break;
            case KernelId::Scatter:      k = run_cpu_scatter(problem_size, variant); break;
            case KernelId::Mandelbrot:   k = run_cpu_mandelbrot(problem_size);     break;
            case KernelId::Transpose:   k = run_cpu_transfer(problem_size);      break;
            case KernelId::Custom:      k = run_cpu_overhead_submit(problem_size); break;
            default:
                // 未实现的 kernel 跳过（median=0，路由器视为不支持）
                return s;
        }
        s.kernel_ns = k;
        s.transfer_ns = 0;            // CPU backend 无 H2D/D2H
        s.total_ns = k;
        // resident 仅 Full：CPU 上 resident == kernel（数据已在缓存/内存）
        s.resident_ns = measure_resident ? k : 0;
    } else if (backend.rfind("cuda", 0) == 0) {
        // 24 §1：GPU 真实微基准（真实 CUDA kernel 计时，非占位）
        bool supported = false;
        std::uint64_t gpu_ns = run_gpu_kernel(kernel_id, problem_size, supported);
        if (!supported || gpu_ns == 0) {
            // 桥接不可用或不支持该维度：如实跳过（不产生零吞吐伪样本）
            return s;
        }
        // 同步单 stream：桥接 elapsed 含 H2D + launch + D2H；未分离传输，
        // kernel_ns 记 total（传输分离留待异步多 stream 支持）
        s.kernel_ns = gpu_ns;
        s.transfer_ns = 0;
        s.total_ns = gpu_ns;
        s.resident_ns = 0;
    }

    // 计算吞吐量 GB/s = bytes_total / time_seconds / 1e9
    // bytes_total = problem_size * bytes_per_elem * (read + write 因子)
    // 简化：Copy/AXPY 读写 2 个数组，Triad 读写 3 个数组
    std::size_t array_factor = 2;
    if (static_cast<KernelId>(kernel_id) == KernelId::Triad) array_factor = 3;
    std::size_t bytes_total = problem_size * bytes_per_elem * array_factor;
    if (s.total_ns > 0) {
        double seconds = static_cast<double>(s.total_ns) * 1e-9;
        s.throughput_gbps = (seconds > 0.0)
            ? (static_cast<double>(bytes_total) / 1e9 / seconds)
            : 0.0;
    }
    return s;
}

std::vector<KernelBenchmarkResult> BenchmarkDriver::run() {
    // 空载提示（不自动判断/干预）
    std::fprintf(stdout,
        "请确保系统空载以获得准确结果（关闭其他高负载进程，未启用自动检测）\n");
    std::fflush(stdout);
    log("=== ACR benchmark driver started ===");
    log(std::string("profile_kind=") + profile_kind_str(cfg_.profile_kind) +
        " warmup=" + std::to_string(cfg_.warmup_rounds) +
        " measure=" + std::to_string(cfg_.measure_rounds) +
        " resident=" + (cfg_.collect_resident ? "on" : "off") +
        " gpu=" + (cfg_.enable_gpu ? "on" : "off"));

    std::vector<KernelBenchmarkResult> results;
    // Commit E：标定 kernel 列表覆盖多维能力曲线族
    // memory(Copy/Triad) + arithmetic(AXPY) + reduction(Dot) +
    // convolution(Convolution2D) + irregular(Histogram/Gather/Scatter) +
    // branch(Mandelbrot) + transfer(Transpose→memcpy) + overhead(Custom→submit)
    // 25 §1：variant 区分同一 kernel 的不同实现/分布；
    // problem_size 统一为总工作项数（二维任务见 workload.width/height）
    struct Spec { std::uint32_t id; std::size_t bpe; const char* variant; };
    const Spec specs[] = {
        { static_cast<std::uint32_t>(KernelId::Copy),           sizeof(float), "" },
        { static_cast<std::uint32_t>(KernelId::AXPY),           sizeof(float), "" },
        { static_cast<std::uint32_t>(KernelId::Triad),          sizeof(float), "" },
        { static_cast<std::uint32_t>(KernelId::Dot),            sizeof(float), "dot" },
        { static_cast<std::uint32_t>(KernelId::Dot),            sizeof(float), "sum" },
        { static_cast<std::uint32_t>(KernelId::Convolution2D),  sizeof(float), "conv3x3" },
        { static_cast<std::uint32_t>(KernelId::Histogram256),   sizeof(float), "hist_tls" },
        { static_cast<std::uint32_t>(KernelId::Histogram256),   sizeof(float), "hist_atomic" },
        { static_cast<std::uint32_t>(KernelId::Gather),         sizeof(float), "" },
        { static_cast<std::uint32_t>(KernelId::Scatter),        sizeof(float), "scatter_perm" },
        { static_cast<std::uint32_t>(KernelId::Scatter),        sizeof(float), "scatter_atomic" },
        { static_cast<std::uint32_t>(KernelId::Mandelbrot),     sizeof(float), "" },
        { static_cast<std::uint32_t>(KernelId::Transpose),     sizeof(float), "" }, // transfer
        { static_cast<std::uint32_t>(KernelId::Custom),         sizeof(float), "" }, // overhead_submit
    };
    // backend 列表：始终测 CPU；GPU 测量仅在 enable_gpu 且 ACR_BUILD_CUDA 时（此处占位）
    std::vector<std::string> backends;
    backends.emplace_back("cpu");
    if (cfg_.enable_gpu) {
        // GPU benchmark 待 CUDA 集成完善后接入；当前仍占位输出零样本
        backends.emplace_back("cuda:0");
    }

    for (const auto& spec : specs) {
        if (!cfg_.kernel_ids.empty() &&
            std::find(cfg_.kernel_ids.begin(), cfg_.kernel_ids.end(),
                      spec.id) == cfg_.kernel_ids.end()) {
            continue;  // 定向微基准：跳过未列出的 kernel
        }
        for (std::size_t sz : cfg_.problem_sizes) {
            for (const auto& backend : backends) {
                KernelBenchmarkResult r;
                r.kernel_id = spec.id;
                r.kernel_name = kernel_name(spec.id);
                r.variant = spec.variant;
                r.backend = backend;
                r.precision = "fp32";
                // 24 §1：原始记录区分实现维度
                r.isa = (backend == "cpu") ? detect_best_isa() : "gpu";
                r.threads = (backend == "cpu")
                    ? static_cast<std::uint32_t>(std::thread::hardware_concurrency())
                    : 0;
                r.problem_size = sz;
                r.bytes_per_element = spec.bpe;
                // 25 §1.4：统一工作量描述
                r.workload.logical_items = sz;
                r.workload.precision = "fp32";
                r.workload.residency = (backend == "cpu") ? "host" : "transfer_inclusive";
                if (static_cast<KernelId>(spec.id) == KernelId::Convolution2D) {
                    r.workload.kernel_shape = "3x3";
                    r.workload.operation_count = 9;
                    r.workload.boundary_mode = "clamp";
                    r.workload.input_bytes = sz * spec.bpe * 2;  // 图像 + 核（近似）
                    r.workload.output_bytes = sz * spec.bpe;
                } else {
                    r.workload.kernel_shape = "1d";
                    r.workload.operation_count = 1;
                    r.workload.boundary_mode = "none";
                    const std::size_t array_factor =
                        (static_cast<KernelId>(spec.id) == KernelId::Triad) ? 3 : 2;
                    r.workload.input_bytes = sz * spec.bpe * array_factor;
                    r.workload.output_bytes = sz * spec.bpe;
                }

                // 预热（不记录）
                for (std::uint32_t w = 0; w < cfg_.warmup_rounds; ++w) {
                    (void)measure_once(spec.id, spec.variant, backend,
                                       sz, spec.bpe, false);
                }
                // 测量
                for (std::uint32_t m = 0; m < cfg_.measure_rounds; ++m) {
                    auto s = measure_once(spec.id, spec.variant, backend,
                                          sz, spec.bpe,
                                          cfg_.collect_resident);
                    if (s.total_ns == 0 && m == 0) {
                        // backend 不可用（如 GPU 占位），跳过整个 (kernel,backend,size)
                        r.samples.clear();
                        break;
                    }
                    r.samples.push_back(s);
                }
                if (!r.samples.empty()) {
                    results.push_back(std::move(r));
                }
            }
        }
    }
    log(std::string("=== benchmark complete: ") + std::to_string(results.size()) +
        " result records ===");
    return results;
}

const std::string& BenchmarkDriver::last_log() const noexcept {
    return log_;
}

// ===== 原始记录 JSON 导出（25 §3.2）=====
bool BenchmarkDriver::write_raw_records_json(
    const std::string& path,
    const std::vector<KernelBenchmarkResult>& results) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << "{\"records\":[";
    for (std::size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        if (i > 0) f << ",";
        f << "{\"kernel_id\":" << r.kernel_id;
        f << ",\"kernel_name\":\"" << r.kernel_name << "\"";
        f << ",\"variant\":\"" << r.variant << "\"";
        f << ",\"backend\":\"" << r.backend << "\"";
        f << ",\"precision\":\"" << r.precision << "\"";
        f << ",\"isa\":\"" << r.isa << "\"";
        f << ",\"threads\":" << r.threads;
        f << ",\"problem_size\":" << r.problem_size;
        f << ",\"bytes_per_element\":" << r.bytes_per_element;
        f << ",\"workload\":{"
          << "\"logical_items\":" << r.workload.logical_items
          << ",\"width\":" << r.workload.width
          << ",\"height\":" << r.workload.height
          << ",\"input_bytes\":" << r.workload.input_bytes
          << ",\"output_bytes\":" << r.workload.output_bytes
          << ",\"operation_count\":" << r.workload.operation_count
          << ",\"kernel_shape\":\"" << r.workload.kernel_shape << "\""
          << ",\"precision\":\"" << r.workload.precision << "\""
          << ",\"residency\":\"" << r.workload.residency << "\""
          << ",\"boundary_mode\":\"" << r.workload.boundary_mode << "\"}";
        f << ",\"samples\":[";
        for (std::size_t j = 0; j < r.samples.size(); ++j) {
            const auto& s = r.samples[j];
            if (j > 0) f << ",";
            f << "{\"kernel_ns\":" << s.kernel_ns
              << ",\"transfer_ns\":" << s.transfer_ns
              << ",\"total_ns\":" << s.total_ns
              << ",\"throughput_gbps\":" << s.throughput_gbps << "}";
        }
        f << "]}";
    }
    f << "]}";
    return true;
}

// ===== GPU 真实微基准（24 §1，经桥接 DLL）=====
std::uint64_t BenchmarkDriver::run_gpu_kernel(std::uint32_t kernel_id,
                                               std::size_t n, bool& supported) {
    supported = false;
    cuda::bridge::ensure_bridge_loaded();  // 触发 DLL 加载（幂等）
    auto& api = cuda::bridge::api();
    if (!api.loaded()) return 0;
    if (!gpu_probe_once_) {
        gpu_probe_once_ = true;
        const char* err = nullptr;
        if (api.init(&err) <= 0) return 0;
    }
    if (gpu_handle_ == nullptr) {
        const char* err = nullptr;
        gpu_handle_ = api.executor_create(0, 65536, 256, &err);
        if (gpu_handle_ == nullptr) return 0;
    }

    std::vector<float> x(n, 1.0f);
    std::vector<float> y(n, 2.0f);
    std::uint64_t elapsed = 0;
    const char* err = nullptr;
    int rc = 1;
    switch (static_cast<KernelId>(kernel_id)) {
        case KernelId::AXPY:
        case KernelId::Triad:  // Triad 用 axpy 内核近似（写回同数组）
            rc = api.submit_axpy(gpu_handle_, 0, n, y.data(), x.data(), 2.0f,
                                 &elapsed, &err);
            supported = true;
            break;
        case KernelId::Copy:
            rc = api.submit_copy(gpu_handle_, 0, n, y.data(), x.data(),
                                 &elapsed, &err);
            supported = true;
            break;
        case KernelId::Dot: {
            std::vector<double> partials(256, 0.0);
            rc = api.submit_reduce(gpu_handle_, 0, n, x.data(), partials.data(),
                                   256, 0, &elapsed, &err);
            supported = true;
            break;
        }
        case KernelId::Convolution2D: {
            // 3x3 卷积：构造 w×h 图像（w*h >= n），测量 n 个输出元素
            std::size_t w = static_cast<std::size_t>(
                std::sqrt(static_cast<double>(n)));
            if (w * w < n) ++w;
            std::size_t h = (n + w - 1) / w;
            std::vector<float> img(w * h, 1.0f);
            float k9[9] = {1, 0, -1, 2, 0, -2, 1, 0, -1};
            rc = api.submit_conv3x3(gpu_handle_, 0, n, y.data(), img.data(),
                                    w, h, k9, &elapsed, &err);
            supported = true;
            break;
        }
        default:
            return 0;  // 桥接未提供该维度 kernel → 如实跳过
    }
    return (rc == 0) ? elapsed : 0;
}

// ===== 本机最优 ISA 探测（原始记录维度）=====
std::string BenchmarkDriver::detect_best_isa() {
#if defined(__x86_64__) || defined(_M_X64)
#if defined(__GNUC__) || defined(__clang__)
    if (__builtin_cpu_supports("avx512f")) return "avx512";
    if (__builtin_cpu_supports("avx2")) return "avx2";
    if (__builtin_cpu_supports("avx")) return "avx";
    if (__builtin_cpu_supports("sse4.2")) return "sse4.2";
    if (__builtin_cpu_supports("sse2")) return "sse2";
#endif
#endif
    return "baseline";
}

} // namespace astro::compute::qualification
