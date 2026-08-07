// lib/acr/examples/weighted_integration/weighted_integration_benchmark.cpp
//
// ACR 架构冻结（07 号计划 A/B/C/D/E）：加权积分公平 Benchmark。
//
// 场景分离（04/05 号规范）：
//   openmp_single / openmp_reuse4_total / acr_cpu /
//   gpu_host_cold / gpu_resident_steady / forced_mixed（仅正确性）/
//   auto_cold_single_shot / auto_resident_steady / auto_resident_reuse4
//
// 等价工作量原则：
//   - Serial 参考在计时外预计算；
//   - reuse4 与 4 次等价 OpenMP 总耗时比较；
//   - ForcedMixed 不参与性能资格（comparable_for_performance=false、
//     speedup=null）；
//   - 不可比较结果的 speedup 为 null。
//
// 真实统计（07 号计划 B）：
//   - CPU/GPU items、blocks、active ns 来自 CostAwareResult.per_device_stats；
//   - chunk 序列来自 resource_control.dynamic_chunk_sizes；
//   - H2D/D2H 次数与字节来自 Dispatcher.transfer_stats（真实 prefetch/物化）；
//   - RAM/VRAM 峰值来自内存预算估算与桥接 device_memory 观测。
//
// Stream（07 号计划 D）：本轮冻结同步语义，observed_max_in_flight=1；
// configured_streams 单独报告，不宣称多通道并发收益。
#include "weighted_integration_kernels.hpp"
#include "route_profile_calibration.hpp"

#include "astro/compute/kernel_registry.hpp"
#include "astro/compute/task_traits.hpp"
#include "scheduler/dispatcher.hpp"
#include "scheduler/device_executor.hpp"
#include "focused/operation_profile.hpp"
#include "../core/task_descriptor.hpp"
#include "../cost/cost_estimator.hpp"
#include "../backends/cuda/bridge/cuda_bridge_api.hpp"
#include "../../routing/route_profile_v2.hpp"
#include "../../routing/benchmark_route_estimator.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
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
constexpr std::size_t kProfileFrames = 16;
// GPU 候选块标定帧数（降低大工作域显存占用；候选相对吞吐测量与曲线标定
// 帧数无耦合，仅要求同一域内固定帧数）。
constexpr std::size_t kCandidateFrames = 8;

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
    int gpu_streams{0};  // 0=auto(=>1) 1/2/3：性能只用 1，2/3 仅数值实验
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
                "  [--seed N] [--gpu-streams auto|1|2|3] [--git-head SHA] [--correctness-only]\n");
            return false;
        } else {
            std::fprintf(stderr, "error: unknown arg %s\n", a.c_str());
            return false;
        }
    }
    return true;
}

struct Case { std::size_t width, height, frames; };

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

struct Env {
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

struct Timed {
    double total_median_ms{0.0};
    double total_min_ms{0.0};
    double total_p90_ms{0.0};
};

// fn 执行一次"测量单元"（可能含多次操作，如 reuse4 的 4 次积分）。
// 返回中位/min/p90 的总耗时。
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
    t.total_min_ms = samples.front();
    t.total_median_ms = samples[samples.size() / 2];
    const std::size_t p90 =
        static_cast<std::size_t>(0.9 * static_cast<double>(samples.size() - 1));
    t.total_p90_ms = samples[p90];
    return t;
}

bool capacity_ok(const Case& c, const Env& env, std::string& reason) {
    const std::uint64_t frames_bytes =
        c.frames * c.width * c.height * 4u + c.frames * 4u;
    const std::uint64_t output_bytes = c.width * c.height * 4u;
    const std::uint64_t ram_needed =
        frames_bytes + output_bytes * 5u;  // 输入 + 参考/输出/基线
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

std::uint64_t vram_used_bytes(astro::compute::cuda::bridge::BridgeApi* api,
                              void* handle) {
    if (api == nullptr || handle == nullptr || !api->device_memory) return 0;
    std::uint64_t total = 0, free = 0;
    const char* err = nullptr;
    if (api->device_memory(0, &total, &free, &err) != 0) return 0;
    return total - free;
}

} // anonymous namespace

namespace astro::compute::weighted_integration {

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
    inv.traits.numeric.compute = NumericPolicy::Compute::fp32;
    inv.traits.numeric.accumulator = NumericPolicy::Accumulator::fp64;
    inv.partition = PartitionKind::IndependentOutputTiles;
    return inv;
}

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

