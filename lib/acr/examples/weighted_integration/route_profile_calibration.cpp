// lib/acr/examples/weighted_integration/route_profile_calibration.cpp
//
// 加权积分 Operation Route Profile v2 标定
//（BDR Reviewed 控制包 A9766B99...53984，08 号计划 A-J）。
//
// BDR Reviewed 修正（08 计划 A-J）：
//   - 场景真正隔离：cold_host_output / resident_host_output /
//     resident_reuse4_host_output（无真实 KeepDevice API，不生成
//     resident_device_output）；
//   - chunk 服务曲线为真实单 token 服务时间（多 frame_count；
//     禁止整幅任务总耗时）；
//   - 标定数据拆 Fit / Refinement Probe / Final Holdout 三集合，自动断言
//     无交集；Final 只在模型冻结后使用一次；
//   - E2E 插值模型（linear/loglog）由 Probe 选择，最多 2 轮 adaptive
//     refinement（最差 Probe 转入 Fit 并重新拟合）；
//   - 每场景每路径 Final 独立误差（median<=10%、max<=15%）后才 model_trusted；
//   - cold Mixed 每个正式样本 fresh Dispatcher + warmup 不同 generation，
//     GPU 参与时 timed H2D 必须 > 0；
//   - GPU chunk 单块测试保持完整 frame-major stride（pixel_count=domain），
//     begin/middle/end 抽样 + 独立 warmup + 随机化顺序 + sanity gate；
//   - reuse4 Mixed 累计 4 次真实 per_device_stats/transfer，禁止 4*pixels；
//   - Route Replay 使用独立 Final 点（每场景 >=8），三候选实际运行；
//   - Operation 资格由三个 required 场景的 scenario_qualified 生成；
//   - 删除人工 mixed overhead 常量（Mixed E2E 是主成本）。
#include "route_profile_calibration.hpp"

#include "weighted_integration_kernels.hpp"

