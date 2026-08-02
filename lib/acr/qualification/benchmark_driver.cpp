// lib/acr/qualification/benchmark_driver.cpp — 微基准框架实现
// Phase E：CPU AXPY/Triad/Copy 微基准；GPU benchmark 占位（ACR_BUILD_CUDA=OFF 时不执行）。
#include "benchmark_driver.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iomanip>
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
        case KernelId::Copy:        return "Copy";
        case KernelId::Triad:       return "Triad";
        case KernelId::AXPY:        return "AXPY";
        case KernelId::Dot:         return "Dot";
        case KernelId::Convolution2D: return "Convolution2D";
        case KernelId::Gemm:        return "Gemm";
        default:                    return "Custom";
    }
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
            cfg.problem_sizes = { 1u << 14, 1u << 18, 1u << 22 }; // 16K, 256K, 4M
            cfg.warmup_rounds = 3;
            cfg.measure_rounds = 10;
            cfg.collect_resident = true;
            break;
    }
    return cfg;
}

// ===== BenchmarkDriver =====
BenchmarkDriver::BenchmarkDriver() = default;
BenchmarkDriver::~BenchmarkDriver() = default;

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

RawBenchmarkSample BenchmarkDriver::measure_once(std::uint32_t kernel_id,
                                                  const std::string& backend,
                                                  std::size_t problem_size,
                                                  std::size_t bytes_per_elem,
                                                  bool measure_resident) {
    RawBenchmarkSample s;
    if (backend == "cpu") {
        std::uint64_t k = 0;
        switch (static_cast<KernelId>(kernel_id)) {
            case KernelId::AXPY:  k = run_cpu_axpy(problem_size);  break;
            case KernelId::Triad: k = run_cpu_triad(problem_size); break;
            case KernelId::Copy:  k = run_cpu_copy(problem_size);  break;
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
        // GPU benchmark 占位：ACR_BUILD_CUDA=OFF 时不执行；ON 时由 Phase F/H 集成
        // 这里返回零样本，profile_generator 会跳过该 backend
        s.kernel_ns = 0;
        s.transfer_ns = 0;
        s.total_ns = 0;
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
    // 标定 kernel 列表：Copy, AXPY, Triad（FP32）
    struct Spec { std::uint32_t id; std::size_t bpe; };
    const Spec specs[] = {
        { static_cast<std::uint32_t>(KernelId::Copy),  sizeof(float) },
        { static_cast<std::uint32_t>(KernelId::AXPY),  sizeof(float) },
        { static_cast<std::uint32_t>(KernelId::Triad), sizeof(float) },
    };
    // backend 列表：始终测 CPU；GPU 测量仅在 enable_gpu 且 ACR_BUILD_CUDA 时（此处占位）
    std::vector<std::string> backends;
    backends.emplace_back("cpu");
    if (cfg_.enable_gpu) {
        // GPU benchmark 待 CUDA 集成完善后接入；当前仍占位输出零样本
        backends.emplace_back("cuda:0");
    }

    for (const auto& spec : specs) {
        for (std::size_t sz : cfg_.problem_sizes) {
            for (const auto& backend : backends) {
                KernelBenchmarkResult r;
                r.kernel_id = spec.id;
                r.kernel_name = kernel_name(spec.id);
                r.backend = backend;
                r.precision = "fp32";
                r.problem_size = sz;
                r.bytes_per_element = spec.bpe;

                // 预热（不记录）
                for (std::uint32_t w = 0; w < cfg_.warmup_rounds; ++w) {
                    (void)measure_once(spec.id, backend, sz, spec.bpe, false);
                }
                // 测量
                for (std::uint32_t m = 0; m < cfg_.measure_rounds; ++m) {
                    auto s = measure_once(spec.id, backend, sz, spec.bpe,
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

} // namespace astro::compute::qualification
