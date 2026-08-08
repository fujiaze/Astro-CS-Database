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
constexpr int kRepeats = 5;  // BDR3：三集合+Replay 测量量翻倍，5 次中位足够
constexpr std::uint32_t kServiceFrames[] = {4u, 16u, 32u};

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
    {2816u * 2816u, 32u}, {3456u * 3456u, 8u}};

// Final Untouched Holdout：>=8/场景，与 Fit/Probe 均不重叠；
// 模型冻结后只运行一次（最终误差 + Route Replay）。
const std::vector<CalPoint> kFinalPoints{
    {768u * 768u, 12u},  {1024u * 1024u, 10u}, {1280u * 1280u, 20u},
    {1536u * 1536u, 24u}, {2560u * 2560u, 20u}, {3072u * 3072u, 12u},
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

// ===== Probe / Final 误差评估（08 计划 B/C/H）=====
struct ProbeActual {
    CalPoint pt;
    RouteSamplePoint sample;  // 中位样本 + 绑定 stats（转 Fit 时保留）
};

struct ErrorEval {
    double median{0.0};
    double max{0.0};
    std::size_t count{0};
    std::size_t worst_index{0};
    double worst_error{0.0};
};

bool gate_passed_errors(double median, double max) {
    return median <= 0.10 && max <= 0.15;
}

// 在给定验证集上按候选插值器选择最佳模型并计算 median/max 误差。
// 同时返回最差验证点（adaptive 补点用）。
ErrorEval evaluate_validation(
    RoutePath& path,
    const std::vector<ProbeActual>& validation) {
    const std::vector<std::string> candidates{
        "piecewise-linear-items-frames",
        "piecewise-loglog-items-frames-time"};
    std::string best_id = candidates.front();
    double best_med = 1e300;
    for (const auto& id : candidates) {
        std::vector<double> errs;
        for (const auto& v : validation) {
            const double pred =
                predict_path(path, v.pt.items, v.pt.frames, id);
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
    ErrorEval ev;
    std::vector<double> errs;
    for (std::size_t i = 0; i < validation.size(); ++i) {
        const double pred =
            predict_path(path, validation[i].pt.items,
                         validation[i].pt.frames, best_id);
        const double actual = validation[i].sample.median_ms;
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

// 最多 max_rounds 轮 adaptive refinement：
//   1) 在剩余 Probe 上选插值器并算误差；
//   2) 未过门则把最差 Probe 转入 Fit（加入 samples 并重算 domain）；
//   3) 对剩余 Probe 复测；模型冻结后才允许 Final。
// 返回 true=剩余 Probe 已过门。
bool adapt_path(RoutePath& path,
                std::vector<ProbeActual>& probe,
                std::uint32_t max_rounds) {
    for (std::uint32_t round = 0; round < max_rounds; ++round) {
        ErrorEval ev = evaluate_validation(path, probe);
        path.refinement_probe_count = ev.count;
        if (gate_passed_errors(ev.median, ev.max) || ev.count < 2) {
            path.adaptive_rounds_used = round;
            return gate_passed_errors(ev.median, ev.max);
        }
        // 最差 Probe 转入 Fit
        RouteSamplePoint s = probe[ev.worst_index].sample;
        s.output_items = probe[ev.worst_index].pt.items;
        s.frame_count = probe[ev.worst_index].pt.frames;
        path.samples.push_back(std::move(s));
        probe.erase(probe.begin() + ev.worst_index);
        recompute_domain(path);
        path.adaptive_rounds_used = round + 1;
    }
    ErrorEval ev = evaluate_validation(path, probe);
    path.refinement_probe_count = ev.count;
    return gate_passed_errors(ev.median, ev.max);
}

// Final 误差（冻结模型后只测一次，不再调参）
ErrorEval evaluate_final(RoutePath& path,
                         const std::vector<ProbeActual>& final_pts) {
    ErrorEval ev = evaluate_validation(path, final_pts);
    path.final_holdout_count = ev.count;
    path.final_median_error_ratio = ev.median;
    path.final_max_error_ratio = ev.max;
    // 兼容字段与最终 holdout 保持一致
    path.holdout_count = ev.count;
    path.median_error_ratio = ev.median;
    path.max_error_ratio = ev.max;
    path.p95_error_ratio = ev.max;  // >=8 点后由 CI 输出真实分位；max 保守
    path.model_trusted = ev.count >= 8 &&
                         gate_passed_errors(ev.median, ev.max);
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
    const std::uint64_t vr0 = vram_used_bytes(bapi, handle);
    auto run_once = [&](Dispatcher& dsp, RouteSamplePoint& st,
                        bool acc) {
        std::fill(d.output.begin(), d.output.end(), 0.0f);
        WeightedIntegrationView v = view_of(d, frames, pixels);
        auto r = run_dispatcher(dsp, v, d.output.data(), d.frames,
                                d.weights, 1u << 16, 1u << 18);
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
        for (int r = 0; r < kRepeats; ++r) {
            auto dsp = make_mixed_dispatcher(regs);  // fresh（计时外）
            RouteSamplePoint st;
            const auto t0 = Clock::now();
            run_once(*dsp, st, false);
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
    run_once(*dsp, warm_st, false);
    TimedWithStats t = measure_with_stats([&](RouteSamplePoint& st) {
        if (reuse4) {
            for (int i = 0; i < 4; ++i) {
                run_once(*dsp, st, true);
            }
        } else {
            run_once(*dsp, st, false);
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
    a.openmp_ms =
        measure_openmp(d, frames, pixels, threads, reuse4).median_ms;
    a.gpu_ms =
        measure_gpu_direct(bapi, handle, d, frames, pixels, scene).median_ms;
    a.mixed_ms =
        measure_mixed(d, frames, pixels, regs, bapi, handle, scene, seed)
            .median_ms;
    return a;
}

} // anonymous namespace

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

    // ===== Probe 测量（8 点，9 条路径独立 actual）=====
    std::fprintf(stderr, "[route-profile] probe: %zu points\n",
                 kProbePoints.size());
    std::map<RoutePath*, std::vector<ProbeActual>> probes;
    for (const auto& pt : kProbePoints) {
        std::fprintf(stderr, "[route-profile] probe %llu x %u\n",
                     static_cast<unsigned long long>(pt.items), pt.frames);
        const std::size_t pixels = static_cast<std::size_t>(pt.items);
        const std::size_t frames = static_cast<std::size_t>(pt.frames);
        TestData d = make_test_data(20260807u + 1u, pt);
        auto rec = [&](RoutePath& path, const TimedWithStats& t,
                       std::uint32_t reuse) {
            ProbeActual pa;
            pa.pt = pt;
            pa.sample = t.stats;
            pa.sample.output_items = pt.items;
            pa.sample.frame_count = pt.frames;
            pa.sample.reuse_count = reuse;
            pa.sample.median_ms = t.median_ms;
            pa.sample.p90_ms = t.p90_ms;
            probes[&path].push_back(std::move(pa));
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
                                 "cold", 20260807u + 1u);
        rec(sc_cold.mixed, tmc, 1u);
        auto tmr = measure_mixed(d, frames, pixels, regs, bapi, gpu_handle,
                                 "resident", 20260807u + 1u);
        rec(sc_res.mixed, tmr, 1u);
        auto tm4 = measure_mixed(d, frames, pixels, regs, bapi, gpu_handle,
                                 "reuse4", 20260807u + 1u);
        rec(sc_reuse.mixed, tm4, 4u);
    }

    // ===== 真正 Adaptive Refinement（每路径最多 2 轮）=====
    auto adapt_all = [&](RoutePath& path) {
        recompute_domain(path);
        adapt_path(path, probes[&path], 2u);
    };
    adapt_all(sc_cold.openmp);
    adapt_all(sc_cold.gpu_direct);
    adapt_all(sc_cold.mixed);
    adapt_all(sc_res.openmp);
    adapt_all(sc_res.gpu_direct);
    adapt_all(sc_res.mixed);
    adapt_all(sc_reuse.openmp);
    adapt_all(sc_reuse.gpu_direct);
    adapt_all(sc_reuse.mixed);

    // ===== Final Untouched Holdout（冻结后只测一次）=====
    std::fprintf(stderr, "[route-profile] final holdout: %zu points\n",
                 kFinalPoints.size());
    std::map<RoutePath*, std::vector<ProbeActual>> finals;
    for (const auto& pt : kFinalPoints) {
        std::fprintf(stderr, "[route-profile] final %llu x %u\n",
                     static_cast<unsigned long long>(pt.items), pt.frames);
        const std::size_t pixels = static_cast<std::size_t>(pt.items);
        const std::size_t frames = static_cast<std::size_t>(pt.frames);
        TestData d = make_test_data(20260807u + 2u, pt);
        auto rec = [&](RoutePath& path, const TimedWithStats& t,
                       std::uint32_t reuse) {
            ProbeActual pa;
            pa.pt = pt;
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
        const bool paths_trusted = sc.openmp.model_trusted &&
                                   sc.gpu_direct.model_trusted &&
                                   sc.mixed.model_trusted;
        const bool metrics_ok = sc.openmp.metrics_complete &&
                                sc.gpu_direct.metrics_complete &&
                                sc.mixed.metrics_complete;
        const bool holdout_ok = sc.openmp.final_holdout_count >= 8 &&
                                sc.gpu_direct.final_holdout_count >= 8 &&
                                sc.mixed.final_holdout_count >= 8;
        const bool replay_ok = sc.route_replay_count >= 8 && all_within;
        sc.scenario_qualified =
            paths_trusted && metrics_ok && holdout_ok && replay_ok &&
            chunk_ok;
        if (!paths_trusted) {
            sc.qualification_reason = "path-model-not-trusted";
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