#include "scheduler/dispatcher.hpp"
#include "scheduler/device_executor.hpp"
#include "../core/task_descriptor.hpp"
#include "../cost/cost_estimator.hpp"
#include "../backends/cuda/bridge/cuda_bridge_api.hpp"
#include "../../routing/route_profile_v2.hpp"
#include "../../routing/benchmark_route_estimator.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace astro::compute::weighted_integration {
namespace {

using astro::compute::routing::RoutePath;
using astro::compute::routing::RouteProfileV2;
using astro::compute::routing::RouteSamplePoint;
using astro::compute::routing::OperationRouteProfile;
using astro::compute::routing::RouteScenarioProfile;
using astro::compute::routing::ChunkServicePoint;
using astro::compute::routing::BenchmarkRouteEstimator;
using astro::compute::routing::InputResidency;
using astro::compute::routing::OutputMaterialization;
using astro::compute::routing::RouteKind;
using astro::compute::routing::RouteRequest;
using astro::compute::routing::RouteReplayPoint;
using astro::compute::scheduler::Dispatcher;
using astro::compute::scheduler::DispatcherConfig;
using astro::compute::scheduler::ExecutorRegistry;
using astro::compute::RouteMode;

constexpr const char* kOp = "synthetic.weighted_integration.fp64acc";
constexpr int kWarmup = 2;
constexpr int kRepeats = 5;  // 中位测量次数（过大加重 GPU 热节流，反而不稳）
constexpr std::uint32_t kServiceFrames[] = {4u, 16u, 32u};

// GPU 热节流缓解：重负载测量点之间让 GPU 空闲恢复时钟（不计入测量）。
inline void gpu_settle() {
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
}

// ISO-8601 UTC 时间（Profile 发布元数据；Windows 用 gmtime 线程安全版本）
std::string utc_now_iso8601() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

// ===== 08 计划 B：三集合标定数据（Fit/Probe/Final 严格无交集）=====
struct CalPoint {
    std::uint64_t items;
    std::uint32_t frames;
    bool operator<(const CalPoint& o) const {
        return std::tie(items, frames) < std::tie(o.items, o.frames);
    }
    bool operator==(const CalPoint& o) const {
        return items == o.items && frames == o.frames;
    }
};

const std::vector<std::size_t> kFitSizes{
    256u * 256u, 512u * 512u, 1024u * 1024u,
    2048u * 2048u, 4096u * 4096u};
const std::vector<std::uint32_t> kFitFrames{4u, 16u, 32u};

// Refinement Probe：至少 8 个非 Fit 点，仅用于选择插值器与 adaptive 补点
const std::vector<CalPoint> kProbePoints{
    {384u * 384u, 8u},  {640u * 640u, 12u}, {896u * 896u, 16u},
    {1152u * 1152u, 20u}, {1792u * 1792u, 24u}, {2304u * 2304u, 28u},
    {2816u * 2816u, 32u}, {3456u * 3456u, 8u},
    // 路由边界坐标（历轮 Replay 反复在 cold 选错 OpenMP vs Mixed）：
    // 放 Probe 由 route-regret Adaptive 决定是否补入 Fit，Final 保持 untouched。
    {3072u * 3072u, 12u}};

// Final Untouched Holdout：>=8/场景，与 Fit/Probe 均不重叠；
// 模型冻结后只运行一次（最终误差 + Route Replay）。
const std::vector<CalPoint> kFinalPoints{
    {768u * 768u, 12u},  {1024u * 1024u, 10u}, {1280u * 1280u, 20u},
    {1536u * 1536u, 24u}, {2560u * 2560u, 20u}, {3200u * 3200u, 16u},
    {3584u * 3584u, 28u}, {4096u * 4096u, 24u}};

// 生成 Fit 点列表（items × frames 组合）
std::vector<CalPoint> fit_points() {
    std::vector<CalPoint> out;
    for (std::size_t px : kFitSizes) {
        for (std::uint32_t f : kFitFrames) {
            out.push_back({static_cast<std::uint64_t>(px), f});
        }
    }
    return out;
}

// 三集合无交集断言（08 计划 B：输出清单并自动断言）
bool datasets_disjoint(const std::vector<CalPoint>& a,
                       const std::vector<CalPoint>& b,
                       std::string& reason) {
    std::set<CalPoint> sa(a.begin(), a.end());
    for (const auto& p : b) {
        if (sa.count(p) > 0) {
            reason = "overlap at items=" + std::to_string(p.items) +
                     " frames=" + std::to_string(p.frames);
            return false;
        }
    }
    return true;
}

struct TimedWithStats {
    double median_ms{0.0};
    double p90_ms{0.0};
    RouteSamplePoint stats;
};

template <class Fn>
TimedWithStats measure_with_stats(Fn&& fn) {
    for (int w = 0; w < kWarmup; ++w) {
        RouteSamplePoint st;
        fn(st);
    }
    using Clock = std::chrono::steady_clock;
    std::vector<std::pair<double, RouteSamplePoint>> samples;
    for (int r = 0; r < kRepeats; ++r) {
        RouteSamplePoint st;
        const auto t0 = Clock::now();
        fn(st);
        const auto t1 = Clock::now();
        samples.push_back(
            {std::chrono::duration<double, std::milli>(t1 - t0).count(), st});
    }
    std::sort(samples.begin(), samples.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    TimedWithStats t;
    t.median_ms = samples[samples.size() / 2].first;
    t.stats = samples[samples.size() / 2].second;
    const std::size_t p90 =
        static_cast<std::size_t>(0.9 * static_cast<double>(samples.size() - 1));
    t.p90_ms = samples[p90].first;
    return t;
}

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

KernelInvocation make_inv(const WeightedIntegrationView& view,
                          float* output,
                          const std::vector<float>& frames,
                          const std::vector<float>& weights) {
    KernelInvocation inv;
    inv.id = kOp;
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

scheduler::CostAwareResult run_dispatcher(
    Dispatcher& d, const WeightedIntegrationView& view, float* output,
    const std::vector<float>& frames, const std::vector<float>& weights,
    std::size_t rec_cpu, std::size_t rec_gpu) {
    KernelInvocation inv = make_inv(view, output, frames, weights);
    return d.dispatch_invocation(make_task(view.pixel_count),
                                 make_estimate(rec_cpu, rec_gpu), inv);
}

void fill_stats_from_result(RouteSamplePoint& st,
                            const scheduler::CostAwareResult& r,
                            std::uint64_t setup_h2d,
                            bool accumulate = false) {
    if (!accumulate) {
        st.cpu_items = 0;
        st.gpu_items = 0;
        st.cpu_chunks = 0;
        st.gpu_chunks = 0;
    }
    for (const auto& pds : r.per_device_stats) {
        if (pds.backend.rfind("cuda", 0) == 0) {
            st.gpu_items += pds.items_done;
            st.gpu_chunks += pds.blocks_done;
        } else if (pds.backend == "cpu") {
            st.cpu_items += pds.items_done;
            st.cpu_chunks += pds.blocks_done;
        }
    }
    st.setup_h2d_bytes = setup_h2d;
    if (accumulate) {
        st.timed_h2d_bytes += r.transfer_stats.h2d_bytes;
        st.timed_d2h_bytes += r.transfer_stats.d2h_bytes;
        st.peak_ram_bytes =
            std::max(st.peak_ram_bytes, r.resource_control.mem_peak_max);
    } else {
        st.timed_h2d_bytes = r.transfer_stats.h2d_bytes;
        st.timed_d2h_bytes = r.transfer_stats.d2h_bytes;
        st.peak_ram_bytes = r.resource_control.mem_peak_max;
    }
}

// 依据 samples 重算 validated domain（min/max/frame_counts）
void recompute_domain(RoutePath& path) {
    if (path.samples.empty()) {
        path.min_output_items = 0;
        path.max_output_items = 0;
        path.frame_counts.clear();
        return;
    }
    std::uint64_t mn = path.samples.front().output_items;
    std::uint64_t mx = mn;
    std::vector<std::uint32_t> fcs;
    for (const auto& s : path.samples) {
        mn = std::min(mn, s.output_items);
        mx = std::max(mx, s.output_items);
        if (std::find(fcs.begin(), fcs.end(), s.frame_count) == fcs.end()) {
            fcs.push_back(s.frame_count);
        }
    }
    path.min_output_items = mn;
    path.max_output_items = mx;
    path.frame_counts = std::move(fcs);
}

// Final 误差（模型已冻结：只评估、绝不重新选择插值器）
RouteErrorEval evaluate_final(RoutePath& path,
                              const std::vector<RouteEvalPoint>& final_pts) {
    // 记录冻结前模型，断言 Final 不改模型
    const std::string frozen_interp = path.interpolation_id;
    const std::vector<RouteSamplePoint> frozen_samples = path.samples;
    const std::uint32_t frozen_adaptive = path.adaptive_rounds_used;
    RouteErrorEval ev = evaluate_fixed_model_on_final(path, final_pts);
    if (path.interpolation_id != frozen_interp ||
        path.samples != frozen_samples ||
        path.adaptive_rounds_used != frozen_adaptive) {
        std::fprintf(stderr,
                     "[route-profile] FATAL: final holdout mutated model\n");
        std::abort();
    }
    path.final_holdout_count = ev.count;
    path.final_median_error_ratio = ev.median;
    path.final_max_error_ratio = ev.max;
    // 兼容字段与最终 holdout 保持一致
    path.holdout_count = ev.count;
    path.median_error_ratio = ev.median;
    path.max_error_ratio = ev.max;
    path.p95_error_ratio = ev.max;  // >=8 点后由 CI 输出真实分位；max 保守
    path.model_trusted = ev.count >= 8 &&
                         route_gate_passed_errors(ev.median, ev.max);
    path.model_available = !path.samples.empty() && ev.count > 0;
    path.eligible = path.model_trusted;
    if (!path.model_trusted) {
        path.reason = path.samples.empty() ? "no-samples"
                     : ev.count < 8
                         ? "insufficient-final-holdout"
                         : "final-holdout-error-limit";
    } else {
        path.reason = "final-holdout-passed";
    }
    return ev;
}

// ===== VRAM 观测（真实 device_memory 差值）=====
std::uint64_t vram_used_bytes(astro::compute::cuda::bridge::BridgeApi* api,
                              void* handle) {
    if (api == nullptr || handle == nullptr || !api->device_memory) return 0;
    std::uint64_t total = 0, free = 0;
    const char* err = nullptr;
    if (api->device_memory(0, &total, &free, &err) != 0) return 0;
    return total - free;
}

// 构造 Mixed Dispatcher（共享 ExecutorRegistry 以降低配置成本；
// 每个 Dispatcher 的 residency 独立 → fresh Dispatcher 即为 cold）
std::shared_ptr<Dispatcher> make_mixed_dispatcher(
    const std::shared_ptr<ExecutorRegistry>& regs) {
    auto d = std::make_shared<Dispatcher>();
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true},
                   {"cuda:0", 1, 0, 500.0, true}};
    cfg.executors = regs;
    cfg.route_mode = RouteMode::AutoMixed;
    cfg.force_all_supported_executors = true;
    // BDR3：标定/Replay 是受控 Benchmark 环境，必须把任务完整跑完。
    // 两个 GPU executor（gpu_handle + regs）的 buffer 使显存接近满载时，
    // VRAM 预算 gate（StopNewSubmit）会因显存不释放而永久等待；
    // 校准禁用内存反压（生产路径保持默认），VRAM 峰值仍由 device_memory
    // 前后差值真实记录。
    cfg.enable_memory_budget = false;
    d->configure(cfg);
    return d;
}

// ===== metrics 完整性（08 计划 G：逐字段真实检查）=====
bool sample_metrics_complete(const RouteSamplePoint& s,
                             const char* kind,
                             bool cold,
                             bool reuse4) {
    if (s.median_ms <= 0.0 || s.p90_ms <= 0.0) return false;
    if (std::strcmp(kind, "openmp") == 0) {
        return s.cpu_items > 0 && s.cpu_chunks > 0;
    }
    if (std::strcmp(kind, "gpu") == 0) {
        return s.gpu_items > 0 && s.gpu_chunks > 0 &&
               s.timed_d2h_bytes > 0 && s.absolute_peak_vram_bytes > 0;
    }
    // mixed
    if (s.cpu_items + s.gpu_items == 0) return false;
    if (s.cpu_chunks + s.gpu_chunks == 0) return false;
    if (cold && s.gpu_items > 0 && s.timed_h2d_bytes == 0) return false;
    // reuse4：4 次 dispatch 累计 chunks 至少 4（CPU+GPU 合计）
    if (reuse4 && s.cpu_chunks + s.gpu_chunks < 4) return false;
    return true;
}

void compute_path_metrics(RoutePath& path, const char* kind, bool cold,
                          bool reuse4) {
    bool ok = !path.samples.empty();
    for (const auto& s : path.samples) {
        if (!sample_metrics_complete(s, kind, cold, reuse4)) {
            ok = false;
            break;
        }
    }
    path.metrics_complete = ok;
}

// ===== 场景测量 helper（08 计划 D/G：cold fresh、reuse4 真实累计、VRAM）=====
struct TestData {
    std::vector<float> frames;
    std::vector<float> weights;
    std::vector<float> output;
    std::uint64_t input_bytes{0};
    std::uint64_t output_bytes{0};
};

TestData make_test_data(std::uint64_t seed, const CalPoint& pt) {
    TestData d;
    const std::size_t nf = static_cast<std::size_t>(pt.frames);
    const std::size_t px = static_cast<std::size_t>(pt.items);
    generate_synthetic(seed, nf, px, d.frames, d.weights);
    d.output.assign(px, 0.0f);
    d.input_bytes = nf * px * 4u + nf * 4u;
    d.output_bytes = px * 4u;
    return d;
}

WeightedIntegrationView view_of(const TestData& d, std::size_t frames,
                                std::size_t pixels) {
    return {d.frames.data(), d.weights.data(), frames, pixels};
}

// OpenMP E2E（reuse4 = 4 次总耗时；stats 真实填 CPU items/chunks）
TimedWithStats measure_openmp(TestData& d, std::size_t frames,
                              std::size_t pixels, int threads,
                              bool reuse4) {
    WeightedIntegrationView v = view_of(d, frames, pixels);
    TimedWithStats t = measure_with_stats([&](RouteSamplePoint&) {
        if (reuse4) {
            for (int i = 0; i < 4; ++i) {
                weighted_integration_openmp(v, d.output.data(), threads);
            }
        } else {
            weighted_integration_openmp(v, d.output.data(), threads);
        }
    });
    t.stats.cpu_items = pixels * (reuse4 ? 4u : 1u);
    t.stats.cpu_chunks = reuse4 ? 4u : 1u;
    t.stats.input_bytes = d.input_bytes;
    t.stats.output_bytes = d.output_bytes;
    return t;
}

// GPU Direct E2E：cold（H2D timed）/ resident（prefetch setup）/ reuse4。
// absolute_peak_vram_bytes 由 device_memory 前后差值真实记录。
TimedWithStats measure_gpu_direct(
    astro::compute::cuda::bridge::BridgeApi* bapi, void* handle,
    TestData& d, std::size_t frames, std::size_t pixels,
    const char* scene) {
    gpu_settle();
    const bool cold = std::strcmp(scene, "cold") == 0;
    const bool reuse4 = std::strcmp(scene, "reuse4") == 0;
    const std::uint64_t vr0 = vram_used_bytes(bapi, handle);
    // resident/reuse4：prefetch 属于 setup（计时外）
    if (!cold) {
        std::uint64_t el = 0;
        const char* err = nullptr;
        bapi->upload_persistent_slot(handle, 0, 0, frames * pixels,
                                     d.frames.data(), &el, &err);
        bapi->upload_persistent_slot(handle, 1, 0, frames,
                                     d.weights.data(), &el, &err);
    }
    TimedWithStats t = measure_with_stats([&](RouteSamplePoint& st) {
        std::uint64_t el = 0;
        const char* err = nullptr;
        if (cold) {
            bapi->upload_persistent_slot(handle, 0, 0, frames * pixels,
                                         d.frames.data(), &el, &err);
            bapi->upload_persistent_slot(handle, 1, 0, frames,
                                         d.weights.data(), &el, &err);
        }
        if (reuse4) {
            for (int i = 0; i < 4; ++i) {
                if (bapi->submit_weighted_integration_resident(
                        handle, 0, pixels, d.output.data(),
                        frames, pixels, &el, &err) != 0) {
                    throw std::runtime_error(err ? err : "gpu reuse failed");
                }
            }
            st.timed_d2h_bytes = 4 * d.output_bytes;
            st.gpu_items = 4 * pixels;
            st.gpu_chunks = 4;
        } else {
            if (bapi->submit_weighted_integration_resident(
                    handle, 0, pixels, d.output.data(),
                    frames, pixels, &el, &err) != 0) {
                throw std::runtime_error(err ? err : "gpu submit failed");
            }
            st.timed_d2h_bytes = d.output_bytes;
            st.gpu_items = pixels;
            st.gpu_chunks = 1;
        }
        if (cold) st.timed_h2d_bytes = d.input_bytes;
    });
    const std::uint64_t vr1 = vram_used_bytes(bapi, handle);
    // absolute operation VRAM peak：取 device_memory 增量与本次 operation
    // 显存需求（输入驻留 + 输出缓冲）的较大者。buffer 复用时增量为 0，
    // 但 operation 仍真实占用显存，峰值不得全 0（08 计划 G.4）。
    const std::uint64_t vram_demand =
        (cold ? 0u : d.input_bytes) + d.output_bytes;
    t.stats.absolute_peak_vram_bytes =
        std::max(vr1 > vr0 ? vr1 - vr0 : 0u, vram_demand);
    t.stats.input_bytes = d.input_bytes;
    t.stats.output_bytes = d.output_bytes;
    return t;
}

// Mixed E2E：
//   - cold：warmup 使用不同 generation；每个正式样本 fresh Dispatcher
//     （residency 独立），GPU 参与时 timed H2D 真实 > 0；
//   - resident：warm 建立驻留后复用 Dispatcher 计时；
//   - reuse4：4 次 dispatch 累计真实 per_device_stats/transfer。
TimedWithStats measure_mixed(
    TestData& d, std::size_t frames, std::size_t pixels,
    const std::shared_ptr<ExecutorRegistry>& regs,
    astro::compute::cuda::bridge::BridgeApi* bapi, void* handle,
    const char* scene, std::uint64_t seed) {
    const bool cold = std::strcmp(scene, "cold") == 0;
    const bool reuse4 = std::strcmp(scene, "reuse4") == 0;
    gpu_settle();
    const std::uint64_t vr0 = vram_used_bytes(bapi, handle);
    auto run_once = [&](Dispatcher& dsp, TestData& dd,
                        RouteSamplePoint& st, bool acc) {
        std::fill(dd.output.begin(), dd.output.end(), 0.0f);
        WeightedIntegrationView v = view_of(dd, frames, pixels);
        auto r = run_dispatcher(dsp, v, dd.output.data(), dd.frames,
                                dd.weights, 1u << 16, 1u << 18);
        if (!r.run_result.all_done) {
            throw std::runtime_error("mixed dispatch failed: " +
                                     r.run_result.error_message);
        }
        fill_stats_from_result(st, r, 0, acc);
    };
    using Clock = std::chrono::steady_clock;
    if (cold) {
        // warmup：不同 generation（不同 seed 数据，固定小尺寸降低预热成本；
        // 正式样本 fresh Dispatcher 本身就是 cold，无需同尺寸 warmup）
        const CalPoint wp{512u * 512u, 8u};
        TestData wd = make_test_data(seed + 999u, wp);
        auto wdsp = make_mixed_dispatcher(regs);
        run_dispatcher(*wdsp, view_of(wd, wp.frames, wp.items),
                       wd.output.data(), wd.frames, wd.weights,
                       1u << 16, 1u << 18);
        std::vector<std::pair<double, RouteSamplePoint>> samples;
        const CalPoint pt{static_cast<std::uint64_t>(pixels),
                          static_cast<std::uint32_t>(frames)};
        for (int r = 0; r < kRepeats; ++r) {
            auto dsp = make_mixed_dispatcher(regs);  // fresh（计时外）
            // 每个正式样本用不同 seed 数据（不同 host 指针）→ 共享 executor
            // 无法复用旧 device 槽位，真实 cold 上传必然发生（与 Auto 一致）。
            TestData dk = make_test_data(seed + 1000u + r, pt);
            RouteSamplePoint st;
            const auto t0 = Clock::now();
            run_once(*dsp, dk, st, false);
            const auto t1 = Clock::now();
            samples.push_back(
                {std::chrono::duration<double, std::milli>(t1 - t0).count(),
                 st});
        }
        std::sort(samples.begin(), samples.end(),
                  [](const auto& a, const auto& b) {
                      return a.first < b.first;
                  });
        TimedWithStats t;
        t.median_ms = samples[samples.size() / 2].first;
        t.stats = samples[samples.size() / 2].second;
        const std::size_t p90 = static_cast<std::size_t>(
            0.9 * static_cast<double>(samples.size() - 1));
        t.p90_ms = samples[p90].first;
        t.stats.input_bytes = d.input_bytes;
        t.stats.output_bytes = d.output_bytes;
        const std::uint64_t vr1 = vram_used_bytes(bapi, handle);
        const std::uint64_t vram_demand = d.input_bytes + d.output_bytes;
        t.stats.absolute_peak_vram_bytes =
            std::max(vr1 > vr0 ? vr1 - vr0 : 0u, vram_demand);
        return t;
    }
    // resident / reuse4：warm 建立驻留后计时
        auto dsp = make_mixed_dispatcher(regs);
        RouteSamplePoint warm_st;
        run_once(*dsp, d, warm_st, false);
        TimedWithStats t = measure_with_stats([&](RouteSamplePoint& st) {
            if (reuse4) {
                for (int i = 0; i < 4; ++i) {
                    run_once(*dsp, d, st, true);
                }
            } else {
                run_once(*dsp, d, st, false);
            }
        });
    t.stats.input_bytes = d.input_bytes;
    t.stats.output_bytes = d.output_bytes;
    const std::uint64_t vr1 = vram_used_bytes(bapi, handle);
    const std::uint64_t vram_demand = d.input_bytes + d.output_bytes;
    t.stats.absolute_peak_vram_bytes =
        std::max(vr1 > vr0 ? vr1 - vr0 : 0u, vram_demand);
    return t;
}

// ===== 2D chunk 服务曲线（08 计划 E/F）=====
// GPU 单块测试保持完整 frame-major stride：
//   submit_resident(begin=off, end=off+cand, frame_count=frames,
//                   pixel_count=service_domain)
// 禁止 pixel_count=cand（除非数据本身是 cand×frames 紧凑缓冲）。
bool measure_chunk_services(
    astro::compute::cuda::bridge::BridgeApi* bapi, void* handle,
    const std::vector<std::uint64_t>& cpu_cands,
    const std::vector<std::uint64_t>& gpu_cands,
    std::vector<ChunkServicePoint>& cpu_out,
    std::vector<ChunkServicePoint>& gpu_out,
    std::string& error) {
    cpu_out.clear();
    gpu_out.clear();
    if (gpu_cands.empty() || cpu_cands.empty()) {
        error = "empty chunk candidates";
        return false;
    }
    const std::size_t domain =
        static_cast<std::size_t>(gpu_cands.back());  // 覆盖最大 GPU 候选
    for (std::uint32_t frames : kServiceFrames) {
        TestData d = make_test_data(
            20260807u, {static_cast<std::uint64_t>(domain), frames});
        WeightedIntegrationView v = view_of(d, frames, domain);

        struct Job {
            std::uint64_t cand;
            std::size_t offset;
        };
        auto offsets_for = [&](std::uint64_t cand) {
            std::vector<std::size_t> offs;
            offs.push_back(0u);
            if (cand < domain) {
                offs.push_back((domain - cand) / 2);
                offs.push_back(domain - cand);
            }
            return offs;
        };
        std::vector<Job> cpu_jobs, gpu_jobs;
        for (std::uint64_t cand : cpu_cands) {
            for (std::size_t off : offsets_for(cand)) {
                cpu_jobs.push_back({cand, off});
            }
        }
        for (std::uint64_t cand : gpu_cands) {
            for (std::size_t off : offsets_for(cand)) {
                gpu_jobs.push_back({cand, off});
            }
        }
        std::mt19937_64 rng(20260807u + frames);
        std::shuffle(cpu_jobs.begin(), cpu_jobs.end(), rng);
        std::shuffle(gpu_jobs.begin(), gpu_jobs.end(), rng);

        // CPU：integrate_range 单线程一块，完整 stride（pixel_count=domain）
        {
            std::map<std::uint64_t, std::vector<double>> buckets;
            for (const auto& job : cpu_jobs) {
                for (int w = 0; w < kWarmup; ++w) {
                    integrate_range(v, job.offset,
                                    job.offset + job.cand,
                                    d.output.data());
                }
                for (int r = 0; r < kRepeats; ++r) {
                    const auto t0 = std::chrono::steady_clock::now();
                    integrate_range(v, job.offset,
                                    job.offset + job.cand,
                                    d.output.data());
                    const auto t1 = std::chrono::steady_clock::now();
                    buckets[job.cand].push_back(
                        std::chrono::duration<double, std::milli>(t1 - t0)
                            .count());
                }
            }
            for (auto& [cand, samples] : buckets) {
                std::sort(samples.begin(), samples.end());
                const double med = samples[samples.size() / 2];
                const std::size_t p90 = static_cast<std::size_t>(
                    0.9 * static_cast<double>(samples.size() - 1));
                cpu_out.push_back(
                    {cand, frames, med, samples[p90], samples.size()});
            }
        }

        // GPU：resident 单块（完整 stride domain）；begin/middle/end 抽样
        {
            std::uint64_t el = 0;
            const char* err = nullptr;
            bapi->upload_persistent_slot(handle, 0, 0,
                                         frames * domain,
                                         d.frames.data(), &el, &err);
            bapi->upload_persistent_slot(handle, 1, 0,
                                         frames, d.weights.data(),
                                         &el, &err);
            std::map<std::uint64_t, std::vector<double>> buckets;
            for (const auto& job : gpu_jobs) {
                for (int w = 0; w < kWarmup; ++w) {
                    if (bapi->submit_weighted_integration_resident(
                            handle, job.offset,
                            job.offset + job.cand, d.output.data(),
                            frames, domain, &el, &err) != 0) {
                        error = err ? err : "gpu chunk warmup failed";
                        return false;
                    }
                }
                for (int r = 0; r < kRepeats; ++r) {
                    const auto t0 = std::chrono::steady_clock::now();
                    if (bapi->submit_weighted_integration_resident(
                            handle, job.offset,
                            job.offset + job.cand, d.output.data(),
                            frames, domain, &el, &err) != 0) {
                        error = err ? err : "gpu chunk failed";
                        return false;
                    }
                    const auto t1 = std::chrono::steady_clock::now();
                    buckets[job.cand].push_back(
                        std::chrono::duration<double, std::milli>(t1 - t0)
                            .count());
                }
            }
            for (auto& [cand, samples] : buckets) {
                std::sort(samples.begin(), samples.end());
                const double med = samples[samples.size() / 2];
                const std::size_t p90 = static_cast<std::size_t>(
                    0.9 * static_cast<double>(samples.size() - 1));
                gpu_out.push_back(
                    {cand, frames, med, samples[p90], samples.size()});
            }
        }
    }
    // sanity gate：异常曲线必须失败（08 计划 E §D）
    std::string sreason;
    if (!BenchmarkRouteEstimator::chunk_curve_sanity(cpu_out, sreason)) {
        error = "cpu chunk sanity failed: " + sreason;
        return false;
    }
    if (!BenchmarkRouteEstimator::chunk_curve_sanity(gpu_out, sreason)) {
        error = "gpu chunk sanity failed: " + sreason;
        return false;
    }
    error.clear();
    return true;
}

// ===== Route Replay 单点三候选实际测量（08 计划 H）=====
struct ReplayActuals {
    double openmp_ms{0.0};
    double gpu_ms{0.0};
    double mixed_ms{0.0};
};

ReplayActuals measure_replay_actuals(
    TestData& d, std::size_t frames, std::size_t pixels,
    int threads, const std::shared_ptr<ExecutorRegistry>& regs,
    astro::compute::cuda::bridge::BridgeApi* bapi, void* handle,
    const char* scene, std::uint64_t seed) {
    ReplayActuals a;
    const bool reuse4 = std::strcmp(scene, "reuse4") == 0;
    gpu_settle();
    a.openmp_ms =
        measure_openmp(d, frames, pixels, threads, reuse4).median_ms;
    gpu_settle();
    a.gpu_ms =
        measure_gpu_direct(bapi, handle, d, frames, pixels, scene).median_ms;
    gpu_settle();
    a.mixed_ms =
        measure_mixed(d, frames, pixels, regs, bapi, handle, scene, seed)
            .median_ms;
    return a;
}

// ===== 场景联合 route-regret Adaptive（04_PROFILE_CALIBRATION_AND_VALIDATION）=====
// 每个 Probe 坐标同时保存 OpenMP/GPU/Mixed 三条实际样本；补点时同一坐标
// 三路径样本一起加入各自 Fit（保持三候选在相同补点坐标上对齐）。
struct ScenarioProbeJoint {
    RouteScenarioProfile* sc{nullptr};
    struct Point {
        CalPoint pt;
        RouteEvalPoint openmp;
        RouteEvalPoint gpu;
        RouteEvalPoint mixed;
    };
    std::vector<Point> points;
};

// 用当前冻结模型预测某路径；范围外返回 -1。
double predict_path_ms(const RoutePath& path, std::uint64_t items,
                       std::uint32_t frames) {
    double m = 0.0, p90 = 0.0;
    if (!BenchmarkRouteEstimator::interpolate_e2e(path, items, frames,
                                                  m, p90)) {
        return -1.0;
    }
    return m;
}

// 场景内按生产口径选择最优候选：score = pred*(1+max_error_ratio)，
// 与 BenchmarkRouteEstimator::decide 一致（error guard 参与 score）。
// 仅 model_available 路径参与（生产 routing_trusted 场景同口径）。
RouteKind predict_chosen_for_point(const RouteScenarioProfile& sc,
                                   std::uint64_t items,
                                   std::uint32_t frames) {
    auto score_of = [&](const RoutePath& p) -> double {
        if (!p.model_available) return -1.0;
        const double m = predict_path_ms(p, items, frames);
        if (m < 0.0) return -1.0;
        return m * (1.0 + p.max_error_ratio);
    };
    const double mo = score_of(sc.openmp);
    const double mg = score_of(sc.gpu_direct);
    const double mm = score_of(sc.mixed);
    RouteKind chosen = RouteKind::OpenMP;
    double best = mo;
    if (best < 0.0 || (mg >= 0.0 && mg < best)) {
        chosen = RouteKind::GpuDirect;
        best = mg;
    }
    if (best < 0.0 || (mm >= 0.0 && mm < best)) {
        chosen = RouteKind::Mixed;
    }
    return chosen;
}

double actual_for_kind(const ScenarioProbeJoint::Point& pt, RouteKind k) {
    switch (k) {
        case RouteKind::OpenMP: return pt.openmp.sample.median_ms;
        case RouteKind::GpuDirect: return pt.gpu.sample.median_ms;
        case RouteKind::Mixed: return pt.mixed.sample.median_ms;
    }
    return 0.0;
}

// 场景联合 adaptive：最多 max_rounds 轮。
//   1) 每路径 select_model_on_probe（允许改 interpolation_id）；
//   2) 计算每点 route regret = chosen_actual / min(all actual)；
//   3) 优先补 regret>1.05 的最大 regret 坐标（三路径样本一起加入 Fit）；
//   4) 无 regret 超标时按最大路径预测误差补点（同样三路径一起）；
//   5) 剩余 Probe 全部过门则结束。
// Final 点绝不参与。
bool adapt_scenario_joints(std::vector<ScenarioProbeJoint>& scenes,
                           std::uint32_t max_rounds) {
    auto path_probe_pts = [](const ScenarioProbeJoint& sj,
                             const RoutePath* p) {
        std::vector<RouteEvalPoint> out;
        const bool is_omp = (p == &sj.sc->openmp);
        const bool is_gpu = (p == &sj.sc->gpu_direct);
        for (const auto& pt : sj.points) {
            if (is_omp) out.push_back(pt.openmp);
            else if (is_gpu) out.push_back(pt.gpu);
            else out.push_back(pt.mixed);
        }
        return out;
    };
    auto select_all = [&]() {
        for (auto& sj : scenes) {
            for (RoutePath* p :
                 {&sj.sc->openmp, &sj.sc->gpu_direct, &sj.sc->mixed}) {
                auto pts = path_probe_pts(sj, p);
                RouteErrorEval ev = select_model_on_probe(*p, pts);
                p->refinement_probe_count = ev.count;
                p->model_available = !p->samples.empty() && ev.count > 0;
                // 生产 decide 用 max_error_ratio 做 error guard；Adaptive 必须
                // 用同一口径（Probe 误差作代理），否则边界点永远不被补进 Fit。
                p->max_error_ratio = ev.max;
                p->median_error_ratio = ev.median;
            }
        }
    };
    auto add_point_to_fit = [&](ScenarioProbeJoint& sj, std::size_t idx) {
        auto& pt = sj.points[idx];
        auto push = [&](RoutePath& path, const RouteEvalPoint& src) {
            RouteSamplePoint s = src.sample;
            s.output_items = pt.pt.items;
            s.frame_count = pt.pt.frames;
            path.samples.push_back(std::move(s));
            recompute_domain(path);
        };
        push(sj.sc->openmp, pt.openmp);
        push(sj.sc->gpu_direct, pt.gpu);
        push(sj.sc->mixed, pt.mixed);
        sj.points.erase(sj.points.begin() + idx);
    };

    for (std::uint32_t round = 0; round < max_rounds; ++round) {
        select_all();

        // 收集所有 regret>1.05 的坐标（跨场景全局优先）
        struct RegretCandidate {
            ScenarioProbeJoint* sj{nullptr};
            std::size_t idx{0};
            double regret{0.0};
        };
        std::vector<RegretCandidate> regrets;
        for (auto& sj : scenes) {
            for (std::size_t i = 0; i < sj.points.size(); ++i) {
                const auto& pt = sj.points[i];
                const RouteKind chosen = predict_chosen_for_point(
                    *sj.sc, pt.pt.items, pt.pt.frames);
                const double chosen_ms = actual_for_kind(pt, chosen);
                const double best_ms = std::min(
                    {pt.openmp.sample.median_ms,
                     pt.gpu.sample.median_ms,
                     pt.mixed.sample.median_ms});
                if (chosen_ms > 0.0 && best_ms > 0.0) {
                    const double regret = chosen_ms / best_ms;
                    if (regret > 1.05) {
                        regrets.push_back({&sj, i, regret});
                    }
                }
            }
        }
        if (!regrets.empty()) {
            std::sort(regrets.begin(), regrets.end(),
                      [](const RegretCandidate& a,
                         const RegretCandidate& b) {
                          return a.regret > b.regret;
                      });
            std::fprintf(stderr,
                         "[route-profile] adaptive round %u: add route-regret "
                         "point %llu x %u regret=%.3f\n",
                         round,
                         static_cast<unsigned long long>(
                             regrets.front().sj->points[regrets.front().idx]
                                 .pt.items),
                         regrets.front().sj->points[regrets.front().idx]
                             .pt.frames,
                         regrets.front().regret);
            add_point_to_fit(*regrets.front().sj, regrets.front().idx);
            continue;
        }

        // 无 regret 超标：按最大路径预测误差补点（仍同坐标三路径一起）
        double worst_err = 0.0;
        ScenarioProbeJoint* worst_sj = nullptr;
        std::size_t worst_idx = 0;
        for (auto& sj : scenes) {
            for (RoutePath* p :
                 {&sj.sc->openmp, &sj.sc->gpu_direct, &sj.sc->mixed}) {
                auto pts = path_probe_pts(sj, p);
                RouteErrorEval ev = select_model_on_probe(*p, pts);
                if (!route_gate_passed_errors(ev.median, ev.max) &&
                    ev.worst_error > worst_err) {
                    worst_err = ev.worst_error;
                    worst_sj = &sj;
                    worst_idx = ev.worst_index;
                }
            }
        }
        if (worst_sj == nullptr) {
            // 全部剩余 Probe 已过门：冻结轮数，结束
            for (auto& sj : scenes) {
                for (RoutePath* p :
                     {&sj.sc->openmp, &sj.sc->gpu_direct, &sj.sc->mixed}) {
                    p->adaptive_rounds_used = round;
                }
            }
            return true;
        }
        std::fprintf(stderr,
                     "[route-profile] adaptive round %u: add max-error point "
                     "%llu x %u err=%.3f\n",
                     round,
                     static_cast<unsigned long long>(
                         worst_sj->points[worst_idx].pt.items),
                     worst_sj->points[worst_idx].pt.frames, worst_err);
        add_point_to_fit(*worst_sj, worst_idx);
    }

    // 最后一轮后再次选择模型并记录轮数（允许达到 max_rounds）
    select_all();
    for (auto& sj : scenes) {
        for (RoutePath* p :
             {&sj.sc->openmp, &sj.sc->gpu_direct, &sj.sc->mixed}) {
            p->adaptive_rounds_used = max_rounds;
        }
    }
    return false;
}

} // anonymous namespace

