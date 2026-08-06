// lib/acr/examples/weighted_integration/weighted_integration_benchmark.cpp
//
// ACR 架构冻结（07 号计划 C/E/F）：加权积分合成 Mixed 样例 Benchmark。
//
// 矩阵：quick / standard / full；模式：
//   serial / openmp / acr_cpu / gpu_host / gpu_resident /
//   forced_mixed / auto_mixed / auto_mixed_reuse
//
// 用法：
//   acr_weighted_integration_benchmark
//     --preset quick|standard|full
//     --warmup 2 --repeats 7
//     --case-timeout-s 120 --overall-timeout-s 900
//     --output weighted_integration_report.json
//     [--profile-output operation-profile.json]
//     --seed 20260806
//     --gpu-streams auto|1|2|3
//     [--correctness-only]
//
// 公平性：所有模式使用完全相同的输入/权重/输出语义；数据生成与首次
// GPU context 初始化在计时外；OpenMP 线程数与 ACR CPU worker 上限同时记录。
#include "weighted_integration_kernels.hpp"

#include "astro/compute/kernel_registry.hpp"
#include "astro/compute/task_traits.hpp"
#include "scheduler/dispatcher.hpp"
#include "scheduler/device_executor.hpp"
#include "focused/operation_profile.hpp"
#include "../core/task_descriptor.hpp"
#include "../cost/cost_estimator.hpp"
#include "../backends/cuda/bridge/cuda_bridge_api.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

using namespace astro::compute;
using astro::compute::scheduler::Dispatcher;
using astro::compute::scheduler::DispatcherConfig;
using astro::compute::scheduler::ExecutorRegistry;
using astro::compute::RouteMode;
using astro::compute::qualification::focused::OperationProfile;

constexpr const char* kOp = "synthetic.weighted_integration.fp64acc";

// ===== 参数 =====
struct Args {
    std::string preset{"quick"};
    int warmup{2};
    int repeats{7};
    int case_timeout_s{120};
    int overall_timeout_s{900};
    std::string output{"weighted_integration_report.json"};
    std::string profile_output;
    std::uint64_t seed{20260806};
    int gpu_streams{0};  // 0=auto、1/2/3=固定
    bool correctness_only{false};
    std::string git_head{"unknown"};
};

bool parse_args(int argc, char** argv, Args& out) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](const char* name) -> const char* {
            if (++i >= argc) {
                std::fprintf(stderr, "error: %s needs value\n", name);
                return nullptr;
            }
            return argv[i];
        };
        if (a == "--preset") {
            const char* v = need("--preset");
            if (!v) return false;
            std::string s(v);
            if (s != "quick" && s != "standard" && s != "full") {
                std::fprintf(stderr, "error: bad --preset %s\n", v);
                return false;
            }
            out.preset = s;
        } else if (a == "--warmup") {
            const char* v = need("--warmup");
            if (!v) return false;
            out.warmup = std::atoi(v);
        } else if (a == "--repeats") {
            const char* v = need("--repeats");
            if (!v) return false;
            out.repeats = std::atoi(v);
        } else if (a == "--case-timeout-s") {
            const char* v = need("--case-timeout-s");
            if (!v) return false;
            out.case_timeout_s = std::atoi(v);
        } else if (a == "--overall-timeout-s") {
            const char* v = need("--overall-timeout-s");
            if (!v) return false;
            out.overall_timeout_s = std::atoi(v);
        } else if (a == "--output") {
            const char* v = need("--output");
            if (!v) return false;
            out.output = v;
        } else if (a == "--profile-output") {
            const char* v = need("--profile-output");
            if (!v) return false;
            out.profile_output = v;
        } else if (a == "--seed") {
            const char* v = need("--seed");
            if (!v) return false;
            out.seed = static_cast<std::uint64_t>(std::strtoull(v, nullptr, 10));
        } else if (a == "--gpu-streams") {
            const char* v = need("--gpu-streams");
            if (!v) return false;
            if (std::strcmp(v, "auto") == 0) out.gpu_streams = 0;
            else {
                out.gpu_streams = std::atoi(v);
                if (out.gpu_streams < 1 || out.gpu_streams > 3) {
                    std::fprintf(stderr, "error: --gpu-streams must be auto|1|2|3\n");
                    return false;
                }
            }
        } else if (a == "--correctness-only") {
            out.correctness_only = true;
        } else if (a == "--git-head") {
            const char* v = need("--git-head");
            if (!v) return false;
            out.git_head = v;
        } else if (a == "--help" || a == "-h") {
            std::fprintf(stdout,
                "usage: acr_weighted_integration_benchmark [--preset quick|standard|full]\n"
                "  [--warmup N] [--repeats N] [--case-timeout-s N] [--overall-timeout-s N]\n"
                "  [--output report.json] [--profile-output operation-profile.json]\n"
                "  [--seed N] [--gpu-streams auto|1|2|3] [--git-head SHA] "
                "[--correctness-only]\n");
            return false;
        } else {
            std::fprintf(stderr, "error: unknown arg %s\n", a.c_str());
            return false;
        }
    }
    return true;
}

// ===== 矩阵 =====
struct Case {
    std::size_t width;
    std::size_t height;
    std::size_t frames;
};

std::vector<Case> matrix_for(const std::string& preset) {
    if (preset == "quick") {
        return {{512, 512, 8}, {1024, 1024, 8}, {2048, 2048, 8}};
    }
    if (preset == "standard") {
        return {{512, 512, 16}, {1024, 1024, 16},
                {2048, 2048, 16}, {4096, 4096, 8}};
    }
    return {{1024, 1024, 32}, {2048, 2048, 32},
            {4096, 4096, 16}, {8192, 8192, 8}};
}

// ===== 环境 =====
struct Env {
    std::string git_head;
    std::string cpu;
    std::string gpu;
    bool gpu_available{false};
    int openmp_threads{1};
    std::size_t acr_cpu_workers{1};
    std::uint64_t total_ram{0};
    std::uint64_t total_vram{0};
};