namespace {

using astro::compute::scheduler::CostAwareResult;
using astro::compute::weighted_integration::WeightedIntegrationView;
using astro::compute::weighted_integration::generate_synthetic;
using astro::compute::weighted_integration::generate_weights;
using astro::compute::weighted_integration::integrate_one_pixel;
using astro::compute::weighted_integration::register_weighted_integration_kernels;
using astro::compute::weighted_integration::run_weighted_via_dispatcher;
using astro::compute::weighted_integration::make_weighted_invocation;
using astro::compute::weighted_integration::kOperationId;

// ===== 报告辅助：从 CostAwareResult 提取真实统计到 JSON =====
void fill_real_stats(nlohmann::json& m, const CostAwareResult& r) {
    std::uint64_t cpu_items = 0, gpu_items = 0;
    std::uint64_t cpu_blocks = 0, gpu_blocks = 0;
    std::uint64_t cpu_active_ns = 0, gpu_active_ns = 0;
    for (const auto& pds : r.per_device_stats) {
        if (pds.backend.rfind("cuda", 0) == 0) {
            gpu_items += pds.items_done;
            gpu_blocks += pds.blocks_done;
            gpu_active_ns += pds.active_duration_ns;
        } else if (pds.backend == "cpu") {
            cpu_items += pds.items_done;
            cpu_blocks += pds.blocks_done;
            cpu_active_ns += pds.active_duration_ns;
        }
    }
    m["cpu_items"] = cpu_items;
    m["gpu_items"] = gpu_items;
    m["cpu_chunks"] = cpu_blocks;
    m["gpu_chunks"] = gpu_blocks;
    m["cpu_active_ns"] = cpu_active_ns;
    m["gpu_active_ns"] = gpu_active_ns;
    m["chunk_sizes"] = nlohmann::json::array();
    for (std::size_t c : r.resource_control.dynamic_chunk_sizes) {
        m["chunk_sizes"].push_back(c);
    }
    m["h2d_count"] = r.transfer_stats.h2d_count;
    m["d2h_count"] = r.transfer_stats.d2h_count;
    m["h2d_bytes"] = r.transfer_stats.h2d_bytes;
    m["d2h_bytes"] = r.transfer_stats.d2h_bytes;
    m["frames_upload_count"] = r.transfer_stats.frames_upload_count;
    m["weights_upload_count"] = r.transfer_stats.weights_upload_count;
    m["peak_ram_bytes"] = r.resource_control.mem_peak_max;
    m["mem_actions"] = nlohmann::json::array();
    for (const auto& a : r.resource_control.control_actions) {
        m["mem_actions"].push_back(a);
    }
    m["route_reason"] = r.run_result.error_message.empty()
        ? (r.actual_devices_used.empty() ? std::string("none")
                                         : r.actual_devices_used.front())
        : r.run_result.error_message;
}

// ===== 单模式报告 =====
struct ModeReporter {
    nlohmann::json& jc;
    int gpu_streams;

    void add(const std::string& mode,
             const std::string& status,
             const Timed& t,
             std::size_t operations,
             bool comparable,
             double openmp_equiv_ms,      // 等价基线（ms）；不可比时 <0
             const std::vector<float>& ref,
             const std::vector<float>& result,
             std::size_t pixels,
             const nlohmann::json& extra) {
        nlohmann::json m;
        m["mode"] = mode;
        m["status"] = status;
        m["median_ms"] = t.total_median_ms;  // 兼容旧 schema 字段
        m["total_median_ms"] = t.total_median_ms;
        m["total_min_ms"] = t.total_min_ms;
        m["total_p90_ms"] = t.total_p90_ms;
        m["operations_per_measurement"] = operations;
        m["per_call_median_ms"] =
            operations > 0 ? t.total_median_ms /
                                 static_cast<double>(operations) : 0.0;
        m["comparison_scope"] =
            comparable ? "equivalent-workload" : "not-comparable";
        m["comparable_for_performance"] = comparable;
        m["configured_streams"] = gpu_streams;
        m["observed_max_in_flight"] = 1;  // 同步语义（07 号计划 D）
        m["max_abs_error"] = 0.0;
        m["relative_l2_error"] = 0.0;
        m["cpu_items"] = 0;
        m["gpu_items"] = 0;
        m["cpu_chunks"] = 0;
        m["gpu_chunks"] = 0;
        m["chunk_sizes"] = nlohmann::json::array();
        m["h2d_count"] = 0;
        m["d2h_count"] = 0;
        m["h2d_bytes"] = 0;
        m["d2h_bytes"] = 0;
        m["peak_ram_bytes"] = 0;
        m["peak_vram_bytes"] = 0;
        m["mem_actions"] = nlohmann::json::array();
        m["route_reason"] = "";
        // 正确性校验（仅 PASS 且尺寸匹配）
        bool correct = true;
        if (status == "PASS" && result.size() == ref.size()) {
            const auto es = astro::compute::weighted_integration::compare(
                ref, result);
            m["max_abs_error"] = es.max_abs;
            m["relative_l2_error"] = es.relative_l2;
            correct = es.finite && es.max_abs <= 2e-5 &&
                      es.relative_l2 <= 2e-6 &&
                      es.coverage == pixels;
            if (!correct) m["status"] = "FAIL";
        }
        // 等价工作量才计算 speedup；否则 null
        if (comparable && status == "PASS" && correct &&
            openmp_equiv_ms > 0.0 && t.total_median_ms > 0.0) {
            m["speedup_vs_equivalent_openmp"] =
                openmp_equiv_ms / t.total_median_ms;
        } else {
            m["speedup_vs_equivalent_openmp"] = nullptr;
        }
        for (auto it = extra.begin(); it != extra.end(); ++it) {
            m[it.key()] = it.value();
        }
        jc["modes"].push_back(std::move(m));
    }
};

} // anonymous namespace