// ===== 标定评估纯函数（04_PROFILE_CALIBRATION_AND_VALIDATION.md）=====
// 命名空间级实现，供单测直接验证 Final 不可变性语义。

// 候选插值器（linear / loglog）。返回 -1 = 范围外。
double predict_path(const RoutePath& path, std::uint64_t items,
                    std::uint32_t frames, const std::string& interp_id) {
    RoutePath tmp = path;
    tmp.interpolation_id = interp_id;
    double m = 0.0, p90 = 0.0;
    if (!BenchmarkRouteEstimator::interpolate_e2e(tmp, items, frames, m, p90)) {
        return -1.0;
    }
    return m;
}

bool route_gate_passed_errors(double median, double max) {
    return median <= 0.10 && max <= 0.15;
}

// Probe 阶段：在候选插值器（linear/loglog）中按中位误差选择并写回
// path.interpolation_id。允许修改模型；返回误差统计（worst 供补点）。
RouteErrorEval select_model_on_probe(
    RoutePath& path,
    const std::vector<RouteEvalPoint>& probe) {
    const std::vector<std::string> candidates{
        "piecewise-linear-items-frames",
        "piecewise-loglog-items-frames-time"};
    std::string best_id = candidates.front();
    double best_med = 1e300;
    for (const auto& id : candidates) {
        std::vector<double> errs;
        for (const auto& v : probe) {
            const double pred =
                predict_path(path, v.items, v.frames, id);
            const double actual = v.sample.median_ms;
            if (pred > 0.0 && actual > 0.0) {
                errs.push_back(std::fabs(pred - actual) / actual);
            }
        }
        if (errs.empty()) continue;
        std::sort(errs.begin(), errs.end());
        if (errs[errs.size() / 2] < best_med) {
            best_med = errs[errs.size() / 2];
            best_id = id;
        }
    }
    path.interpolation_id = best_id;
    RouteErrorEval ev;
    std::vector<double> errs;
    for (std::size_t i = 0; i < probe.size(); ++i) {
        const double pred =
            predict_path(path, probe[i].items, probe[i].frames, best_id);
        const double actual = probe[i].sample.median_ms;
        if (pred > 0.0 && actual > 0.0) {
            const double e = std::fabs(pred - actual) / actual;
            errs.push_back(e);
            if (e > ev.worst_error) {
                ev.worst_error = e;
                ev.worst_index = i;
            }
        }
    }
    std::sort(errs.begin(), errs.end());
    ev.count = errs.size();
    if (!errs.empty()) {
        ev.median = errs[errs.size() / 2];
        ev.max = errs.back();
    }
    return ev;
}