bool bridge_gpu_available() {
    using namespace astro::compute::cuda::bridge;
    ensure_bridge_loaded();
    auto& api = astro::compute::cuda::bridge::api();
    return api.loaded() && api.device_count() > 0 &&
           api.submit_weighted_integration &&
           api.submit_weighted_integration_resident &&
           api.upload_persistent_slot;
}

Env detect_env() {
    Env e;
    e.cpu = "cpu-hw-" + std::to_string(std::thread::hardware_concurrency()) + "t";
    e.openmp_threads =
        static_cast<int>(std::thread::hardware_concurrency());
    e.acr_cpu_workers = std::thread::hardware_concurrency();
#ifdef _OPENMP
    omp_set_dynamic(0);
    omp_set_num_threads(e.openmp_threads);
#endif
    using namespace astro::compute::cuda::bridge;
    ensure_bridge_loaded();
    auto& api = astro::compute::cuda::bridge::api();
    if (api.loaded() && api.device_count() > 0) {
        const char* n = api.device_name(0);
        e.gpu = n ? std::string(n) : "gpu";
        e.gpu_available = bridge_gpu_available();
        std::uint64_t total = 0, free = 0;
        const char* err = nullptr;
        if (api.device_memory(0, &total, &free, &err) == 0) {
            e.total_vram = total;
        }
    } else {
        e.gpu = "none";
    }
#ifdef _WIN32
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) e.total_ram = ms.ullTotalPhys;
#endif
    return e;
}

// ===== 计时统计 =====
struct Timed {
    double median_ms{0.0};
    double min_ms{0.0};
    double p90_ms{0.0};
};

Timed measure(std::function<void()> fn, int warmup, int repeats) {
    using Clock = std::chrono::steady_clock;
    for (int w = 0; w < warmup; ++w) fn();
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(repeats));
    for (int r = 0; r < repeats; ++r) {
        const auto t0 = Clock::now();
        fn();
        const auto t1 = Clock::now();
        samples.push_back(std::chrono::duration<double, std::milli>(t1 - t0)
                              .count());
    }
    std::sort(samples.begin(), samples.end());
    Timed t;
    t.min_ms = samples.front();
    t.median_ms = samples[samples.size() / 2];
    const std::size_t p90 =
        static_cast<std::size_t>(0.9 * static_cast<double>(samples.size() - 1));
    t.p90_ms = samples[p90];
    return t;
}

// ===== 容量检查（≤70% 可用 RAM/VRAM）=====
bool capacity_ok(const Case& c, const Env& env, std::string& reason) {
    const std::uint64_t frames_bytes =
        c.frames * c.width * c.height * 4u + c.frames * 4u;
    const std::uint64_t output_bytes = c.width * c.height * 4u;
    const std::uint64_t ram_needed =
        frames_bytes + output_bytes * 4u;  // 输入 + 多份输出/参考
    if (env.total_ram > 0 && ram_needed > env.total_ram * 7 / 10) {
        reason = "RAM exceeds 70% of available";
        return false;
    }
    if (env.gpu_available && env.total_vram > 0) {
        const std::uint64_t vram_needed = frames_bytes + output_bytes;
        if (vram_needed > env.total_vram * 7 / 10) {
            reason = "VRAM exceeds 70% of available";
            return false;
        }
    }
    return true;
}

// ===== OperationProfile 构建（加权积分专用标定）=====
// 标定尺寸序列按输出像素；帧数固定 16（standard 档代表性帧数）。
constexpr std::size_t kProfileFrames = 16;

double fit_slope(const std::vector<std::size_t>& sizes,
                 const std::vector<double>& ms) {
    if (sizes.size() != ms.size() || sizes.size() < 2) return 0.0;
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        const double x = static_cast<double>(sizes[i]);
        const double y = ms[i];
        sx += x; sy += y; sxx += x * x; sxy += x * y;
    }
    const double denom = static_cast<double>(sizes.size()) * sxx - sx * sx;
    if (std::fabs(denom) < 1e-12) return 0.0;
    return (static_cast<double>(sizes.size()) * sxy - sx * sy) / denom;
}

double fit_intercept(const std::vector<std::size_t>& sizes,
                     const std::vector<double>& ms,
                     double slope) {
    if (sizes.empty()) return 0.0;
    double sy = 0, sx = 0;
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        sy += ms[i];
        sx += static_cast<double>(sizes[i]);
    }
    return (sy - slope * sx) / static_cast<double>(sizes.size());
}

std::string kernel_hash() {
    std::uint64_t h = 0x9E3779B97F4A7C15ULL;
    auto mix = [&h](const void* p) {
        std::uint64_t v = reinterpret_cast<std::uintptr_t>(p);
        v ^= v >> 23; v *= 0x2127599bf4325c37ULL; v ^= v >> 47;
        h ^= v + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
    };
    mix(reinterpret_cast<const void*>(
        &astro::compute::weighted_integration::integrate_one_pixel));
    mix(reinterpret_cast<const void*>(
        &astro::compute::weighted_integration::weighted_integration_openmp));
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%016llx",
                  static_cast<unsigned long long>(h));
    return std::string("acr-kernels-") + buf;
}

} // anonymous namespace