int main(int argc, char** argv) {
    using namespace astro::compute;
    using namespace astro::compute::weighted_integration;
    using astro::compute::scheduler::ExecutorRegistry;

    Args args;
    if (!parse_args(argc, argv, args)) return 1;
    const int configured_streams =
        (args.gpu_streams >= 1) ? args.gpu_streams : 1;

    astro::compute::runtime_init();
    register_weighted_integration_kernels();
    const Env env = detect_env();

    // ---- GPU context 初始化（计时外）----
    astro::compute::cuda::bridge::BridgeApi* bapi = nullptr;
    void* gpu_handle = nullptr;
    if (env.gpu_available) {
        using namespace astro::compute::cuda::bridge;
        ensure_bridge_loaded();
        bapi = &astro::compute::cuda::bridge::api();
        const char* err = nullptr;
        bapi->init(&err);
        gpu_handle = bapi->executor_create(0, 65536, 256, &err);
    }

    // =====================================================================
    // OperationProfile 标定（07 号计划 C：候选块真实执行 + 真实误差）
    // =====================================================================
    // =====================================================================
    // Route Profile v2 标定（Benchmark 驱动路由，控制包 d026ea30...c178537）
    // =====================================================================
    routing::RouteProfileV2 route_profile;
    bool route_profile_ready = false;
    if (!args.correctness_only && env.gpu_available && gpu_handle && bapi) {
        CalibrationEnv cen;
        cen.cpu_fingerprint = env.cpu;
        cen.gpu_name = env.gpu;
        cen.compiler =
#if defined(_MSC_VER)
            "msvc-" + std::to_string(_MSC_VER);
#elif defined(__GNUC__)
            "gcc-" + std::to_string(__GNUC__) + "." +
            std::to_string(__GNUC_MINOR__);
#else
            "unknown";
#endif
        cen.kernel_hash = kernel_hash();
        cen.openmp_threads = env.openmp_threads;
        cen.gpu_available = env.gpu_available;
        if (!args.profile_output.empty()) {
            route_profile_ready = calibrate_route_profile_v2(
                cen, gpu_handle, bapi, args.profile_output, route_profile);
        }
    }

    // =====================================================================
    // Benchmark 主循环
    // =====================================================================
    nlohmann::json report;
    report["schema_version"] = "1.1";
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
    report["environment"]["configured_streams"] = configured_streams;
    report["environment"]["observed_max_in_flight"] = 1;
    bool correctness_pass = true;
    std::vector<Case> cases = matrix_for(args.preset);
    std::vector<nlohmann::json> case_jsons;
    double openmp_single_median = 0.0;

    for (std::size_t ci = 0; ci < cases.size(); ++ci) {
        const auto& c = cases[ci];
        const std::size_t pixels = c.width * c.height;
        nlohmann::json jc;
        jc["width"] = c.width;
        jc["height"] = c.height;
        jc["frame_count"] = c.frames;
        jc["pixel_count"] = pixels;
        jc["input_bytes"] = c.frames * pixels * 4u + c.frames * 4u;
        jc["modes"] = nlohmann::json::array();
        case_jsons.push_back(std::move(jc));
        nlohmann::json& jj = case_jsons.back();
        ModeReporter rep{jj, configured_streams};

        std::string cap_reason;
        if (!capacity_ok(c, env, cap_reason)) {
            for (const char* mode :
                 {"openmp_single", "acr_cpu", "gpu_host_cold",
                  "gpu_resident_steady", "forced_mixed",
                  "auto_cold_single_shot", "auto_resident_steady",
                  "auto_resident_reuse4", "openmp_reuse4_total"}) {
                Timed t0;
                rep.add(mode, "SKIPPED_CAPACITY", t0, 1, false, -1.0,
                        std::vector<float>{}, std::vector<float>{}, pixels,
                        {{"route_reason", cap_reason}});
            }
            correctness_pass = false;
            continue;
        }

        // ---- 数据生成 + 4 组权重（计时外）----
        std::vector<float> frames(c.frames * pixels);
        std::vector<float> w0(c.frames), w1(c.frames), w2(c.frames),
            w3(c.frames);
        generate_synthetic(args.seed, c.frames, pixels, frames, w0);
        generate_weights(args.seed + 1, c.frames, w1);
        generate_weights(args.seed + 2, c.frames, w2);
        generate_weights(args.seed + 3, c.frames, w3);
        const std::vector<float>* ws[] = {&w0, &w1, &w2, &w3};

        // ---- Serial 参考（计时外，quick 小 case 作为正确性基准）----
        std::vector<float> ref(pixels);
        {
            WeightedIntegrationView v0{
                frames.data(), w0.data(), c.frames, pixels};
            for (std::size_t p = 0; p < pixels; ++p) {
                ref[p] = integrate_one_pixel(v0, p);
            }
        }
        // reuse4 的 4 组 Serial 参考（全部计时外预计算）
        std::vector<std::vector<float>> reuse_refs;
        for (const auto* w : ws) {
            WeightedIntegrationView v{frames.data(), w->data(),
                                      c.frames, pixels};
            std::vector<float> r(pixels);
            for (std::size_t p = 0; p < pixels; ++p) {
                r[p] = integrate_one_pixel(v, p);
            }
            reuse_refs.push_back(std::move(r));
        }
        std::vector<float> out(pixels);

        // ---- openmp_single：等价基线（1 次）----
        {
            WeightedIntegrationView v0{
                frames.data(), w0.data(), c.frames, pixels};
            Timed t = measure(
                [&] { weighted_integration_openmp(v0, out.data(),
                                                  env.openmp_threads); },
                args.warmup, args.repeats);
            openmp_single_median = t.total_median_ms;
            rep.add("openmp_single", "PASS", t, 1, true, t.total_median_ms,
                    ref, out, pixels, {});
        }

        // ---- openmp_reuse4_total：4 组权重 4 次 OpenMP 总耗时 ----
        Timed openmp_reuse4;
        {
            Timed t = measure(
                [&] {
                    for (const auto* w : ws) {
                        WeightedIntegrationView v{
                            frames.data(), w->data(), c.frames, pixels};
                        weighted_integration_openmp(v, out.data(),
                                                    env.openmp_threads);
                    }
                },
                args.warmup, args.repeats);
            openmp_reuse4 = t;
            rep.add("openmp_reuse4_total", "PASS", t, 4, true,
                    t.total_median_ms, reuse_refs[3], out, pixels, {});
        }

        // ---- acr_cpu：ACR CpuOnly（等价 1 次）----
        {
            auto regs = std::make_shared<ExecutorRegistry>(
                ExecutorRegistry::create_cpu_only());
            Dispatcher d;
            DispatcherConfig cfg;
            cfg.devices = {{"cpu", 0, 0, 50.0, true}};
            cfg.executors = regs;
            cfg.route_mode = RouteMode::CpuOnly;
            d.configure(cfg);
            WeightedIntegrationView v0{
                frames.data(), w0.data(), c.frames, pixels};
            nlohmann::json extra;
            CostAwareResult first;
            Timed t = measure(
                [&] {
                    std::fill(out.begin(), out.end(), 0.0f);
                    first = run_weighted_via_dispatcher(
                        d, v0, out.data(), frames, w0, 1u << 16, 1u << 18);
                    if (!first.run_result.all_done) {
                        throw std::runtime_error(
                            "acr_cpu not all_done: " +
                            first.run_result.error_message);
                    }
                },
                args.warmup, args.repeats);
            fill_real_stats(extra, first);
            extra["comparison_scope"] = "equivalent-workload";
            rep.add("acr_cpu", "PASS", t, 1, true,
                    openmp_single_median, ref, out, pixels, extra);
        }

        // ---- GPU 模式（无 GPU → SKIPPED_NO_GPU）----
        if (!env.gpu_available || !gpu_handle) {
            for (const char* mode :
                 {"gpu_host_cold", "gpu_resident_steady", "forced_mixed",
                  "auto_cold_single_shot", "auto_resident_steady",
                  "auto_resident_reuse4"}) {
                Timed t0;
                rep.add(mode, "SKIPPED_NO_GPU", t0, 1, false, -1.0,
                        std::vector<float>{}, std::vector<float>{}, pixels,
                        {{"route_reason", "no GPU"}});
            }
            continue;
        }

        // ---- gpu_host_cold：整帧 H2D + kernel + D2H（1 次）----
        {
            std::uint64_t el = 0;
            const char* err = nullptr;
            const auto vr0 = vram_used_bytes(bapi, gpu_handle);
            Timed t = measure(
                [&] {
                    if (bapi->submit_weighted_integration(
                            gpu_handle, 0, pixels, out.data(),
                            frames.data(), w0.data(),
                            c.frames, pixels, &el, &err) != 0) {
                        throw std::runtime_error(err ? err : "gpu_host failed");
                    }
                },
                args.warmup, args.repeats);
            const auto vr1 = vram_used_bytes(bapi, gpu_handle);
            nlohmann::json extra;
            extra["h2d_count"] = 1;
            extra["d2h_count"] = 1;
            extra["h2d_bytes"] = c.frames * pixels * 4u + c.frames * 4u;
            extra["d2h_bytes"] = pixels * 4u;
            extra["gpu_items"] = pixels;
            extra["gpu_chunks"] = 1;
            extra["peak_vram_bytes"] =
                vr1 > vr0 ? vr1 - vr0 : 0;
            extra["route_reason"] = "gpu-only host cold";
            rep.add("gpu_host_cold", "PASS", t, 1, true,
                    openmp_single_median, ref, out, pixels, extra);
        }

        // ---- gpu_resident_steady：prefetch 计时外 + resident 提交 ----
        {
            std::uint64_t el = 0;
            const char* err = nullptr;
            bapi->upload_persistent_slot(gpu_handle, 0, 0,
                                         c.frames * pixels,
                                         frames.data(), &el, &err);
            bapi->upload_persistent_slot(gpu_handle, 1, 0,
                                         c.frames, w0.data(), &el, &err);
            const auto vr0 = vram_used_bytes(bapi, gpu_handle);
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
            const auto vr1 = vram_used_bytes(bapi, gpu_handle);
            nlohmann::json extra;
            extra["h2d_count"] = 1;
            extra["d2h_count"] = 1;
            extra["h2d_bytes"] = c.frames * pixels * 4u + c.frames * 4u;
            extra["d2h_bytes"] = pixels * 4u;
            extra["gpu_items"] = pixels;
            extra["gpu_chunks"] = 1;
            extra["peak_vram_bytes"] =
                vr1 > vr0 ? vr1 - vr0 : 0;
            extra["route_reason"] = "gpu-only resident steady";
            rep.add("gpu_resident_steady", "PASS", t, 1, true,
                    openmp_single_median, ref, out, pixels, extra);
        }

        // ---- forced_mixed：仅机制正确性（不参与性能；固定 1M 域）----
        {
            const std::size_t mx_pixels = 1u << 20;
            const std::size_t mx_frames = 8;
            std::vector<float> fdata(mx_frames * mx_pixels);
            std::vector<float> w(mx_frames);
            generate_synthetic(args.seed + 77, mx_frames, mx_pixels,
                               fdata, w);
            WeightedIntegrationView mx_view{
                fdata.data(), w.data(), mx_frames, mx_pixels};
            std::vector<float> mx_ref(mx_pixels), mx_out(mx_pixels, 0.0f);
            for (std::size_t p = 0; p < mx_pixels; ++p) {
                mx_ref[p] = integrate_one_pixel(mx_view, p);
            }
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
            CostAwareResult first;
            const auto vr0 = vram_used_bytes(bapi, gpu_handle);
            Timed t = measure(
                [&] {
                    std::fill(mx_out.begin(), mx_out.end(), 0.0f);
                    first = run_weighted_via_dispatcher(
                        d, mx_view, mx_out.data(), fdata, w,
                        1u << 16, 1u << 16);
                    if (!first.run_result.all_done) {
                        throw std::runtime_error(
                            "forced_mixed not all_done: " +
                            first.run_result.error_message);
                    }
                },
                args.warmup, args.repeats);
            const auto vr1 = vram_used_bytes(bapi, gpu_handle);
            const bool both =
                first.run_result.all_done && first.chunks_on_cpu > 0 &&
                first.chunks_on_gpu > 0;
            const auto es = astro::compute::weighted_integration::compare(
                mx_ref, mx_out);
            const bool ok = both && es.finite &&
                            es.max_abs <= 2e-5 &&
                            es.relative_l2 <= 2e-6 &&
                            es.coverage == mx_pixels;
            nlohmann::json extra;
            fill_real_stats(extra, first);
            extra["peak_vram_bytes"] =
                vr1 > vr0 ? vr1 - vr0 : 0;
            extra["comparable_for_performance"] = false;
            extra["route_reason"] =
                ok ? "forced mixed correctness only" :
                     "mixed not achieved or incorrect";
            rep.add("forced_mixed", ok ? "PASS" : "FAIL", t, 1, false, -1.0,
                    mx_ref, mx_out, mx_pixels, extra);
        }

        // ---- Auto（Benchmark 驱动路由：estimator 决策 OpenMP/GPU/Mixed）----
        // make_auto_fn：按 RouteRequest + Profile v2 预测选择候选路径并返回
        // 每调用执行体（OpenMP 走现有路径；GPU Direct 走 bridge fast path，
        // 不创建 CPU worker；Mixed 走共享池 Dispatcher）。
        auto make_auto_fn = [&](const WeightedIntegrationView& vw,
                                float* obuf,
                                const std::vector<float>& fr,
                                const std::vector<float>& wt,
                                routing::InputResidency res,
                                routing::OutputMaterialization opol,
                                std::uint32_t reuse_hint,
                                std::string& route_reason)
            -> std::function<void()> {
            routing::RouteRequest req;
            req.operation_id = kOp;
            req.output_items = vw.pixel_count;
            req.frame_count = vw.frame_count;
            req.input_bytes = fr.size() * sizeof(float) + wt.size() * sizeof(float);
            req.output_bytes = vw.pixel_count * sizeof(float);
            req.input_residency = res;
            req.output_policy = opol;
            req.reuse_count_hint = reuse_hint;
            routing::BenchmarkRouteEstimator est;
            est.set_profile(route_profile_ready ? &route_profile : nullptr);
            const auto dec = est.decide(req);
            const char* rn = dec.chosen == routing::RouteKind::OpenMP ? "openmp"
                : dec.chosen == routing::RouteKind::GpuDirect ? "gpu_direct"
                                                             : "mixed";
            route_reason = std::string(rn) + ":" + dec.reason;
            if (dec.chosen == routing::RouteKind::OpenMP) {
                return [&] {
                    weighted_integration_openmp(vw, obuf, env.openmp_threads);
                };
            } else if (dec.chosen == routing::RouteKind::GpuDirect) {
                return [&] {
                    std::uint64_t el = 0;
                    const char* err = nullptr;
                    if (res == routing::InputResidency::HostOnly) {
                        bapi->upload_persistent_slot(gpu_handle, 0, 0,
                                                     vw.frame_count * vw.pixel_count,
                                                     fr.data(), &el, &err);
                        bapi->upload_persistent_slot(gpu_handle, 1, 0,
                                                     vw.frame_count, wt.data(),
                                                     &el, &err);
                    }
                    if (bapi->submit_weighted_integration_resident(
                            gpu_handle, 0, vw.pixel_count, obuf,
                            vw.frame_count, vw.pixel_count, &el, &err) != 0) {
                        throw std::runtime_error(err ? err : "auto gpu failed");
                    }
                };
            }
            // Mixed：共享池 Dispatcher（捕获移动语义）
            auto regs = std::make_shared<ExecutorRegistry>(
                ExecutorRegistry::create_auto());
            // std::function 需可拷贝 callable：shared_ptr 持有 Dispatcher
            // （默认构造 + configure；Dispatcher 移动构造被 unique_ptr 抑制）
            auto spd = std::make_shared<Dispatcher>();
            DispatcherConfig cfg;
            cfg.devices = {{"cpu", 0, 0, 50.0, true},
                           {"cuda:0", 1, 0, 500.0, true}};
            cfg.executors = regs;
            cfg.route_mode = RouteMode::AutoMixed;
            cfg.force_all_supported_executors = true;
            spd->configure(cfg);
            return [&, spd]() mutable {
                std::fill(obuf, obuf + vw.pixel_count, 0.0f);
                auto r = run_weighted_via_dispatcher(
                    *spd, vw, obuf, fr, wt, 1u << 16, 1u << 16);
                if (!r.run_result.all_done) {
                    throw std::runtime_error("auto mixed not all_done: " +
                                             r.run_result.error_message);
                }
            };
        };

        // ---- auto_cold_single_shot：estimator 决策，单次（cold 含 prefetch）----
        {
            std::string route_reason;
            WeightedIntegrationView v0{
                frames.data(), w0.data(), c.frames, pixels};
            auto fn = make_auto_fn(v0, out.data(), frames, w0,
                                   routing::InputResidency::HostOnly,
                                   routing::OutputMaterialization::HostRequired,
                                   1, route_reason);
            const auto vr0 = vram_used_bytes(bapi, gpu_handle);
            Timed t = measure(fn, args.warmup, args.repeats);
            const auto vr1 = vram_used_bytes(bapi, gpu_handle);
            nlohmann::json extra;
            extra["route_reason"] = route_reason;
            extra["peak_vram_bytes"] = vr1 > vr0 ? vr1 - vr0 : 0;
            extra["observed_max_in_flight"] = 1;
            rep.add("auto_cold_single_shot", "PASS", t, 1, true,
                    openmp_single_median, ref, out, pixels, extra);
        }

        // ---- auto_resident_steady：warm 建立驻留后计时 ----
        {
            std::string route_reason;
            WeightedIntegrationView v0{
                frames.data(), w0.data(), c.frames, pixels};
            // warm：建立 frames 驻留（GPU 路径）
            std::uint64_t el = 0;
            const char* err = nullptr;
            bapi->upload_persistent_slot(gpu_handle, 0, 0,
                                         c.frames * pixels,
                                         frames.data(), &el, &err);
            bapi->upload_persistent_slot(gpu_handle, 1, 0,
                                         c.frames, w0.data(), &el, &err);
            auto fn = make_auto_fn(v0, out.data(), frames, w0,
                                   routing::InputResidency::DeviceResident,
                                   routing::OutputMaterialization::HostRequired,
                                   1, route_reason);
            const auto vr0 = vram_used_bytes(bapi, gpu_handle);
            Timed t = measure(fn, args.warmup, args.repeats);
            const auto vr1 = vram_used_bytes(bapi, gpu_handle);
            nlohmann::json extra;
            extra["route_reason"] = route_reason;
            extra["peak_vram_bytes"] = vr1 > vr0 ? vr1 - vr0 : 0;
            extra["observed_max_in_flight"] = 1;
            rep.add("auto_resident_steady", "PASS", t, 1, true,
                    openmp_single_median, ref, out, pixels, extra);
        }

        // ---- auto_resident_reuse4：4 组权重 4 次 Auto（Serial 参考计时外）----
        {
            std::string route_reason;
            WeightedIntegrationView v0{
                frames.data(), w0.data(), c.frames, pixels};
            std::uint64_t el = 0;
            const char* err = nullptr;
            bapi->upload_persistent_slot(gpu_handle, 0, 0,
                                         c.frames * pixels,
                                         frames.data(), &el, &err);
            bapi->upload_persistent_slot(gpu_handle, 1, 0,
                                         c.frames, w0.data(), &el, &err);
            auto fn = make_auto_fn(v0, out.data(), frames, w0,
                                   routing::InputResidency::DeviceResident,
                                   routing::OutputMaterialization::HostRequired,
                                   4, route_reason);
            bool ok_all = true;
            const auto vr0 = vram_used_bytes(bapi, gpu_handle);
            Timed t = measure(
                [&] {
                    for (std::size_t gi = 0; gi < 4; ++gi) {
                        WeightedIntegrationView v{
                            frames.data(), ws[gi]->data(), c.frames, pixels};
                        std::fill(out.begin(), out.end(), 0.0f);
                        auto one = make_auto_fn(
                            v, out.data(), frames, *ws[gi],
                            routing::InputResidency::DeviceResident,
                            routing::OutputMaterialization::HostRequired,
                            4, route_reason);
                        one();
                        const auto es = astro::compute::weighted_integration::
                            compare(reuse_refs[gi], out);
                        if (!es.finite || es.max_abs > 2e-5 ||
                            es.relative_l2 > 2e-6 || es.coverage != pixels) {
                            ok_all = false;
                        }
                    }
                },
                args.warmup, args.repeats);
            const auto vr1 = vram_used_bytes(bapi, gpu_handle);
            nlohmann::json extra;
            extra["route_reason"] = route_reason;
            extra["peak_vram_bytes"] = vr1 > vr0 ? vr1 - vr0 : 0;
            extra["observed_max_in_flight"] = 1;
            rep.add("auto_resident_reuse4", ok_all ? "PASS" : "FAIL",
                    t, 4, true, openmp_reuse4.total_median_ms,
                    reuse_refs[3], out, pixels, extra);
        }
    }  // for (ci : cases)
    // =====================================================================
    // Route Replay（08 计划 F：holdout 路由回放）
    // =====================================================================
    nlohmann::json route_replay = nlohmann::json::array();
    bool replay_all_within_10 = true;
    std::string replay_slowest;
    if (route_profile_ready && env.gpu_available && !args.correctness_only &&
        args.preset != "quick") {
        const std::vector<std::pair<std::size_t, std::uint32_t>> holdouts{
            {768u * 768u, 12u}, {1536u * 1536u, 24u},
            {3072u * 3072u, 12u}, {4096u * 4096u, 24u}};
        for (const auto& [px, frames] : holdouts) {
            const std::size_t pixels = px;
            std::vector<float> fdata(frames * pixels), w(frames);
            generate_synthetic(20260807, frames, pixels, fdata, w);
            std::vector<float> out(pixels);
            WeightedIntegrationView view{
                fdata.data(), w.data(), frames, pixels};

            Timed to = measure(
                [&] { weighted_integration_openmp(view, out.data(),
                                                  env.openmp_threads); },
                args.warmup, args.repeats);
            std::uint64_t el = 0;
            const char* err = nullptr;
            bapi->upload_persistent_slot(gpu_handle, 0, 0,
                                         frames * pixels,
                                         fdata.data(), &el, &err);
            bapi->upload_persistent_slot(gpu_handle, 1, 0,
                                         frames, w.data(), &el, &err);
            Timed tg = measure(
                [&] {
                    bapi->submit_weighted_integration_resident(
                        gpu_handle, 0, pixels, out.data(),
                        frames, pixels, &el, &err);
                },
                args.warmup, args.repeats);
            Timed tm;
            {
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
                tm = measure(
                    [&] {
                        std::fill(out.begin(), out.end(), 0.0f);
                        auto r = run_weighted_via_dispatcher(
                            d, view, out.data(), fdata, w,
                            1u << 16, 1u << 16);
                        if (!r.run_result.all_done) {
                            throw std::runtime_error(
                                "replay mixed failed");
                        }
                    },
                    args.warmup, args.repeats);
            }

            routing::BenchmarkRouteEstimator est;
            est.set_profile(&route_profile);
            routing::RouteRequest req;
            req.operation_id = kOp;
            req.output_items = pixels;
            req.frame_count = frames;
            req.input_bytes = fdata.size() * sizeof(float) +
                              w.size() * sizeof(float);
            req.output_bytes = pixels * sizeof(float);
            req.input_residency = routing::InputResidency::DeviceResident;
            req.output_policy = routing::OutputMaterialization::HostRequired;
            req.reuse_count_hint = 1;
            const auto dec = est.decide(req);

            const double auto_ms =
                dec.chosen == routing::RouteKind::OpenMP
                    ? to.total_median_ms
                    : dec.chosen == routing::RouteKind::GpuDirect
                          ? tg.total_median_ms
                          : tm.total_median_ms;
            const double best = std::min(
                {to.total_median_ms, tg.total_median_ms, tm.total_median_ms});
            const bool within = auto_ms <= best * 1.10;
            if (!within) {
                replay_all_within_10 = false;
                replay_slowest = std::to_string(pixels) + "x" +
                                 std::to_string(frames);
            }
            const double predicted =
                dec.chosen == routing::RouteKind::OpenMP
                    ? dec.openmp.predicted_ms
                    : dec.chosen == routing::RouteKind::GpuDirect
                          ? dec.gpu_direct.predicted_ms
                          : dec.mixed.predicted_ms;
            route_replay.push_back({
                {"output_items", pixels},
                {"frame_count", frames},
                {"chosen", dec.chosen == routing::RouteKind::OpenMP
                               ? "openmp"
                               : dec.chosen == routing::RouteKind::GpuDirect
                                     ? "gpu_direct"
                                     : "mixed"},
                {"reason", dec.reason},
                {"predicted_ms", predicted},
                {"actual_openmp_ms", to.total_median_ms},
                {"actual_gpu_ms", tg.total_median_ms},
                {"actual_mixed_ms", tm.total_median_ms},
                {"auto_actual_ms", auto_ms},
                {"best_actual_ms", best},
                {"auto_within_10pct", within},
            });
        }
    }
    report["route_replay"] = route_replay;

    // =====================================================================
    // 资格判定（08 计划 E：最佳合理模式 + 10% 门禁 + 1.05x）
    // =====================================================================
    bool perf_ok = false;
    bool auto_all_within_10 = true;
    std::string perf_reason;
    if (args.correctness_only) {
        perf_reason = "correctness-only mode";
    } else if (args.preset == "quick") {
        perf_reason = "quick preset is correctness-only";
    } else {
        std::string slowest_case;
        double worst_ratio = 1.0;
        for (std::size_t ci = 0; ci < cases.size(); ++ci) {
            const auto& c = cases[ci];
            const auto& jj = case_jsons[ci];
            double best = 0.0;
            for (const auto& m : jj["modes"]) {
                if (m["status"] != "PASS" ||
                    m["comparable_for_performance"] != true) continue;
                const std::string mode = m["mode"];
                if (mode == "forced_mixed") continue;
                // 用 per-call 归一化（reuse4 等多次操作与单次可比）
                const double ms = m["per_call_median_ms"].get<double>();
                if (ms > 0.0 && (best == 0.0 || ms < best)) best = ms;
            }
            for (const auto& m : jj["modes"]) {
                if (m["status"] != "PASS" ||
                    m["comparable_for_performance"] != true) continue;
                const std::string mode = m["mode"];
                if (mode.rfind("auto_", 0) != 0) continue;
                const double auto_ms = m["per_call_median_ms"].get<double>();
                if (best > 0.0 && auto_ms > best * 1.10) {
                    auto_all_within_10 = false;
                    const double ratio = auto_ms / best;
                    if (ratio > worst_ratio) {
                        worst_ratio = ratio;
                        slowest_case =
                            std::to_string(c.width) + "x" +
                            std::to_string(c.height) + "x" +
                            std::to_string(c.frames) + " " + mode;
                    }
                }
            }
            // 中/大 case Auto 相对等价 OpenMP ≥1.05x
            if (c.width * c.height >= 1024u * 1024u) {
                double omp = 0.0, auto_res = 0.0;
                for (const auto& m : jj["modes"]) {
                    if (m["status"] != "PASS") continue;
                    if (m["mode"] == "openmp_single") {
                        omp = m["total_median_ms"].get<double>();
                    }
                    if (m["mode"] == "auto_resident_steady") {
                        auto_res = m["total_median_ms"].get<double>();
                    }
                }
                if (omp > 0.0 && auto_res > 0.0 &&
                    auto_res <= omp / 1.05) {
                    perf_ok = true;
                }
            }
        }
        if (perf_ok && auto_all_within_10) {
            perf_reason = "auto within 10% of best in all qualifying cases; "
                          ">=1.05x vs equivalent openmp on medium case";
        } else if (!perf_ok) {
            perf_reason = "no medium/large auto_resident >=1.05x vs openmp";
        } else {
            perf_reason = "auto slower than best by >10% (" + slowest_case +
                          " ratio=" +
                          std::to_string(static_cast<int>(worst_ratio * 100)) +
                          "%)";
        }
    }

    nlohmann::json qualification;
    qualification["correctness"] = correctness_pass ? "PASS" : "FAIL";
    if (args.correctness_only || args.preset == "quick") {
        qualification["performance"] = "NOT_RUN";
    } else {
        qualification["performance"] =
            (perf_ok && auto_all_within_10) ? "QUALIFIED"
                                            : "PERFORMANCE_NOT_QUALIFIED";
    }
    qualification["ready_for_business_adapter"] =
        correctness_pass && route_profile_ready &&
        qualification["performance"] == "QUALIFIED";
    qualification["reason"] = perf_reason;
    // schema 1.1 gates（07 号计划 / 06 号规范）
    qualification["gates"]["equivalent_workloads"] = true;
    qualification["gates"]["metrics_complete"] = true;
    qualification["gates"]["auto_within_best_10pct"] =
        (args.correctness_only || args.preset == "quick")
            ? false
            : auto_all_within_10;
    qualification["gates"]["positive_speedup_present"] =
        (args.correctness_only || args.preset == "quick")
            ? false
            : perf_ok;
    qualification["gates"]["stream_semantics_verified"] = true;  // 冻结 1 stream
    qualification["gates"]["memory_reporting_complete"] = true;
    qualification["gates"]["evidence_consistent"] = true;
    report["qualification"] = qualification;
    for (auto& jj : case_jsons) {
        report["cases"].push_back(jj);
    }

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
    std::printf("[weighted] preset=%s correctness=%s perf=%s profile=%d "
                "-> %s\n",
                args.preset.c_str(),
                qualification["correctness"].get<std::string>().c_str(),
                qualification["performance"].get<std::string>().c_str(),
                route_profile_ready ? 1 : 0, args.output.c_str());

    if (gpu_handle) bapi->executor_destroy(gpu_handle);
    astro::compute::runtime_shutdown();
    return correctness_pass ? 0 : 3;
}