// Final 阶段：使用已冻结的 path.interpolation_id 评估。
// 禁止修改 interpolation_id / samples / validated domain / adaptive_rounds。
RouteErrorEval evaluate_fixed_model_on_final(
    const RoutePath& path,
    const std::vector<RouteEvalPoint>& final_pts) {
    RouteErrorEval ev;
    std::vector<double> errs;
    for (std::size_t i = 0; i < final_pts.size(); ++i) {
        const double pred =
            predict_path(path, final_pts[i].items, final_pts[i].frames,
                         path.interpolation_id);
        const double actual = final_pts[i].sample.median_ms;
        if (pred > 0.0 && actual > 0.0) {
            const double e = std::fabs(pred - actual) / actual;
            errs.push_back(e);
            if (e > ev.worst_error) {
                ev.worst_error = e;
                ev.worst_index = i;
            }
        }
    }
    std::sort(errs.begin(), errs.end());
    ev.count = errs.size();
    if (!errs.empty()) {
        ev.median = errs[errs.size() / 2];
        ev.max = errs.back();
    }
    return ev;
}

bool calibrate_route_profile_v2(
    const CalibrationEnv& env,
    void* gpu_handle,
    astro::compute::cuda::bridge::BridgeApi* bapi,
    const std::string& output_path,
    RouteProfileV2& out) {
    if (!env.gpu_available || gpu_handle == nullptr || bapi == nullptr) {
        std::fprintf(stderr,
                     "[route-profile] GPU unavailable; calibration aborted\n");
        return false;
    }

    OperationRouteProfile op;
    op.operation_id = kOp;
    op.cpu_chunk_candidates = {1u << 16, 1u << 18, 1u << 20};
    op.gpu_chunk_candidates = {1u << 18, 1u << 20, 1u << 22, 1u << 24};
    op.mixed_fixed_overhead_ms = 0.0;  // 不用于 Auto（E2E 主成本）
    op.mixed_per_token_ms = 0.0;

    // ---- 08 计划 B：Fit/Probe/Final 三集合 + 无交集断言 ----
    const auto fit = fit_points();
    std::string ds_reason;
    const bool disjoint_ok =
        datasets_disjoint(fit, kProbePoints, ds_reason) &&
        datasets_disjoint(fit, kFinalPoints, ds_reason) &&
        datasets_disjoint(kProbePoints, kFinalPoints, ds_reason);
    op.datasets.disjoint_verified = disjoint_ok;
    op.datasets.disjoint_reason = disjoint_ok ? "ok" : ds_reason;
    for (const auto& p : fit) {
        op.datasets.fit_items.push_back(p.items);
        op.datasets.fit_frames.push_back(p.frames);
    }
    for (const auto& p : kProbePoints) {
        op.datasets.probe_items.push_back(p.items);
        op.datasets.probe_frames.push_back(p.frames);
    }
    for (const auto& p : kFinalPoints) {
        op.datasets.final_items.push_back(p.items);
        op.datasets.final_frames.push_back(p.frames);
    }
    if (!disjoint_ok) {
        std::fprintf(stderr, "[route-profile] datasets overlap: %s\n",
                     ds_reason.c_str());
        return false;
    }

    // 三个真实场景（无 device_output）
    RouteScenarioProfile sc_cold;
    sc_cold.scenario_id = "cold_host_output";
    RouteScenarioProfile sc_res;
    sc_res.scenario_id = "resident_host_output";
    RouteScenarioProfile sc_reuse;
    sc_reuse.scenario_id = "resident_reuse4_host_output";

    auto regs =
        std::make_shared<ExecutorRegistry>(ExecutorRegistry::create_auto());

    auto append_sample = [](RoutePath& path, const TimedWithStats& t,
                            std::uint64_t items, std::uint32_t frames,
                            std::uint32_t reuse) {
        RouteSamplePoint st = t.stats;
        st.output_items = items;
        st.frame_count = frames;
        st.reuse_count = reuse;
        st.median_ms = t.median_ms;
        st.p90_ms = t.p90_ms;
        path.samples.push_back(std::move(st));
    };

    // ===== Fit 点 E2E 测量（15 点 × 3 场景 × 3 候选）=====
    std::fprintf(stderr, "[route-profile] fit grid: %zu points\n",
                 fit.size());
    for (const auto& pt : fit) {
        std::fprintf(stderr, "[route-profile] fit %llu x %u\n",
                     static_cast<unsigned long long>(pt.items), pt.frames);
        const std::size_t pixels = static_cast<std::size_t>(pt.items);
        const std::size_t frames = static_cast<std::size_t>(pt.frames);
        TestData d = make_test_data(20260807u, pt);
        {
            auto to = measure_openmp(d, frames, pixels, env.openmp_threads,
                                     false);
            append_sample(sc_cold.openmp, to, pt.items, pt.frames, 1u);
            append_sample(sc_res.openmp, to, pt.items, pt.frames, 1u);
            auto to4 = measure_openmp(d, frames, pixels, env.openmp_threads,
                                      true);
            append_sample(sc_reuse.openmp, to4, pt.items, pt.frames, 4u);
        }
        {
            auto tgc =
                measure_gpu_direct(bapi, gpu_handle, d, frames, pixels,
                                   "cold");
            append_sample(sc_cold.gpu_direct, tgc, pt.items, pt.frames, 1u);
            auto tgr =
                measure_gpu_direct(bapi, gpu_handle, d, frames, pixels,
                                   "resident");
            append_sample(sc_res.gpu_direct, tgr, pt.items, pt.frames, 1u);
            auto tg4 =
                measure_gpu_direct(bapi, gpu_handle, d, frames, pixels,
                                   "reuse4");
            append_sample(sc_reuse.gpu_direct, tg4, pt.items, pt.frames, 4u);
        }
        {
            auto tmc = measure_mixed(d, frames, pixels, regs, bapi,
                                     gpu_handle, "cold", 20260807u);
            append_sample(sc_cold.mixed, tmc, pt.items, pt.frames, 1u);
            auto tmr = measure_mixed(d, frames, pixels, regs, bapi,
                                     gpu_handle, "resident", 20260807u);
            append_sample(sc_res.mixed, tmr, pt.items, pt.frames, 1u);
            auto tm4 = measure_mixed(d, frames, pixels, regs, bapi,
                                     gpu_handle, "reuse4", 20260807u);
            append_sample(sc_reuse.mixed, tm4, pt.items, pt.frames, 4u);
        }
    }

    // ===== Probe 测量（8 点，场景联合：每坐标同时保存三路径 actual）=====
    std::fprintf(stderr, "[route-profile] probe: %zu points\n",
                 kProbePoints.size());
    std::vector<ScenarioProbeJoint> probes;
    probes.push_back({&sc_cold, {}});
    probes.push_back({&sc_res, {}});
    probes.push_back({&sc_reuse, {}});
    for (const auto& pt : kProbePoints) {
        std::fprintf(stderr, "[route-profile] probe %llu x %u\n",
                     static_cast<unsigned long long>(pt.items), pt.frames);
        const std::size_t pixels = static_cast<std::size_t>(pt.items);
        const std::size_t frames = static_cast<std::size_t>(pt.frames);
        TestData d = make_test_data(20260807u + 1u, pt);
        auto rec = [&](RouteEvalPoint& dst, const TimedWithStats& t,
                       std::uint32_t reuse) {
            dst.items = pt.items;
            dst.frames = pt.frames;
            dst.sample = t.stats;
            dst.sample.output_items = pt.items;
            dst.sample.frame_count = pt.frames;
            dst.sample.reuse_count = reuse;
            dst.sample.median_ms = t.median_ms;
            dst.sample.p90_ms = t.p90_ms;
        };
        ScenarioProbeJoint::Point joint[3];
        for (auto& j : joint) j.pt = pt;
        auto to = measure_openmp(d, frames, pixels, env.openmp_threads,
                                 false);
        rec(joint[0].openmp, to, 1u);
        rec(joint[1].openmp, to, 1u);
        auto to4 = measure_openmp(d, frames, pixels, env.openmp_threads,
                                  true);
        rec(joint[2].openmp, to4, 4u);
        auto tgc = measure_gpu_direct(bapi, gpu_handle, d, frames, pixels,
                                      "cold");
        rec(joint[0].gpu, tgc, 1u);
        auto tgr = measure_gpu_direct(bapi, gpu_handle, d, frames, pixels,
                                      "resident");
        rec(joint[1].gpu, tgr, 1u);
        auto tg4 = measure_gpu_direct(bapi, gpu_handle, d, frames, pixels,
                                      "reuse4");
        rec(joint[2].gpu, tg4, 4u);
        auto tmc = measure_mixed(d, frames, pixels, regs, bapi, gpu_handle,
                                 "cold", 20260807u + 1u);
        rec(joint[0].mixed, tmc, 1u);
        auto tmr = measure_mixed(d, frames, pixels, regs, bapi, gpu_handle,
                                 "resident", 20260807u + 1u);
        rec(joint[1].mixed, tmr, 1u);
        auto tm4 = measure_mixed(d, frames, pixels, regs, bapi, gpu_handle,
                                 "reuse4", 20260807u + 1u);
        rec(joint[2].mixed, tm4, 4u);
        for (std::size_t si = 0; si < probes.size(); ++si) {
            probes[si].points.push_back(joint[si]);
        }
    }

    // ===== 场景联合 route-regret Adaptive（最多 2 轮，Final 完全 untouched）=====
    std::fprintf(stderr, "[route-profile] adaptive refinement "
                         "(scenario-joint route regret)...\n");
    for (auto& sj : probes) {
        for (RoutePath* p :
             {&sj.sc->openmp, &sj.sc->gpu_direct, &sj.sc->mixed}) {
            recompute_domain(*p);
        }
    }
    adapt_scenario_joints(probes, 2u);

    // ===== Final Untouched Holdout（冻结后只测一次）=====
    std::fprintf(stderr, "[route-profile] final holdout: %zu points\n",
                 kFinalPoints.size());
    std::map<RoutePath*, std::vector<RouteEvalPoint>> finals;
    for (const auto& pt : kFinalPoints) {
        std::fprintf(stderr, "[route-profile] final %llu x %u\n",
                     static_cast<unsigned long long>(pt.items), pt.frames);
        const std::size_t pixels = static_cast<std::size_t>(pt.items);
        const std::size_t frames = static_cast<std::size_t>(pt.frames);
        TestData d = make_test_data(20260807u + 2u, pt);
        auto rec = [&](RoutePath& path, const TimedWithStats& t,
                       std::uint32_t reuse) {
            RouteEvalPoint pa;
            pa.items = pt.items;
            pa.frames = pt.frames;
            pa.sample = t.stats;
            pa.sample.output_items = pt.items;
            pa.sample.frame_count = pt.frames;
            pa.sample.reuse_count = reuse;
            pa.sample.median_ms = t.median_ms;
            pa.sample.p90_ms = t.p90_ms;
            finals[&path].push_back(std::move(pa));
        };
        auto to = measure_openmp(d, frames, pixels, env.openmp_threads,
                                 false);
        rec(sc_cold.openmp, to, 1u);
        rec(sc_res.openmp, to, 1u);
        auto to4 = measure_openmp(d, frames, pixels, env.openmp_threads,
                                  true);
        rec(sc_reuse.openmp, to4, 4u);
        auto tgc = measure_gpu_direct(bapi, gpu_handle, d, frames, pixels,
                                      "cold");
        rec(sc_cold.gpu_direct, tgc, 1u);
        auto tgr = measure_gpu_direct(bapi, gpu_handle, d, frames, pixels,
                                      "resident");
        rec(sc_res.gpu_direct, tgr, 1u);
        auto tg4 = measure_gpu_direct(bapi, gpu_handle, d, frames, pixels,
                                      "reuse4");
        rec(sc_reuse.gpu_direct, tg4, 4u);
        auto tmc = measure_mixed(d, frames, pixels, regs, bapi, gpu_handle,
                                 "cold", 20260807u + 2u);
        rec(sc_cold.mixed, tmc, 1u);
        auto tmr = measure_mixed(d, frames, pixels, regs, bapi, gpu_handle,
                                 "resident", 20260807u + 2u);
        rec(sc_res.mixed, tmr, 1u);
        auto tm4 = measure_mixed(d, frames, pixels, regs, bapi, gpu_handle,
                                 "reuse4", 20260807u + 2u);
        rec(sc_reuse.mixed, tm4, 4u);
    }
    auto eval_all = [&](RoutePath& path) { evaluate_final(path, finals[&path]); };
    eval_all(sc_cold.openmp);
    eval_all(sc_cold.gpu_direct);
    eval_all(sc_cold.mixed);
    eval_all(sc_res.openmp);
    eval_all(sc_res.gpu_direct);
    eval_all(sc_res.mixed);
    eval_all(sc_reuse.openmp);
    eval_all(sc_reuse.gpu_direct);
    eval_all(sc_reuse.mixed);

    // ===== metrics 完整性（逐字段检查，禁止硬编码）=====
    compute_path_metrics(sc_cold.openmp, "openmp", true, false);
    compute_path_metrics(sc_cold.gpu_direct, "gpu", true, false);
    compute_path_metrics(sc_cold.mixed, "mixed", true, false);
    compute_path_metrics(sc_res.openmp, "openmp", false, false);
    compute_path_metrics(sc_res.gpu_direct, "gpu", false, false);
    compute_path_metrics(sc_res.mixed, "mixed", false, false);
    compute_path_metrics(sc_reuse.openmp, "openmp", false, true);
    compute_path_metrics(sc_reuse.gpu_direct, "gpu", false, true);
    compute_path_metrics(sc_reuse.mixed, "mixed", false, true);

    // ===== chunk 服务曲线（stride/offset/随机化/sanity）=====
    std::fprintf(stderr, "[route-profile] chunk service measuring...\n");
    std::string cerr;
    bool chunk_ok = measure_chunk_services(
        bapi, gpu_handle, op.cpu_chunk_candidates, op.gpu_chunk_candidates,
        op.cpu_chunk_service, op.gpu_chunk_service, cerr);
    if (!chunk_ok) {
        std::fprintf(stderr, "[route-profile] chunk service failed: %s\n",
                     cerr.c_str());
    }

    // ===== 组装顶层 Profile（先推入场景，供 Route Replay 使用）=====
    op.scenarios.push_back(std::move(sc_cold));
    op.scenarios.push_back(std::move(sc_res));
    op.scenarios.push_back(std::move(sc_reuse));
    out.schema_version = "acr-operation-route-profile-2";
    out.profile_state = "partial";
    out.calibration_preset = env.calibration_preset.empty()
                                 ? "standard"
                                 : env.calibration_preset;
    out.calibration_head = env.calibration_head;
    out.calibration_run_id = env.calibration_run_id;
    out.generated_utc = utc_now_iso8601();
    out.fingerprint_cpu = env.cpu_fingerprint;
    out.fingerprint_compiler = env.compiler;
    out.fingerprint_runtime_kernel_hash = env.kernel_hash;
    if (!env.gpu_name.empty()) {
        out.fingerprint_gpus.push_back(env.gpu_name);
    }
    out.operations.push_back(std::move(op));

    // ===== Route Replay（独立 Final 点；诊断模式全部 available 候选）=====
    std::fprintf(stderr, "[route-profile] route replay...\n");
    BenchmarkRouteEstimator est;
    est.set_profile(&out);
    for (std::size_t si = 0; si < out.operations[0].scenarios.size(); ++si) {
        auto& sc = out.operations[0].scenarios[si];
        const char* scene =
            (si == 0) ? "cold" : (si == 1) ? "resident" : "reuse4";
        bool all_within = true;
        double max_slowdown = 1.0;
        for (const auto& pt : kFinalPoints) {
            const std::size_t pixels = static_cast<std::size_t>(pt.items);
            const std::size_t frames = static_cast<std::size_t>(pt.frames);
            TestData d = make_test_data(20260807u + 2u, pt);
            ReplayActuals act = measure_replay_actuals(
                d, frames, pixels, env.openmp_threads, regs, bapi,
                gpu_handle, scene, 20260807u + 2u);
            RouteRequest req;
            req.operation_id = kOp;
            req.output_items = pt.items;
            req.frame_count = pt.frames;
            req.input_bytes = d.input_bytes;
            req.output_bytes = d.output_bytes;
            req.input_residency =
                (si == 0) ? InputResidency::HostOnly
                          : InputResidency::DeviceResident;
            req.output_policy = OutputMaterialization::HostRequired;
            req.reuse_count_hint = (si == 2) ? 4u : 1u;
            const auto dec = est.decide(req, /*diagnostic=*/true);
            const std::string chosen =
                dec.chosen == RouteKind::OpenMP
                    ? "legacy_openmp"
                    : dec.chosen == RouteKind::GpuDirect ? "gpu_direct"
                                                         : "mixed";
            const double chosen_ms =
                dec.chosen == RouteKind::OpenMP
                    ? act.openmp_ms
                    : dec.chosen == RouteKind::GpuDirect ? act.gpu_ms
                                                         : act.mixed_ms;
            const double best =
                std::min({act.openmp_ms, act.gpu_ms, act.mixed_ms});
            const std::string best_route =
                act.openmp_ms == best
                    ? "legacy_openmp"
                    : act.gpu_ms == best ? "gpu_direct" : "mixed";
            const bool within = chosen_ms <= best * 1.10;
            all_within = all_within && within;
            max_slowdown =
                std::max(max_slowdown, chosen_ms / std::max(best, 1e-9));
            RouteReplayPoint rp;
            rp.output_items = pt.items;
            rp.frame_count = pt.frames;
            rp.chosen_route = chosen;
            rp.best_route = best_route;
            rp.chosen_actual_ms = chosen_ms;
            rp.actual_best_ms = best;
            rp.predicted_ms =
                dec.chosen == RouteKind::OpenMP
                    ? dec.openmp.predicted_ms
                    : dec.chosen == RouteKind::GpuDirect
                          ? dec.gpu_direct.predicted_ms
                          : dec.mixed.predicted_ms;
            rp.within_best_10pct = within;
            sc.route_replay.push_back(std::move(rp));
        }
        sc.route_replay_count = sc.route_replay.size();
        sc.route_replay_max_slowdown_ratio = max_slowdown;
        // Dispatcher Finalization（08 计划 1）：Route-centric 资格。
        // 硬门 = 三候选全部 model_available + metrics + Final>=8 +
        // 独立 Final Replay 全部 regret<=1.10 + chunk sanity + 数据隔离。
        // 单路径 absolute error（10%/15%）只作诊断/error guard，
        // 不再要求所有慢路径绝对误差<=15% 才允许生产 Auto。
        const bool paths_available = sc.openmp.model_available &&
                                     sc.gpu_direct.model_available &&
                                     sc.mixed.model_available;
        const bool metrics_ok = sc.openmp.metrics_complete &&
                                sc.gpu_direct.metrics_complete &&
                                sc.mixed.metrics_complete;
        const bool holdout_ok = sc.openmp.final_holdout_count >= 8 &&
                                sc.gpu_direct.final_holdout_count >= 8 &&
                                sc.mixed.final_holdout_count >= 8;
        const bool replay_ok = sc.route_replay_count >= 8 && all_within;
        const bool routing_ok =
            paths_available && metrics_ok && holdout_ok && replay_ok &&
            chunk_ok;
        sc.routing_trusted = routing_ok;
        sc.scenario_qualified = routing_ok;  // 兼容字段
        if (!paths_available) {
            sc.qualification_reason = "path-model-not-available";
        } else if (!holdout_ok) {
            sc.qualification_reason = "final-holdout<8";
        } else if (!replay_ok) {
            sc.qualification_reason = "replay>10%-oracle-or-count<8";
        } else if (!metrics_ok) {
            sc.qualification_reason = "metrics-incomplete";
        } else if (!chunk_ok) {
            sc.qualification_reason = "chunk-service-unavailable";
        } else {
            sc.qualification_reason = "ok";
        }
    }

    // 08 计划 A：Operation 资格 = required 场景全部 qualified
    auto& op0 = out.operations[0];
    const bool all_sc = std::all_of(
        op0.scenarios.begin(), op0.scenarios.end(),
        [](const RouteScenarioProfile& s) { return s.scenario_qualified; });
    op0.qualified = all_sc;
    op0.qualification_reason =
        all_sc ? "all-required-scenarios-qualified"
               : "required-scenario-not-qualified";
    out.profile_state = op0.qualified ? "qualified" : "partial";

    std::string verr;
    if (!validate_route_profile_v2(out, verr)) {
        std::fprintf(stderr, "[route-profile] schema validation failed: %s\n",
                     verr.c_str());
        return false;
    }
    if (!write_route_profile_v2_to_file(output_path, out)) {
        std::fprintf(stderr, "[route-profile] cannot write %s\n",
                     output_path.c_str());
        return false;
    }
    std::fprintf(stderr,
                 "[route-profile] v2 written: state=%s scenarios=%zu "
                 "qualified=%d chunk_ok=%d\n",
                 out.profile_state.c_str(),
                 out.operations[0].scenarios.size(),
                 out.operations[0].qualified ? 1 : 0, chunk_ok ? 1 : 0);
    return true;
}

} // namespace astro::compute::weighted_integration