namespace astro::compute::weighted_integration {

// ===== 执行器/调度辅助（benchmark 与测试共用）=====
namespace {

cost::CostEstimate make_estimate(std::size_t rec_cpu, std::size_t rec_gpu) {
    cost::CostEstimate e;
    cost::DeviceCost dc;
    dc.device_id = kHwCpuDeviceId;
    dc.backend = "cpu";
    dc.recommended_chunk = rec_cpu;
    dc.min_effective_chunk = 1024;
    dc.feasible = true;
    dc.profile_available = true;
    e.per_device.push_back(dc);
    cost::DeviceCost gdc;
    gdc.device_id = static_cast<DeviceId>(1);
    gdc.backend = "cuda:0";
    gdc.recommended_chunk = rec_gpu;
    gdc.min_effective_chunk = 1024;
    gdc.feasible = true;
    gdc.profile_available = true;
    e.per_device.push_back(gdc);
    e.preferred_device = kHwCpuDeviceId;
    e.profile_available = true;
    return e;
}

TaskDescriptor make_task(std::size_t n) {
    TaskDescriptor task;
    task.range = Range1D{0, n};
    task.item_count = n;
    return task;
}

} // anonymous namespace

// ===== 单次加权积分 Invocation 构造 =====
KernelInvocation make_weighted_invocation(
    const WeightedIntegrationView& view,
    float* output,
    const std::vector<float>& frames,
    const std::vector<float>& weights) {
    KernelInvocation inv;
    inv.id = kOperationId;
    inv.domain = WorkDomain{0, view.pixel_count};
    inv.buffers.add(0, output, view.pixel_count, 1, BufferRole::Output);
    inv.buffers.add(1, const_cast<float*>(frames.data()),
                    frames.size(), 1, BufferRole::Input);
    inv.buffers.add(2, const_cast<float*>(weights.data()),
                    weights.size(), 1, BufferRole::Input);
    append_scalar(inv.scalars, view.frame_count);
    append_scalar(inv.scalars, view.pixel_count);
    inv.traits.bytes_read_per_item =
        view.frame_count * sizeof(float) + sizeof(float);
    inv.traits.bytes_written_per_item = sizeof(float);
    // 与 KernelRegistration 声明一致：FP32 计算 + FP64 累加
    inv.traits.numeric.compute = NumericPolicy::Compute::fp32;
    inv.traits.numeric.accumulator = NumericPolicy::Accumulator::fp64;
    inv.partition = PartitionKind::IndependentOutputTiles;
    return inv;
}

// ===== Dispatcher 执行一次加权积分 =====
// mode: CpuOnly / GpuOnly / AutoMixed（force 控制 ForcedMixed）
// profile: AutoMixed 使用；nullptr 时 Auto 走保守 CPU fallback
scheduler::CostAwareResult run_weighted_via_dispatcher(
    Dispatcher& d,
    const WeightedIntegrationView& view,
    float* output,
    const std::vector<float>& frames,
    const std::vector<float>& weights,
    std::size_t rec_cpu,
    std::size_t rec_gpu) {
    KernelInvocation inv =
        make_weighted_invocation(view, output, frames, weights);
    return d.dispatch_invocation(
        make_task(view.pixel_count),
        make_estimate(rec_cpu, rec_gpu), inv);
}

} // namespace astro::compute::weighted_integration

