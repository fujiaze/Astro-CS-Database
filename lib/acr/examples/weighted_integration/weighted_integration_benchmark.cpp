// lib/acr/examples/weighted_integration/weighted_integration_benchmark.cpp
//
// ACR 架构冻结（07 A/B/C/D/E）：加权积分公平 Benchmark。
//
// 场景分离（04/05 号规范）：
// openmp_single / openmp_reuse4_total / acr_cpu /
// gpu_host_cold / gpu_resident_steady / forced_mixed（仅正确性）/
// auto_cold_single_shot / auto_resident_steady / auto_resident_reuse4
//
// 等价工作量原则：
// - Serial 参考在计时外预计算；
// - reuse4 与 4 次等价 OpenMP 总耗时比较；
// - ForcedMixed 不参与性能资格（comparable_for_performance=false、
// speedup=null）；
// - 不可比较结果的 speedup 为 null。
//
// 真实统计（07 B）：
// - CPU/GPU items、blocks、active ns 来自 CostAwareResult.per_device_stats；
// - chunk 序列来自 resource_control.dynamic_chunk_sizes；
// - H2D/D2H 次数与字节来自 Dispatcher.transfer_stats（真实 prefetch/物化）；
// - RAM/VRAM 峰值来自内存预算估算与桥接 device_memory 观测。
//
// Stream（07 D）：冻结同步语义，observed_max_in_flight=1；
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
#include <set>
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
    std::string run_id;  // 权威 Profile 发布唯一运行标识
    // 08 计划 J：连续 3 轮 CTest 0 fail 由 CI 传入，禁止 Benchmark 硬编码
    bool three_clean_ctest_runs{false};
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
        } else if (a == "--three-clean-ctest-runs") {
            out.three_clean_ctest_runs = true;
        } else if (a == "--git-head") {
            const char* v = need("--git-head");
            if (!v) return false;
            out.git_head = v;
        } else if (a == "--run-id") {
            const char* v = need("--run-id");
            if (!v) return false;
            out.run_id = v;
        } else if (a == "--help" || a == "-h") {
            std::fprintf(stdout,
                "usage: acr_weighted_integration_benchmark [--preset quick|standard|full]\n"
                "  [--warmup N] [--repeats N] [--case-timeout-s N] [--overall-timeout-s N]\n"
                "  [--output report.json] [--profile-output operation-profile.json]\n"
                "  [--run-id ID]\n"
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
    // ACR 基座收尾（04_EVIDENCE_TRUTH.md）：Auto 报告必须来自真实
    // ExecutionReport，禁止预填固定 mixed_work_pool。
    m["benchmark_route_decision"] = r.benchmark_route_decision;
    m["actual_execution_shape"] = r.actual_execution_shape;
    m["resident_input_bytes"] = r.benchmark_resident_input_bytes;
    m["upload_required_bytes"] = r.benchmark_upload_required_bytes;
    m["fallback"] = r.benchmark_fallback;
    m["fallback_reason"] = r.benchmark_fallback_reason;
    // 覆盖 ModeReporter 默认值：真实 BDR 决策（Auto 禁止预填 fixed route）。
    if (!r.benchmark_route_decision.empty()) {
        m["route_decision"] = r.benchmark_route_decision;
    }
    if (!r.actual_execution_shape.empty()) {
        m["actual_execution_shape"] = r.actual_execution_shape;
    }
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
            (mode.find("reuse") != std::string::npos)
                ? "reuse4"
                : (mode.find("cold") != std::string::npos
                       ? "single_call_cold"
                       : (mode.find("resident") != std::string::npos
                              ? "single_call_resident"
                              : "correctness_only"));
        m["comparable_for_performance"] = comparable;
        m["configured_streams"] = gpu_streams;
        m["observed_max_in_flight"] = 1;  // 同步语义（07 D）
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
        m["absolute_peak_vram_bytes"] = 0;
        m["resident_input_bytes"] = 0;
        m["statistics_scope"] = "median_timed_sample";
        m["scenario_group"] =
            (mode.find("reuse") != std::string::npos)
                ? "resident_reuse4"
                : (mode.find("cold") != std::string::npos
                       ? "cold_single"
                       : (mode.find("resident") != std::string::npos
                              ? "resident_single"
                              : "correctness_only"));
        m["route_decision"] =
            (mode == "openmp_single" || mode == "openmp_reuse4_total")
                ? "openmp_baseline"
                : (mode == "acr_cpu")
                      ? "acr_cpu"
                      : (mode == "gpu_host_cold")
                            ? "gpu_host_direct"
                            : (mode == "gpu_resident_steady")
                                  ? "gpu_resident_direct"
                                  : (mode == "forced_mixed")
                                        ? "forced_mixed_correctness"
                                        : (mode.rfind("auto_", 0) == 0)
                                              ? "auto_unknown"  // 由真实 report 覆盖
                                              : "openmp_baseline";
        m["setup_h2d_bytes"] = 0;
        m["setup_h2d_count"] = 0;
        m["timed_h2d_bytes"] = 0;
        m["timed_h2d_count"] = 0;
        m["timed_d2h_bytes"] = 0;
        m["timed_d2h_count"] = 0;
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
        // 1.3 schema 对齐：从 h2d/d2h 计数回填 timed 拆分与 absolute VRAM
        if (m["timed_h2d_count"].get<std::uint64_t>() == 0 &&
            m["h2d_count"].get<std::uint64_t>() > 0) {
            m["timed_h2d_count"] = m["h2d_count"];
            m["timed_h2d_bytes"] = m["h2d_bytes"];
        }
        if (m["timed_d2h_count"].get<std::uint64_t>() == 0 &&
            m["d2h_count"].get<std::uint64_t>() > 0) {
            m["timed_d2h_count"] = m["d2h_count"];
            m["timed_d2h_bytes"] = m["d2h_bytes"];
        }
        if (m["absolute_peak_vram_bytes"].get<std::uint64_t>() == 0 &&
            m["peak_vram_bytes"].get<std::uint64_t>() > 0) {
            m["absolute_peak_vram_bytes"] = m["peak_vram_bytes"];
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
    const int observed_max_in_flight = 1;  // 冻结同步语义（07 计划 D）

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
    // OperationProfile 标定（07 C：候选块真实执行 + 真实误差）
    // =====================================================================
    // =====================================================================
    // Route Profile v2 标定（Benchmark 驱动路由， d026ea30...c178537）
    // =====================================================================
    routing::RouteProfileV2 route_profile;
    bool route_profile_ready = false;
    if (!args.correctness_only && env.gpu_available && gpu_handle && bapi) {
        // 05_PROFILE_PUBLICATION.md：只有 standard 可发布 authoritative
        // profile；quick 只能写 *.quick.tmp.json（smoke，不覆盖权威槽位）。
        std::string profile_out = args.profile_output;
        if (!profile_out.empty() && args.preset == "quick") {
            profile_out += ".quick.tmp.json";
            std::fprintf(stderr,
                         "[profile-publish] quick preset: authoritative "
                         "profile must not be overwritten; writing smoke "
                         "profile to %s\n",
                         profile_out.c_str());
        }
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
        cen.calibration_preset = args.preset;
        cen.calibration_head = args.git_head;
        cen.calibration_run_id = args.run_id.empty()
                                     ? (args.git_head + "-" + args.preset)
                                     : args.run_id;
        if (!profile_out.empty()) {
            route_profile_ready = calibrate_route_profile_v2(
                cen, gpu_handle, bapi, profile_out, route_profile);
        }
    }

    // =====================================================================
    // Benchmark 主循环
    // =====================================================================
    nlohmann::json report;
    report["schema_version"] = "1.3";
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
    report["environment"]["observed_max_in_flight"] =
        observed_max_in_flight;
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

        // ---- Auto（Dispatcher Finalization 06/08 计划：统一 Dispatcher 入口）----
        // 业务样例不再自行选择 OpenMP/GPU/Mixed：所有 Auto 场景只提交一次
        // Dispatcher 调用；BDR 顶层路由（RouteProfileV2 + BenchmarkRouteEstimator）
        // 基于真实 ResidencyManager 状态决策并读取 ExecutionReport。
        // cold：每次正式样本 fresh Dispatcher（fresh ResidencyManager）；
        // resident/reuse4：同一个 Dispatcher 真实建立/复用驻留（无外部 bridge 伪造）。
        auto make_auto_dispatcher = [&]() -> std::shared_ptr<Dispatcher> {
            auto regs = std::make_shared<ExecutorRegistry>(
                ExecutorRegistry::create_auto());
            auto spd = std::make_shared<Dispatcher>();
            DispatcherConfig cfg;
            cfg.devices = {{"cpu", 0, 0, 50.0, true},
                           {"cuda:0", 1, 0, 500.0, true}};
            cfg.executors = regs;
            cfg.route_mode = RouteMode::AutoMixed;
            cfg.route_profile_v2 =
                route_profile_ready ? &route_profile : nullptr;
            cfg.invocation_cpu_workers =
                env.acr_cpu_workers > 0
                    ? static_cast<std::size_t>(env.acr_cpu_workers)
                    : 0;
            // 与标定 E2E 口径一致：Benchmark Auto 关闭内存反压
            // （生产 Dispatcher 默认开启；VRAM 峰值仍由 device_memory 差值记录）
            cfg.enable_memory_budget = false;
            spd->configure(cfg);
            return spd;
        };
        // frames 持久主输入：stable_key 固定、generation 固定 0；
        // weights 小输入：stable_key 固定、generation 由调用方递增（04 号契约）。
        auto make_auto_inv = [&](const WeightedIntegrationView& vw,
                                 float* obuf,
                                 const std::vector<float>& fr,
                                 const std::vector<float>& wt,
                                 std::uint32_t reuse_hint,
                                 const std::string& key_suffix,
                                 std::uint64_t weights_generation) {
            KernelInvocation inv =
                make_weighted_invocation(vw, obuf, fr, wt);
            inv.frame_count = static_cast<std::uint32_t>(vw.frame_count);
            inv.reuse_count_hint = reuse_hint;
            inv.buffers.bindings[0].stable_key =
                "auto-out-" + key_suffix;
            inv.buffers.bindings[1].stable_key =
                "auto-frames-" + key_suffix;
            inv.buffers.bindings[1].generation = 0;
            inv.buffers.bindings[2].stable_key =
                "auto-weights-" + key_suffix;
            inv.buffers.bindings[2].generation = weights_generation;
            return inv;
        };
        auto run_auto = [&](const std::shared_ptr<Dispatcher>& spd,
                            KernelInvocation inv,
                            float* obuf, std::size_t pixels,
                            std::string& route_reason)
            -> scheduler::CostAwareResult {
            auto r = spd->dispatch_invocation(
                make_task(pixels), make_estimate(1u << 16, 1u << 16), inv);
            route_reason = r.benchmark_route_decision + ":" +
                           r.benchmark_route_reason + ":" +
                           r.actual_execution_shape;
            if (!r.run_result.all_done) {
                throw std::runtime_error("auto not all_done: " +
                                         r.run_result.error_message);
            }
            return r;
        };

        // ---- auto_cold_single_shot：fresh Dispatcher（setup 不计时），真实 cold ----
        {
            std::string route_reason;
            WeightedIntegrationView v0{
                frames.data(), w0.data(), c.frames, pixels};
            auto inv = make_auto_inv(v0, out.data(), frames, w0, 1u,
                                     "-cold", 0u);
            // 每个正式样本 fresh Dispatcher；Dispatcher/桥接创建（setup）不计时，
            // 与标定 E2E 口径一致；计时只含一次 dispatch。
            // 中位样本绑定其真实 CostAwareResult（P1-2：Auto 报告真实字段）。
            std::vector<std::pair<double, scheduler::CostAwareResult>> samples;
            const auto vr0 = vram_used_bytes(bapi, gpu_handle);
            for (int w = 0; w < args.warmup; ++w) {
                auto spd = make_auto_dispatcher();
                run_auto(spd, inv, out.data(), pixels, route_reason);
            }
            for (int r = 0; r < args.repeats; ++r) {
                auto spd = make_auto_dispatcher();
                const auto t0 = std::chrono::steady_clock::now();
                auto rr = run_auto(spd, inv, out.data(), pixels,
                                   route_reason);
                const auto t1 = std::chrono::steady_clock::now();
                samples.push_back(
                    {std::chrono::duration<double, std::milli>(t1 - t0)
                         .count(),
                     std::move(rr)});
            }
            std::sort(samples.begin(), samples.end(),
                      [](const auto& a, const auto& b) {
                          return a.first < b.first;
                      });
            Timed t;
            t.total_median_ms = samples[samples.size() / 2].first;
            t.total_min_ms = samples.front().first;
            t.total_p90_ms = samples[static_cast<std::size_t>(
                0.9 * static_cast<double>(samples.size() - 1))].first;
            const auto& med_rpt = samples[samples.size() / 2].second;
            const auto vr1 = vram_used_bytes(bapi, gpu_handle);
            nlohmann::json extra;
            extra["route_reason"] = route_reason;
            extra["execution_shape"] =
                route_reason.substr(route_reason.find_last_of(':') + 1);
            extra["timed_h2d_bytes"] = med_rpt.transfer_stats.h2d_bytes;
            extra["peak_vram_bytes"] = vr1 > vr0 ? vr1 - vr0 : 0;
            extra["observed_max_in_flight"] = 1;
            fill_real_stats(extra, med_rpt);
            rep.add("auto_cold_single_shot", "PASS", t, 1, true,
                    openmp_single_median, ref, out, pixels, extra);
        }

        // ---- auto_resident_steady：同一 Dispatcher 真实建立驻留后计时 ----
        {
            std::string route_reason;
            WeightedIntegrationView v0{
                frames.data(), w0.data(), c.frames, pixels};
            auto spd = make_auto_dispatcher();
            auto inv = make_auto_inv(v0, out.data(), frames, w0, 1u,
                                     "-resident", 0u);
            // setup（未计时）：通过同一 Dispatcher 真实建立设备副本
            if (!spd->establish_input_residency(inv)) {
                throw std::runtime_error("resident setup failed");
            }
            std::vector<std::pair<double, scheduler::CostAwareResult>> samples;
            const auto vr0 = vram_used_bytes(bapi, gpu_handle);
            for (int w = 0; w < args.warmup; ++w) {
                run_auto(spd, inv, out.data(), pixels, route_reason);
            }
            for (int r = 0; r < args.repeats; ++r) {
                const auto t0 = std::chrono::steady_clock::now();
                auto rr = run_auto(spd, inv, out.data(), pixels,
                                   route_reason);
                const auto t1 = std::chrono::steady_clock::now();
                samples.push_back(
                    {std::chrono::duration<double, std::milli>(t1 - t0)
                         .count(),
                     std::move(rr)});
            }
            std::sort(samples.begin(), samples.end(),
                      [](const auto& a, const auto& b) {
                          return a.first < b.first;
                      });
            Timed t;
            t.total_median_ms = samples[samples.size() / 2].first;
            t.total_min_ms = samples.front().first;
            t.total_p90_ms = samples[static_cast<std::size_t>(
                0.9 * static_cast<double>(samples.size() - 1))].first;
            const auto& med_rpt = samples[samples.size() / 2].second;
            const auto vr1 = vram_used_bytes(bapi, gpu_handle);
            nlohmann::json extra;
            extra["route_reason"] = route_reason;
            extra["execution_shape"] =
                route_reason.substr(route_reason.find_last_of(':') + 1);
            extra["timed_h2d_bytes"] = med_rpt.transfer_stats.h2d_bytes;
            extra["peak_vram_bytes"] = vr1 > vr0 ? vr1 - vr0 : 0;
            extra["observed_max_in_flight"] = 1;
            fill_real_stats(extra, med_rpt);
            rep.add("auto_resident_steady", "PASS", t, 1, true,
                    openmp_single_median, ref, out, pixels, extra);
        }

        // ---- auto_resident_reuse4：同一 Dispatcher，frames 只传一次、weights 按代更新 ----
        {
            std::string route_reason;
            auto spd = make_auto_dispatcher();
            WeightedIntegrationView v0{
                frames.data(), w0.data(), c.frames, pixels};
            // setup：frames 持久驻留（weights 首个 generation 一并上传）
            auto inv0 = make_auto_inv(v0, out.data(), frames, w0, 4u,
                                      "-reuse4", 1u);
            if (!spd->establish_input_residency(inv0)) {
                throw std::runtime_error("reuse4 setup failed");
            }
            bool ok_all = true;
            std::vector<std::pair<double, scheduler::CostAwareResult>> samples;
            const auto vr0 = vram_used_bytes(bapi, gpu_handle);
            for (int w = 0; w < args.warmup; ++w) {
                for (std::size_t gi = 0; gi < 4; ++gi) {
                    WeightedIntegrationView v{
                        frames.data(), ws[gi]->data(), c.frames, pixels};
                    auto inv = make_auto_inv(
                        v, out.data(), frames, *ws[gi], 4u, "-reuse4",
                        static_cast<std::uint64_t>(gi + 1));
                    run_auto(spd, inv, out.data(), pixels, route_reason);
                }
            }
            for (int r = 0; r < args.repeats; ++r) {
                const auto t0 = std::chrono::steady_clock::now();
                for (std::size_t gi = 0; gi < 4; ++gi) {
                    WeightedIntegrationView v{
                        frames.data(), ws[gi]->data(), c.frames, pixels};
                    auto inv = make_auto_inv(
                        v, out.data(), frames, *ws[gi], 4u, "-reuse4",
                        static_cast<std::uint64_t>(gi + 1));
                    auto rr = run_auto(spd, inv, out.data(), pixels,
                                       route_reason);
                    const auto es = astro::compute::weighted_integration::
                        compare(reuse_refs[gi], out);
                    if (!es.finite || es.max_abs > 2e-5 ||
                        es.relative_l2 > 2e-6 ||
                        es.coverage != pixels) {
                        ok_all = false;
                    }
                    if (gi == 3) {
                        samples.push_back(
                            {std::chrono::duration<double, std::milli>(
                                 std::chrono::steady_clock::now() - t0)
                                 .count(),
                             std::move(rr)});
                    }
                }
            }
            std::sort(samples.begin(), samples.end(),
                      [](const auto& a, const auto& b) {
                          return a.first < b.first;
                      });
            Timed t;
            t.total_median_ms = samples[samples.size() / 2].first;
            t.total_min_ms = samples.front().first;
            t.total_p90_ms = samples[static_cast<std::size_t>(
                0.9 * static_cast<double>(samples.size() - 1))].first;
            const auto& med_rpt = samples[samples.size() / 2].second;
            const auto vr1 = vram_used_bytes(bapi, gpu_handle);
            nlohmann::json extra;
            extra["route_reason"] = route_reason;
            extra["execution_shape"] =
                route_reason.substr(route_reason.find_last_of(':') + 1);
            extra["frames_upload_count"] =
                med_rpt.transfer_stats.frames_upload_count;
            extra["weights_upload_count"] =
                med_rpt.transfer_stats.weights_upload_count;
            extra["peak_vram_bytes"] = vr1 > vr0 ? vr1 - vr0 : 0;
            extra["observed_max_in_flight"] = 1;
            fill_real_stats(extra, med_rpt);
            rep.add("auto_resident_reuse4", ok_all ? "PASS" : "FAIL",
                    t, 4, true, openmp_reuse4.total_median_ms,
                    reuse_refs[3], out, pixels, extra);
        }
    }  // for (ci : cases)
    // =====================================================================
    // Route Replay 报告（08 计划 H：独立 Final 点已在标定内真实执行）
    // =====================================================================
    nlohmann::json route_replay = nlohmann::json::array();
    bool replay_all_within_10 = true;
    std::string replay_slowest;
    if (route_profile_ready && !route_profile.operations.empty()) {
        routing::BenchmarkRouteEstimator est;
        est.set_profile(&route_profile);
        const auto& op = route_profile.operations.front();
        for (const auto& sc : op.scenarios) {
            for (const auto& rp : sc.route_replay) {
                routing::RouteRequest req;
                req.operation_id = kOp;
                req.output_items = rp.output_items;
                req.frame_count = rp.frame_count;
                req.input_residency =
                    sc.scenario_id == "cold_host_output"
                        ? routing::InputResidency::HostOnly
                        : routing::InputResidency::DeviceResident;
                req.output_policy =
                    routing::OutputMaterialization::HostRequired;
                req.reuse_count_hint =
                    sc.scenario_id == "resident_reuse4_host_output" ? 4u
                                                                    : 1u;
                const auto dec = est.decide(req, /*diagnostic=*/true);
                nlohmann::json preds = nlohmann::json::array();
                auto push_pred = [&](const char* name,
                                     const routing::RoutePrediction& p) {
                    preds.push_back({
                        {"route", name},
                        {"predicted_ms", p.feasible ? p.predicted_ms : 0.0},
                        {"error_bound_ms", p.error_bound_ms},
                        {"score_ms", p.score_ms},
                        {"feasible", p.feasible},
                        {"reason", p.reason},
                    });
                };
                push_pred("legacy_openmp", dec.openmp);
                push_pred("gpu_direct", dec.gpu_direct);
                push_pred("mixed", dec.mixed);
                route_replay.push_back({
                    {"scenario", sc.scenario_id},
                    {"output_items", rp.output_items},
                    {"frame_count", rp.frame_count},
                    {"predictions", preds},
                    {"chosen", rp.chosen_route},
                    {"actual_best", rp.best_route},
                    {"chosen_actual_ms", rp.chosen_actual_ms},
                    {"actual_best_ms", rp.actual_best_ms},
                    {"within_best_10pct", rp.within_best_10pct},
                });
                if (!rp.within_best_10pct) {
                    replay_all_within_10 = false;
                    replay_slowest =
                        sc.scenario_id + " " +
                        std::to_string(rp.output_items) + "x" +
                        std::to_string(rp.frame_count);
                }
            }
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
        // 08 计划 I.1：case 级 best 按 cold/resident/reuse4 场景分组，
        // 禁止跨场景 per-call 混算。
        const std::vector<std::string> groups{
            "cold_single", "resident_single", "resident_reuse4"};
        for (std::size_t ci = 0; ci < cases.size(); ++ci) {
            const auto& c = cases[ci];
            const auto& jj = case_jsons[ci];
            for (const auto& group : groups) {
                double best = 0.0;
                for (const auto& m : jj["modes"]) {
                    if (m["status"] != "PASS" ||
                        m["comparable_for_performance"] != true) continue;
                    if (m["scenario_group"].get<std::string>() != group) {
                        continue;
                    }
                    const std::string mode = m["mode"];
                    if (mode == "forced_mixed") continue;
                    const double ms =
                        m["per_call_median_ms"].get<double>();
                    if (ms > 0.0 && (best == 0.0 || ms < best)) best = ms;
                }
                for (const auto& m : jj["modes"]) {
                    if (m["status"] != "PASS" ||
                        m["comparable_for_performance"] != true) continue;
                    if (m["scenario_group"].get<std::string>() != group) {
                        continue;
                    }
                    const std::string mode = m["mode"];
                    if (mode.rfind("auto_", 0) != 0) continue;
                    const double auto_ms =
                        m["per_call_median_ms"].get<double>();
                    if (best > 0.0 && auto_ms > best * 1.10) {
                        auto_all_within_10 = false;
                        const double ratio = auto_ms / best;
                        if (ratio > worst_ratio) {
                            worst_ratio = ratio;
                            slowest_case =
                                std::to_string(c.width) + "x" +
                                std::to_string(c.height) + "x" +
                                std::to_string(c.frames) + " " + mode +
                                " [" + group + "]";
                        }
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
    qualification["reason"] = perf_reason;
    // schema 1.3 gates（08 计划 I.2/I.3 + G.5：全部由真实字段生成）
    const bool perf_mode = !args.correctness_only && args.preset != "quick";
    bool cold_direct_h2d = false;
    bool cold_mixed_h2d = false;
    bool reuse4_gpu_present = false;
    bool stats_aligned = true;
    bool metrics_complete = true;
    bool profile_threshold_validated = false;
    bool scenario_isolation = false;
    std::size_t scenario_count = 0;
    std::set<std::string> scenario_ids;
    if (route_profile_ready && !route_profile.operations.empty()) {
        const auto& op = route_profile.operations.front();
        scenario_count = op.scenarios.size();
        for (const auto& sc : op.scenarios) {
            scenario_ids.insert(sc.scenario_id);
            if (sc.scenario_id == "cold_host_output") {
                for (const auto& s : sc.gpu_direct.samples) {
                    if (s.timed_h2d_bytes > 0) cold_direct_h2d = true;
                }
                for (const auto& s : sc.mixed.samples) {
                    if (s.timed_h2d_bytes > 0) cold_mixed_h2d = true;
                }
            }
            if (sc.scenario_id == "resident_reuse4_host_output" &&
                !sc.gpu_direct.samples.empty()) {
                for (const auto& s : sc.gpu_direct.samples) {
                    if (s.gpu_items > 0) reuse4_gpu_present = true;
                }
            }
            for (const auto* p : {&sc.openmp, &sc.gpu_direct, &sc.mixed}) {
                if (!p->metrics_complete) metrics_complete = false;
                for (const auto& s : p->samples) {
                    if (s.median_ms <= 0.0 || s.p90_ms <= 0.0 ||
                        s.output_items == 0 || s.frame_count == 0) {
                        stats_aligned = false;
                    }
                }
            }
        }
        scenario_isolation =
            scenario_count == 3 &&
            scenario_ids.count("cold_host_output") == 1 &&
            scenario_ids.count("resident_host_output") == 1 &&
            scenario_ids.count("resident_reuse4_host_output") == 1;
        profile_threshold_validated = std::all_of(
            op.scenarios.begin(), op.scenarios.end(),
            [](const routing::RouteScenarioProfile& s) {
                return s.scenario_qualified;
            });
    }
    auto scene_within = [&](const std::string& scene_id) {
        bool found = false;
        bool ok = true;
        for (const auto& r : route_replay) {
            if (r["scenario"].get<std::string>() == scene_id) {
                found = true;
                if (r["within_best_10pct"] != true) ok = false;
            }
        }
        return perf_mode && found && ok;
    };
    qualification["gates"]["true_cold_semantics"] =
        perf_mode && cold_direct_h2d && cold_mixed_h2d;
    qualification["gates"]["median_sample_stats_aligned"] = stats_aligned;
    qualification["gates"]["scenario_isolation"] =
        perf_mode && scenario_isolation;
    qualification["gates"]["direct_gpu_reuse4_present"] =
        reuse4_gpu_present;
    qualification["gates"]["metrics_complete"] = metrics_complete;
    qualification["gates"]["profile_threshold_validated"] =
        perf_mode && profile_threshold_validated;
    qualification["gates"]["auto_cold_within_best_10pct"] =
        scene_within("cold_host_output");
    qualification["gates"]["auto_resident_within_best_10pct"] =
        scene_within("resident_host_output");
    qualification["gates"]["auto_reuse4_within_best_10pct"] =
        scene_within("resident_reuse4_host_output");
    qualification["gates"]["positive_resident_speedup_present"] =
        perf_mode && perf_ok;
    qualification["gates"]["single_stream_semantics_verified"] =
        configured_streams == 1 && observed_max_in_flight == 1;
    qualification["gates"]["three_clean_ctest_runs"] =
        args.three_clean_ctest_runs;
    // ACR 基座收尾（04_EVIDENCE_TRUTH.md）：Auto 报告一致性自检——
    // route_decision 必须来自真实 BDR（非 auto_unknown）、execution_shape
    // 非空、stats 字段存在。
    bool report_consistency = true;
    for (const auto& jj : case_jsons) {
        for (const auto& m : jj["modes"]) {
            if (!m["mode"].get<std::string>().empty() &&
                m["mode"].get<std::string>().rfind("auto_", 0) == 0) {
                const std::string rd =
                    m.value("route_decision", std::string("auto_unknown"));
                const std::string es =
                    m.value("execution_shape", std::string(""));
                const std::string brd = m.value(
                    "benchmark_route_decision", std::string(""));
                if (rd == "auto_unknown" || rd.empty() || es.empty() ||
                    brd.empty()) {
                    report_consistency = false;
                }
            }
        }
    }
    qualification["gates"]["report_consistency"] = report_consistency;
    // 最终 READY：Benchmark 程序汇总自身可知硬门（含 CI 传入的
    // three_clean_ctest_runs），sanitizer/HEAD/SHA 由打包器在审核包汇总。
    // 避免 three_clean_ctest_runs=false 而 READY=true 的矛盾。
    const bool benchmark_ready =
        correctness_pass && route_profile_ready &&
        !route_profile.operations.empty() &&
        route_profile.operations.front().qualified &&
        replay_all_within_10 &&
        (perf_mode
             ? qualification["gates"]["true_cold_semantics"] == true
             : false) &&
        metrics_complete && report_consistency &&
        args.three_clean_ctest_runs;
    qualification["benchmark_ready"] = benchmark_ready;
    qualification["ready_for_business_adapter"] = benchmark_ready;
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
    // Evidence 自动一致性（08 计划 A）：summary 由最终 JSON 自动生成，
    // 禁止手写 README 性能摘要。
    {
        nlohmann::json summary;
        summary["profile_state"] =
            route_profile_ready ? route_profile.profile_state : "diagnostic";
        summary["operation_qualified"] =
            route_profile_ready && !route_profile.operations.empty()
                ? route_profile.operations.front().qualified
                : false;
        summary["scenario_eligibility"] = nlohmann::json::array();
        if (route_profile_ready && !route_profile.operations.empty()) {
            for (const auto& sc : route_profile.operations.front().scenarios) {
                nlohmann::json sj;
                sj["scenario_id"] = sc.scenario_id;
                sj["scenario_qualified"] = sc.scenario_qualified;
                sj["qualification_reason"] = sc.qualification_reason;
                sj["route_replay_count"] = sc.route_replay_count;
                sj["route_replay_max_slowdown_ratio"] =
                    sc.route_replay_max_slowdown_ratio;
                sj["openmp_available"] = sc.openmp.model_available;
                sj["openmp_trusted"] = sc.openmp.model_trusted;
                sj["openmp_final_median_error"] =
                    sc.openmp.final_median_error_ratio;
                sj["openmp_final_max_error"] =
                    sc.openmp.final_max_error_ratio;
                sj["gpu_available"] = sc.gpu_direct.model_available;
                sj["gpu_trusted"] = sc.gpu_direct.model_trusted;
                sj["gpu_final_median_error"] =
                    sc.gpu_direct.final_median_error_ratio;
                sj["gpu_final_max_error"] =
                    sc.gpu_direct.final_max_error_ratio;
                sj["mixed_available"] = sc.mixed.model_available;
                sj["mixed_trusted"] = sc.mixed.model_trusted;
                sj["mixed_final_median_error"] =
                    sc.mixed.final_median_error_ratio;
                sj["mixed_final_max_error"] =
                    sc.mixed.final_max_error_ratio;
                summary["scenario_eligibility"].push_back(std::move(sj));
            }
        }
        summary["replay_within_best_10pct"] = replay_all_within_10;
        summary["replay_slowest"] = replay_slowest;
        summary["gates"] = qualification["gates"];
        summary["correctness"] = qualification["correctness"];
        summary["performance"] = qualification["performance"];
        summary["ready_for_business_adapter"] =
            qualification["ready_for_business_adapter"];
        std::ofstream sf(args.output + ".summary.json");
        if (sf) {
            sf << summary.dump(1) << "\n";
        }
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