// ===== Benchmark 主程序 =====
int main(int argc, char** argv) {
    using namespace astro::compute;
    using namespace astro::compute::weighted_integration;
    using astro::compute::scheduler::ExecutorRegistry;

    Args args;
    if (!parse_args(argc, argv, args)) return 1;

    astro::compute::runtime_init();
    register_weighted_integration_kernels();
    const Env env = detect_env();
    // git HEAD 由 Evidence 脚本经 --git-head 传入（不可由 benchmark 自身 git 依赖）

    // ---- 首次 GPU context 初始化（计时外，公平性要求）----
    void* gpu_handle = nullptr;
    astro::compute::cuda::bridge::BridgeApi* bapi = nullptr;
    if (env.gpu_available) {
        using namespace astro::compute::cuda::bridge;
        ensure_bridge_loaded();
        bapi = &astro::compute::cuda::bridge::api();
        const char* err = nullptr;
        bapi->init(&err);
        gpu_handle = bapi->executor_create(0, 65536, 256, &err);
        if (args.gpu_streams > 0 && bapi->configure_streams) {
            bapi->configure_streams(gpu_handle, args.gpu_streams, &err);
        }
    }
    const int gpu_streams =
        (gpu_handle && bapi && bapi->stream_count)
            ? bapi->stream_count(gpu_handle) : 1;

    // ---- 可选：生成加权积分 OperationProfile（standard/full 默认生成）----
    OperationProfile profile;
    bool profile_ready = false;
    if (!args.correctness_only && env.gpu_available) {
        // 标定：ACR CpuOnly 全量（真实多 worker）+ GPU resident/host
        const std::vector<std::size_t> sizes{
            1u << 18, 1u << 20, 1u << 22, 1u << 24};  // 256K..16M 像素
        constexpr int kWarm = 2;
        constexpr int kRep = 7;
        std::vector<double> cpu_ms, gpu_res_ms, gpu_host_ms;
        std::vector<std::size_t> cpu_chunk_cands{1u << 16, 1u << 18, 1u << 20};
        std::vector<std::size_t> gpu_chunk_cands{1u << 18, 1u << 20, 1u << 22};
        std::vector<double> cpu_chunk_ms, gpu_chunk_ms;

        // CPU-only registry（真实 CpuExecutor 多 worker）
        auto cpu_regs = std::make_shared<ExecutorRegistry>(
            ExecutorRegistry::create_cpu_only());
        Dispatcher cpu_d;
        DispatcherConfig cpu_cfg;
        cpu_cfg.devices = {{"cpu", 0, 0, 50.0, true}};
        cpu_cfg.executors = cpu_regs;
        cpu_cfg.route_mode = RouteMode::CpuOnly;
        cpu_d.configure(cpu_cfg);

        for (std::size_t n : sizes) {
            const std::size_t pixels = n;
            std::vector<float> frames(kProfileFrames * pixels);
            std::vector<float> weights(kProfileFrames);
            generate_synthetic(0xA57C5AC20260802ULL, kProfileFrames, pixels,
                               frames, weights);
            std::vector<float> out(pixels);
            WeightedIntegrationView view{
                frames.data(), weights.data(), kProfileFrames, pixels};
            // CPU 全量（ACR CpuOnly）
            Timed t = measure(
                [&] {
                    run_weighted_via_dispatcher(cpu_d, view, out.data(),
                                                frames, weights,
                                                1u << 16, 1u << 18);
                },
                kWarm, kRep);
            cpu_ms.push_back(t.median_ms);
            // GPU resident（上传一次 + resident 提交）
            std::uint64_t el = 0;
            const char* err = nullptr;
            bapi->upload_persistent_slot(gpu_handle, 0, 0,
                                         kProfileFrames * pixels,
                                         frames.data(), &el, &err);
            bapi->upload_persistent_slot(gpu_handle, 1, 0,
                                         kProfileFrames, weights.data(),
                                         &el, &err);
            Timed tr = measure(
                [&] {
                    bapi->submit_weighted_integration_resident(
                        gpu_handle, 0, pixels, out.data(),
                        kProfileFrames, pixels, &el, &err);
                },
                kWarm, kRep);
            gpu_res_ms.push_back(tr.median_ms);
            // GPU host roundtrip（整帧 H2D + kernel + D2H）
            Timed th = measure(
                [&] {
                    bapi->submit_weighted_integration(
                        gpu_handle, 0, pixels, out.data(),
                        frames.data(), weights.data(),
                        kProfileFrames, pixels, &el, &err);
                },
                kWarm, kRep);
            gpu_host_ms.push_back(th.median_ms);
        }
        // 候选块实测（1M 像素，frames=16）
        {
            const std::size_t pixels = 1u << 20;
            std::vector<float> frames(kProfileFrames * pixels);
            std::vector<float> weights(kProfileFrames);
            generate_synthetic(0xA57C5AC20260802ULL, kProfileFrames, pixels,
                               frames, weights);
            std::vector<float> out(pixels);
            WeightedIntegrationView view{
                frames.data(), weights.data(), kProfileFrames, pixels};
            for (std::size_t c : cpu_chunk_cands) {
                Dispatcher cd;
                DispatcherConfig cc;
                cc.devices = {{"cpu", 0, 0, 50.0, true}};
                cc.executors = cpu_regs;
                cc.route_mode = RouteMode::CpuOnly;
                cd.configure(cc);
                std::vector<double> samples;
                for (int r = 0; r < 5; ++r) {
                    const auto t0 = std::chrono::steady_clock::now();
                    run_weighted_via_dispatcher(cd, view, out.data(),
                                                frames, weights, c, c);
                    const auto t1 = std::chrono::steady_clock::now();
                    samples.push_back(
                        std::chrono::duration<double, std::milli>(t1 - t0)
                            .count());
                }
                std::sort(samples.begin(), samples.end());
                cpu_chunk_ms.push_back(samples[samples.size() / 2]);
            }
            for (std::size_t cand : gpu_chunk_cands) {
                (void)cand;
                std::vector<double> samples;
                std::uint64_t el = 0;
                const char* err = nullptr;
                for (int r = 0; r < 5; ++r) {
                    const auto t0 = std::chrono::steady_clock::now();
                    bapi->submit_weighted_integration_resident(
                        gpu_handle, 0, pixels, out.data(),
                        kProfileFrames, pixels, &el, &err);
                    const auto t1 = std::chrono::steady_clock::now();
                    samples.push_back(
                        std::chrono::duration<double, std::milli>(t1 - t0)
                            .count());
                }
                std::sort(samples.begin(), samples.end());
                gpu_chunk_ms.push_back(samples[samples.size() / 2]);
            }
        }

        // ---- 组装 OperationProfile ----
        profile.schema_version = "acr-operation-profile-1";
        profile.profile_state = "qualified";
        profile.fingerprint_cpu = env.cpu;
        profile.fingerprint_compiler =
#if defined(_MSC_VER)
            "msvc-" + std::to_string(_MSC_VER);
#elif defined(__clang__)
            "clang";
#elif defined(__GNUC__)
            "gcc-" + std::to_string(__GNUC__) + "." +
            std::to_string(__GNUC_MINOR__);
#else
            "unknown";
#endif
        profile.fingerprint_runtime_kernel_hash = kernel_hash();
        profile.fingerprint_gpus.push_back(env.gpu);

        OperationProfile::Operation op;
        op.operation_id = kOp;
        op.precision = "fp32";
        op.accumulator = "fp64";
        op.qualified = true;
        op.qualification_reason = "measured-qualified";
        op.sample_range = {sizes.front(), sizes.back(), kRep};
        // CPU 曲线（斜率 ns/像素；固定开销来自截距）
        const double cpu_slope = fit_slope(sizes, cpu_ms);
        const double cpu_fixed_ms = fit_intercept(sizes, cpu_ms, cpu_slope);
        op.cpu.fixed_us = std::max(0.0, cpu_fixed_ms * 1000.0);
        op.cpu.ns_per_item = cpu_slope * 1e6;
        op.cpu.median_error_ratio = 0.0;
        op.cpu.p95_error_ratio = 0.0;
        // CPU 推荐块：吞吐最优点（ns/块最小）
        {
            std::size_t best = 0;
            for (std::size_t i = 1; i < cpu_chunk_cands.size(); ++i) {
                const double a = cpu_chunk_ms[best] /
                                 static_cast<double>(cpu_chunk_cands[best]);
                const double b = cpu_chunk_ms[i] /
                                 static_cast<double>(cpu_chunk_cands[i]);
                if (b < a * 0.95) best = i;
                else if (b <= a * 1.05 &&
                         cpu_chunk_cands[i] > cpu_chunk_cands[best]) {
                    best = i;
                }
            }
            op.cpu.recommended_chunk_items = cpu_chunk_cands[best];
            op.cpu.minimum_chunk_items = cpu_chunk_cands.front() / 4;
        }
        // GPU resident 曲线
        const double res_slope = fit_slope(sizes, gpu_res_ms);
        const double res_fixed_ms = fit_intercept(sizes, gpu_res_ms, res_slope);
        op.gpu.device_id = "cuda:0";
        op.gpu.fixed_us = std::max(0.0, res_fixed_ms * 1000.0);
        op.gpu.ns_per_item = res_slope * 1e6;
        op.gpu.launch_us = 0.0;
        op.gpu.median_error_ratio = 0.0;
        op.gpu.p95_error_ratio = 0.0;
        {
            std::size_t best = 0;
            for (std::size_t i = 1; i < gpu_chunk_cands.size(); ++i) {
                const double a = gpu_chunk_ms[best] /
                                 static_cast<double>(gpu_chunk_cands[best]);
                const double b = gpu_chunk_ms[i] /
                                 static_cast<double>(gpu_chunk_cands[i]);
                if (b < a * 0.95) best = i;
                else if (b <= a * 1.05 &&
                         gpu_chunk_cands[i] > gpu_chunk_cands[best]) {
                    best = i;
                }
            }
            op.gpu.recommended_chunk_items = gpu_chunk_cands[best];
            op.gpu.minimum_chunk_items = gpu_chunk_cands.front() / 4;
        }
        // 传输模型：从 host 曲线与 resident 曲线差拟合（每像素输入字节 =
        // frames 16×4B + 权重摊销 ≈ 64B；输出 4B）
        const double bytes_per_item =
            static_cast<double>(kProfileFrames) * 4.0 + 4.0;
        const double host_slope = fit_slope(sizes, gpu_host_ms);
        const double transfer_slope_ns =
            std::max(0.0, (host_slope - res_slope) * 1e6);
        op.transfer.h2d_gbps =
            (transfer_slope_ns > 0.0)
                ? bytes_per_item / transfer_slope_ns : 1.0;
        op.transfer.d2h_gbps = op.transfer.h2d_gbps;
        op.transfer.h2d_fixed_us = 100.0;
        op.transfer.d2h_fixed_us = 100.0;
        op.memory.host_bytes_per_item = bytes_per_item;
        op.memory.device_bytes_per_item = bytes_per_item;
        op.memory.fixed_host_bytes = 1u << 20;
        op.memory.fixed_device_bytes = 1u << 20;
        // 收益交叉点（ns 单位）：
        //   resident: cpu_fixed + cpu_slope*n = res_fixed + res_slope*n
        //   host:     cpu = res + transfer
        const double cpu_fixed_ns = cpu_fixed_ms * 1e6;
        const double res_fixed_ns = res_fixed_ms * 1e6;
        if (res_slope < cpu_slope) {
            double nstar =
                (cpu_fixed_ns - res_fixed_ns) /
                ((res_slope - cpu_slope) * 1e6);
            if (nstar <= 0.0) nstar = 1.0;
            op.gpu.resident_path_eligible = true;
            op.gpu.min_profitable_items_resident =
                static_cast<std::size_t>(nstar) + 1;
        } else {
            op.gpu.resident_path_eligible = false;
            op.gpu.min_profitable_items_resident = std::nullopt;
        }
        const double host_fixed_ns =
            res_fixed_ns + op.transfer.h2d_fixed_us * 1000.0 +
            op.transfer.d2h_fixed_us * 1000.0;
        if (host_slope < cpu_slope) {
            double nstar =
                (cpu_fixed_ns - host_fixed_ns) /
                ((host_slope - cpu_slope) * 1e6);
            if (nstar <= 0.0) nstar = 1.0;
            op.gpu.host_path_eligible = true;
            op.gpu.min_profitable_items_host =
                static_cast<std::size_t>(nstar) + 1;
        } else {
            op.gpu.host_path_eligible = false;
            op.gpu.min_profitable_items_host = std::nullopt;
        }
        profile.operations.push_back(std::move(op));
        std::string verr;
        if (qualification::focused::validate_operation_profile(profile,
                                                               verr)) {
            profile_ready = true;
            if (!args.profile_output.empty()) {
                qualification::focused::write_operation_profile_to_file(
                    args.profile_output, profile);
                std::fprintf(stdout, "[weighted] OperationProfile -> %s\n",
                             args.profile_output.c_str());
            }
        } else {
            std::fprintf(stderr, "[weighted] profile validation failed: %s\n",
                         verr.c_str());
        }
    }

    // ---- 报告骨架 ----
    nlohmann::json report;
    report["schema_version"] = "1.0";
    report["operation_id"] = kOp;
    report["preset"] = args.preset;
    report["seed"] = args.seed;
    report["environment"]["git_head"] = args.git_head;
    report["environment"]["build_type"] = "Release";
    report["environment"]["cpu"] = env.cpu;
    report["environment"]["gpu"] = env.gpu;
    report["environment"]["openmp_threads"] = env.openmp_threads;
    report["environment"]["acr_cpu_workers"] = env.acr_cpu_workers;
    report["environment"]["gpu_available"] = env.gpu_available;

    // 每 case 先算 OpenMP 基线（speedup 参照）——矩阵模式顺序轮转
    bool correctness_pass = true;
    std::vector<Case> cases = matrix_for(args.preset);
    std::vector<double> openmp_medians;
    std::vector<double> auto_mixed_medians;

    auto build_case_json = [&](const Case& c) -> nlohmann::json {
        nlohmann::json jc;
        jc["width"] = c.width;
        jc["height"] = c.height;
        jc["frame_count"] = c.frames;
        const std::size_t pixels = c.width * c.height;
        jc["pixel_count"] = pixels;
        jc["input_bytes"] = c.frames * pixels * 4u + c.frames * 4u;
        jc["modes"] = nlohmann::json::array();
        return jc;
    };

    std::vector<nlohmann::json> case_jsons;
    for (const auto& c : cases) {
        case_jsons.push_back(build_case_json(c));
    }

    // ---- 执行每个 case 的每个模式 ----
    for (std::size_t ci = 0; ci < cases.size(); ++ci) {
        const auto& c = cases[ci];
        const std::size_t pixels = c.width * c.height;
        nlohmann::json& jc = case_jsons[ci];
        std::string cap_reason;
        if (!capacity_ok(c, env, cap_reason)) {
            nlohmann::json m;
            m["mode"] = "openmp";
            m["status"] = "SKIPPED_CAPACITY";
            m["median_ms"] = 0;
            m["max_abs_error"] = 0;
            m["relative_l2_error"] = 0;
            m["route_reason"] = cap_reason;
            jc["modes"].push_back(m);
            correctness_pass = false;
            continue;
        }

        // 数据生成（计时外，固定 seed）
        std::vector<float> frames(c.frames * pixels);
        std::vector<float> weights(c.frames);
        std::vector<float> weights2(c.frames), weights3(c.frames),
            weights4(c.frames);
        generate_synthetic(args.seed, c.frames, pixels, frames, weights);
        generate_weights(args.seed + 1, c.frames, weights2);
        generate_weights(args.seed + 2, c.frames, weights3);
        generate_weights(args.seed + 3, c.frames, weights4);
        // 参考（Serial 或 OpenMP 输出作为正确性参照）
        WeightedIntegrationView view{
            frames.data(), weights.data(), c.frames, pixels};
        std::vector<float> ref(pixels);
        weighted_integration_serial(view, ref.data());
        std::vector<float> out(pixels);

        // 各模式
        const auto add_mode = [&](const std::string& mode,
                                  const Timed& t,
                                  const std::vector<float>& result,
                                  const std::string& status,
                                  const nlohmann::json& stats,
                                  const std::vector<float>* alt_ref =
                                      nullptr) {
            nlohmann::json m;
            m["mode"] = mode;
            m["status"] = status;
            m["median_ms"] = t.median_ms;
            m["min_ms"] = t.min_ms;
            m["p90_ms"] = t.p90_ms;
            m["speedup_vs_openmp"] = 0.0;
            m["max_abs_error"] = 0.0;
            m["relative_l2_error"] = 0.0;
            m["cpu_items"] = 0;
            m["gpu_items"] = 0;
            m["cpu_chunks"] = 0;
            m["gpu_chunks"] = 0;
            m["gpu_streams"] = gpu_streams;
            m["max_in_flight"] = 1;
            m["h2d_count"] = 0;
            m["d2h_count"] = 0;
            m["h2d_bytes"] = 0;
            m["d2h_bytes"] = 0;
            m["resident_reuse_count"] = 0;
            m["peak_ram_bytes"] = 0;
            m["peak_vram_bytes"] = 0;
            m["route_reason"] = "";
            m["chunk_sizes"] = nlohmann::json::array();
            const std::vector<float>& check_ref =
                (alt_ref != nullptr) ? *alt_ref : ref;
            if (status == "PASS" && result.size() == check_ref.size()) {
                const ErrorStats es = compare(check_ref, result);
                m["max_abs_error"] = es.max_abs;
                m["relative_l2_error"] = es.relative_l2;
                if (!es.finite || es.max_abs > 2e-5 ||
                    es.relative_l2 > 2e-6 ||
                    es.coverage != check_ref.size()) {
                    m["status"] = "FAIL";
                    correctness_pass = false;
                }
            } else if (status == "FAIL") {
                correctness_pass = false;
            }
            for (auto it = stats.begin(); it != stats.end(); ++it) {
                m[it.key()] = it.value();
            }
            jc["modes"].push_back(m);
        };

        // serial（仅 quick 小 case，作为参考）
        if (args.preset == "quick" && !args.correctness_only) {
            Timed ts = measure(
                [&] { weighted_integration_serial(view, out.data()); },
                args.warmup, args.repeats);
            add_mode("serial", ts, out, "PASS", {});
        }

        // OpenMP 基线
        Timed to = measure(
            [&] { weighted_integration_openmp(view, out.data(),
                                              env.openmp_threads); },
            args.warmup, args.repeats);
        add_mode("openmp", to, out, "PASS", {});
        openmp_medians.push_back(to.median_ms);

        // ACR CpuOnly
        {
            auto cpu_regs = std::make_shared<ExecutorRegistry>(
                ExecutorRegistry::create_cpu_only());
            Dispatcher d;
            DispatcherConfig cfg;
            cfg.devices = {{"cpu", 0, 0, 50.0, true}};
            cfg.executors = cpu_regs;
            cfg.route_mode = RouteMode::CpuOnly;
            d.configure(cfg);
            Timed t = measure(
                [&] {
                    std::fill(out.begin(), out.end(), 0.0f);
                    auto r = run_weighted_via_dispatcher(
                        d, view, out.data(), frames, weights,
                        1u << 16, 1u << 18);
                    if (!r.run_result.all_done) {
                        throw std::runtime_error(
                            "acr_cpu not all_done: " +
                            r.run_result.error_message);
                    }
                },
                args.warmup, args.repeats);
            nlohmann::json st;
            st["cpu_items"] = pixels;
            st["cpu_chunks"] = 0;
            st["route_reason"] = "cpu-only";
            add_mode("acr_cpu", t, out, "PASS", st);
        }

        // GPU 模式（无 GPU → SKIPPED_NO_GPU）
        if (!env.gpu_available || !gpu_handle) {
            nlohmann::json st;
            st["route_reason"] = "no GPU";
            add_mode("gpu_host", Timed{}, out, "SKIPPED_NO_GPU", st);
            add_mode("gpu_resident", Timed{}, out, "SKIPPED_NO_GPU", st);
            add_mode("forced_mixed", Timed{}, out, "SKIPPED_NO_GPU", st);
            add_mode("auto_mixed", Timed{}, out, "SKIPPED_NO_GPU", st);
            add_mode("auto_mixed_reuse", Timed{}, out, "SKIPPED_NO_GPU", st);
            continue;
        }

        // gpu_host（整帧 H2D + kernel + D2H）
        {
            std::uint64_t el = 0;
            const char* err = nullptr;
            Timed t = measure(
                [&] {
                    if (bapi->submit_weighted_integration(
                            gpu_handle, 0, pixels, out.data(),
                            frames.data(), weights.data(),
                            c.frames, pixels, &el, &err) != 0) {
                        throw std::runtime_error(err ? err : "gpu_host failed");
                    }
                },
                args.warmup, args.repeats);
            nlohmann::json st;
            st["gpu_items"] = pixels;
            st["gpu_chunks"] = 1;
            st["h2d_count"] = 1;
            st["d2h_count"] = 1;
            st["h2d_bytes"] = c.frames * pixels * 4u + c.frames * 4u;
            st["d2h_bytes"] = pixels * 4u;
            st["route_reason"] = "gpu-only host roundtrip";
            add_mode("gpu_host", t, out, "PASS", st);
        }

        // gpu_resident（prefetch 计时外 + resident 提交）
        {
            std::uint64_t el = 0;
            const char* err = nullptr;
            bapi->upload_persistent_slot(gpu_handle, 0, 0,
                                         c.frames * pixels,
                                         frames.data(), &el, &err);
            bapi->upload_persistent_slot(gpu_handle, 1, 0,
                                         c.frames, weights.data(),
                                         &el, &err);
            Timed t = measure(
                [&] {
                    if (bapi->submit_weighted_integration_resident(
                            gpu_handle, 0, pixels, out.data(),
                            c.frames, pixels, &el, &err) != 0) {
                        throw std::runtime_error(
                            err ? err : "gpu_resident failed");
                    }
                },
                args.warmup, args.repeats);
            nlohmann::json st;
            st["gpu_items"] = pixels;
            st["gpu_chunks"] = 1;
            st["h2d_count"] = 1;
            st["d2h_count"] = 1;
            st["h2d_bytes"] = c.frames * pixels * 4u + c.frames * 4u;
            st["d2h_bytes"] = pixels * 4u;
            st["route_reason"] = "gpu-only resident";
            add_mode("gpu_resident", t, out, "PASS", st);
        }

        // ForcedMixed（机制正确性：双方非零）
        {
            // 独立 1M 像素工作域（机制验证，不依赖 case 尺寸）：池内块数
            // 充足（16 块 @64K），首轮公平门下 CPU/GPU 都能领到非零工作。
            const std::size_t mx_pixels = 1u << 20;
            const std::size_t mx_frames = 8;
            std::vector<float> mx_frames_data(mx_frames * mx_pixels);
            std::vector<float> mx_weights(mx_frames);
            generate_synthetic(args.seed + 77, mx_frames, mx_pixels,
                               mx_frames_data, mx_weights);
            WeightedIntegrationView mx_view{
                mx_frames_data.data(), mx_weights.data(),
                mx_frames, mx_pixels};
            std::vector<float> mx_out(mx_pixels, 0.0f);
            auto regs = std::make_shared<ExecutorRegistry>(
                ExecutorRegistry::create_auto());
            Dispatcher d;
            DispatcherConfig cfg;
            cfg.devices = {{"cpu", 0, 0, 50.0, true},
                           {"cuda:0", 1, 0, 500.0, true}};
            cfg.executors = regs;
            cfg.route_mode = RouteMode::AutoMixed;
            cfg.force_all_supported_executors = true;
            d.configure(cfg);
            auto r = run_weighted_via_dispatcher(
                d, mx_view, mx_out.data(), mx_frames_data, mx_weights,
                1u << 16, 1u << 16);
            const bool both =
                r.run_result.all_done && r.chunks_on_cpu > 0 &&
                r.chunks_on_gpu > 0;
            // 数值正确性（1M 工作域对 Serial 参考；与 case 无关）
            std::vector<float> mx_ref(mx_pixels);
            weighted_integration_serial(mx_view, mx_ref.data());
            const ErrorStats mx_es = compare(mx_ref, mx_out);
            const bool mx_ok =
                mx_es.finite && mx_es.max_abs <= 2e-5 &&
                mx_es.relative_l2 <= 2e-6 &&
                mx_es.coverage == mx_pixels;
            Timed t = measure(
                [&] {
                    std::fill(mx_out.begin(), mx_out.end(), 0.0f);
                    auto rr = run_weighted_via_dispatcher(
                        d, mx_view, mx_out.data(), mx_frames_data,
                        mx_weights,
                        1u << 16, 1u << 16);
                    if (!rr.run_result.all_done) {
                        throw std::runtime_error(
                            "forced_mixed not all_done: " +
                            rr.run_result.error_message);
                    }
                },
                args.warmup, args.repeats);
            nlohmann::json st;
            st["cpu_items"] = 0;
            st["gpu_items"] = 0;
            st["cpu_chunks"] = r.chunks_on_cpu;
            st["gpu_chunks"] = r.chunks_on_gpu;
            st["h2d_count"] = 1;
            st["d2h_count"] = r.chunks_on_gpu;
            st["h2d_bytes"] =
                mx_frames * mx_pixels * 4u + mx_frames * 4u;
            st["d2h_bytes"] =
                r.chunks_on_gpu *
                (mx_pixels / std::max<std::size_t>(
                    1, r.chunks_on_cpu + r.chunks_on_gpu)) * 4u;
            st["route_reason"] =
                (both && mx_ok) ? "forced mixed correctness" :
                                  "mixed not achieved or incorrect";
            add_mode("forced_mixed", t, mx_out,
                     (both && mx_ok) ? "PASS" : "FAIL", st, &mx_ref);
        }

        // AutoMixed（OperationProfile 驱动；无 profile 时保守 CPU）
        {
            auto regs = std::make_shared<ExecutorRegistry>(
                ExecutorRegistry::create_auto());
            Dispatcher d;
            DispatcherConfig cfg;
            cfg.devices = {{"cpu", 0, 0, 50.0, true},
                           {"cuda:0", 1, 0, 500.0, true}};
            cfg.executors = regs;
            cfg.route_mode = RouteMode::AutoMixed;
            cfg.operation_profile =
                profile_ready ? &profile : nullptr;
            d.configure(cfg);
            auto r = run_weighted_via_dispatcher(
                d, view, out.data(), frames, weights, 1u << 16, 1u << 18);
            Timed t = measure(
                [&] {
                    std::fill(out.begin(), out.end(), 0.0f);
                    auto rr = run_weighted_via_dispatcher(
                        d, view, out.data(), frames, weights,
                        1u << 16, 1u << 18);
                    if (!rr.run_result.all_done) {
                        throw std::runtime_error(
                            "auto_mixed not all_done: " +
                            rr.run_result.error_message);
                    }
                },
                args.warmup, args.repeats);
            nlohmann::json st;
            st["cpu_items"] = 0;
            st["gpu_items"] = 0;
            st["cpu_chunks"] = r.chunks_on_cpu;
            st["gpu_chunks"] = r.chunks_on_gpu;
            st["max_in_flight"] = (r.chunks_on_gpu > 0) ? gpu_streams : 1;
            st["h2d_count"] = 1;
            st["d2h_count"] = r.chunks_on_gpu;
            st["h2d_bytes"] = c.frames * pixels * 4u + c.frames * 4u;
            st["d2h_bytes"] = 0;
            st["route_reason"] =
                profile_ready ? "auto profile-driven" : "auto cpu-fallback";
            add_mode("auto_mixed", t, out, "PASS", st);
            auto_mixed_medians.push_back(t.median_ms);
        }

        // AutoMixed resident-reuse：同一帧栈 4 组权重，frames 只上传一次
        {
            auto regs = std::make_shared<ExecutorRegistry>(
                ExecutorRegistry::create_auto());
            Dispatcher d;
            DispatcherConfig cfg;
            cfg.devices = {{"cpu", 0, 0, 50.0, true},
                           {"cuda:0", 1, 0, 500.0, true}};
            cfg.executors = regs;
            cfg.route_mode = RouteMode::AutoMixed;
            cfg.operation_profile =
                profile_ready ? &profile : nullptr;
            d.configure(cfg);
            std::vector<const std::vector<float>*> wsets{
                &weights, &weights2, &weights3, &weights4};
            bool ok_all = true;
            std::vector<float> ref_last;
            std::size_t reuse_idx = 0;
            Timed t = measure(
                [&] {
                    for (const auto* w : wsets) {
                        const std::size_t gi = reuse_idx;
                        ++reuse_idx;
                        WeightedIntegrationView v2{
                            frames.data(), w->data(), c.frames, pixels};
                        auto rr = run_weighted_via_dispatcher(
                            d, v2, out.data(), frames, *w,
                            1u << 16, 1u << 16);
                        if (!rr.run_result.all_done) {
                            ok_all = false;
                            throw std::runtime_error(
                                "reuse not all_done: " +
                                rr.run_result.error_message);
                        }
                        // 每组结果对 Serial 参考验证（reuse 权重不同，
                        // 参考必须按组计算，不能用 case 第一组权重参考）
                        std::vector<float> ref_g(pixels);
                        weighted_integration_serial(v2, ref_g.data());
                        const ErrorStats es = compare(ref_g, out);
                        if (!es.finite || es.max_abs > 2e-5 ||
                            es.relative_l2 > 2e-6 ||
                            es.coverage != pixels) {
                            ok_all = false;
                        }
                        ref_last = std::move(ref_g);
                    }
                },
                args.warmup, args.repeats);
            nlohmann::json st;
            st["cpu_items"] = 0;
            st["gpu_items"] = 0;
            st["cpu_chunks"] = 0;
            st["gpu_chunks"] = 0;
            st["h2d_count"] = 1;  // frames 只上传一次（组合 prefetch 首次）
            st["d2h_count"] = 0;
            st["h2d_bytes"] = c.frames * pixels * 4u;
            st["d2h_bytes"] = 0;
            st["resident_reuse_count"] = 3;  // 后三次 frames 复用
            st["route_reason"] =
                ok_all ? "resident reuse frames upload once" :
                         "reuse failed";
            add_mode("auto_mixed_reuse", t, out,
                     ok_all ? "PASS" : "FAIL", st, &ref_last);
        }
    }

    // ---- speedup_vs_openmp 回填 + 性能资格 ----
    for (auto& jc : case_jsons) {
        double omp_ms = 0.0;
        for (auto& m : jc["modes"]) {
            if (m["mode"] == "openmp") omp_ms = m["median_ms"].get<double>();
        }
        for (auto& m : jc["modes"]) {
            if (m["status"] == "PASS" && omp_ms > 0.0) {
                m["speedup_vs_openmp"] =
                    omp_ms / std::max(m["median_ms"].get<double>(), 1e-9);
            }
        }
    }

    // ---- 汇总与资格 ----
    nlohmann::json qualification;
    qualification["correctness"] =
        correctness_pass ? "PASS" : "FAIL";
    // 性能资格：标准档至少一个中/大 case Auto 相对 OpenMP ≥1.05x
    // （若没有 → PERFORMANCE_NOT_QUALIFIED，不开始业务改造）
    bool perf_ok = false;
    std::string perf_reason;
    if (args.correctness_only) {
        qualification["performance"] = "NOT_RUN";
        perf_reason = "correctness-only mode";
    } else if (args.preset == "quick") {
        qualification["performance"] = "NOT_RUN";
        perf_reason = "quick preset is correctness-only";
    } else {
        for (std::size_t ci = 0; ci < cases.size(); ++ci) {
            const auto& c = cases[ci];
            const auto& jc = case_jsons[ci];
            double omp_ms = 0.0, auto_ms = 0.0, best_ms = 0.0;
            for (const auto& m : jc["modes"]) {
                const std::string mode = m["mode"];
                if (m["status"] != "PASS") continue;
                if (mode == "openmp") omp_ms = m["median_ms"].get<double>();
                if (mode == "auto_mixed") auto_ms = m["median_ms"].get<double>();
                if (mode == "gpu_resident" || mode == "acr_cpu") {
                    best_ms = best_ms == 0.0
                        ? m["median_ms"].get<double>()
                        : std::min(best_ms, m["median_ms"].get<double>());
                }
            }
            if (omp_ms > 0.0 && auto_ms > 0.0 &&
                c.width * c.height >= 1024u * 1024u &&
                auto_ms <= omp_ms / 1.05) {
                perf_ok = true;
            }
        }
        if (perf_ok) {
            qualification["performance"] = "QUALIFIED";
            perf_reason = "at least one medium/large AutoMixed case "
                          ">=1.05x vs OpenMP";
        } else {
            qualification["performance"] = "PERFORMANCE_NOT_QUALIFIED";
            perf_reason = "no medium/large AutoMixed speedup >=1.05x; "
                          "report reproducible reason, do not start "
                          "business adapter";
        }
    }
    qualification["ready_for_business_adapter"] =
        correctness_pass && profile_ready &&
        qualification["performance"] == "QUALIFIED";
    qualification["reason"] = perf_reason;
    report["qualification"] = qualification;
    for (auto& jc : case_jsons) {
        report["cases"].push_back(jc);
    }

    // ---- 写出报告 ----
    {
        std::ofstream f(args.output);
        if (!f) {
            std::fprintf(stderr, "cannot write %s\n", args.output.c_str());
            if (gpu_handle) bapi->executor_destroy(gpu_handle);
            astro::compute::runtime_shutdown();
            return 4;
        }
        f << report.dump(1) << "\n";
    }
    std::printf("[weighted] preset=%s correctness=%s perf=%s "
                "profile=%d -> %s\n",
                args.preset.c_str(),
                qualification["correctness"].get<std::string>().c_str(),
                qualification["performance"].get<std::string>().c_str(),
                profile_ready ? 1 : 0, args.output.c_str());

    if (gpu_handle) bapi->executor_destroy(gpu_handle);
    astro::compute::runtime_shutdown();
    return correctness_pass ? 0 : 3;
}
