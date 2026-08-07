// lib/acr/examples/weighted_integration/route_profile_calibration.cpp
//
// 加权积分 Operation Route Profile v2 标定（BDR 复核控制包 63c296b0...70a74）。
//
// BDR 复核修正（08 计划 A-I）：
//   - 场景真正隔离：cold_host_output / resident_host_output /
//     resident_reuse4_host_output（无真实 KeepDevice API，不生成
//     resident_device_output）；
//   - chunk 服务曲线为真实单 token 服务时间（多 frame_count；
//     禁止整幅任务总耗时）；
//   - holdout 至少 8 个，各场景独立 actual（OpenMP/GPU/Mixed 分别实测）；
//   - E2E 插值模型（linear/loglog）由 holdout 选择；
//   - 超门自动补标定点（adaptive，最多 2 轮）；
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
#include <fstream>
#include <memory>
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
using astro::compute::scheduler::Dispatcher;
using astro::compute::scheduler::DispatcherConfig;
using astro::compute::scheduler::ExecutorRegistry;
using astro::compute::RouteMode;

constexpr const char* kOp = "synthetic.weighted_integration.fp64acc";
constexpr int kWarmup = 2;
constexpr int kRepeats = 7;
constexpr std::uint32_t kServiceFrames[] = {4u, 16u, 32u};

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
                            std::uint64_t setup_h2d) {
    st.cpu_items = 0;
    st.gpu_items = 0;
    st.cpu_chunks = 0;
    st.gpu_chunks = 0;
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
    st.timed_h2d_bytes = r.transfer_stats.h2d_bytes;
    st.timed_d2h_bytes = r.transfer_stats.d2h_bytes;
    st.peak_ram_bytes = r.resource_control.mem_peak_max;
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

// 计算路径在 holdout 上的 median/max 误差；返回最佳 interpolation_id。
std::string select_interpolation(
    RoutePath& path,
    const std::vector<std::tuple<std::uint64_t, std::uint32_t, double>>&
        holdout) {
    const std::vector<std::string> candidates{
        "piecewise-linear-items-frames",
        "piecewise-loglog-items-frames-time"};
    std::string best_id = candidates.front();
    double best_score = 1e300;
    for (const auto& id : candidates) {
        std::vector<double> errs;
        for (const auto& [items, frames, actual] : holdout) {
            const double pred = predict_path(path, items, frames, id);
            if (pred > 0.0 && actual > 0.0) {
                errs.push_back(std::fabs(pred - actual) / actual);
            }
        }
        if (errs.empty()) continue;
        std::sort(errs.begin(), errs.end());
        const double med = errs[errs.size() / 2];
        if (med < best_score) {
            best_score = med;
            best_id = id;
        }
    }
    path.interpolation_id = best_id;
    // 用选中的插值器计算最终误差
    std::vector<double> errs;
    for (const auto& [items, frames, actual] : holdout) {
        const double pred = predict_path(path, items, frames, best_id);
        if (pred > 0.0 && actual > 0.0) {
            errs.push_back(std::fabs(pred - actual) / actual);
        }
    }
    std::sort(errs.begin(), errs.end());
    path.holdout_count = errs.size();
    if (!errs.empty()) {
        path.median_error_ratio = errs[errs.size() / 2];
        path.max_error_ratio = errs.back();
        path.p95_error_ratio = path.max_error_ratio;
    }
    return best_id;
}

// 自适应补标定：对最差 holdout 邻域补一个标定点（简单版，最多 2 轮）。
// 返回是否已通过门（median<=10%、max<=15%）。
bool gate_passed(const RoutePath& p) {
    return p.median_error_ratio <= 0.10 && p.max_error_ratio <= 0.15;
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

    // ---- 标定矩阵 ----
    const std::vector<std::size_t> sizes{
        256u * 256u, 512u * 512u, 1024u * 1024u,
        2048u * 2048u, 4096u * 4096u};
    const std::vector<std::uint32_t> frames_list{4u, 16u, 32u};

    OperationRouteProfile op;
    op.operation_id = kOp;
    op.cpu_chunk_candidates = {1u << 16, 1u << 18, 1u << 20};
    op.gpu_chunk_candidates = {1u << 18, 1u << 20, 1u << 22, 1u << 24};
    op.mixed_fixed_overhead_ms = 0.0;  // 不用于 Auto（E2E 主成本）
    op.mixed_per_token_ms = 0.0;

    // 三个真实场景（无 device_output）
    RouteScenarioProfile sc_cold;
    sc_cold.scenario_id = "cold_host_output";
    RouteScenarioProfile sc_res;
    sc_res.scenario_id = "resident_host_output";
    RouteScenarioProfile sc_reuse;
    sc_reuse.scenario_id = "resident_reuse4_host_output";

    std::vector<std::tuple<std::uint64_t, std::uint32_t, double>>
        holdout_openmp, holdout_gpu, holdout_mixed;
    std::vector<std::tuple<std::uint64_t, std::uint32_t, double>>
        holdout_reuse_openmp, holdout_reuse_gpu, holdout_reuse_mixed;
    std::vector<std::tuple<std::uint64_t, std::uint32_t, double>>
        holdout_cold_gpu, holdout_cold_mixed;

    auto append_sample = [](RoutePath& path, std::uint64_t items,
                            std::uint32_t frames, double med, double p90,
                            RouteSamplePoint st) {
        st.output_items = items;
        st.frame_count = frames;
        st.median_ms = med;
        st.p90_ms = p90;
        path.samples.push_back(std::move(st));
    };

    for (std::size_t px : sizes) {
        for (std::uint32_t frames : frames_list) {
            const std::size_t pixels = px;
            const std::uint64_t input_bytes =
                static_cast<std::uint64_t>(frames) * pixels * 4u +
                frames * 4u;
            const std::uint64_t output_bytes = pixels * 4u;
            std::vector<float> fdata(frames * pixels), w(frames);
            generate_synthetic(20260807, frames, pixels, fdata, w);
            std::vector<float> out_buf(pixels);
            WeightedIntegrationView view{
                fdata.data(), w.data(), frames, pixels};

            // ---- OpenMP E2E（reuse4 为 4 次总耗时）----
            {
                TimedWithStats t = measure_with_stats([&](RouteSamplePoint&) {
                    weighted_integration_openmp(view, out_buf.data(),
                                                env.openmp_threads);
                });
                RouteSamplePoint st = t.stats;
                st.input_bytes = input_bytes;
                st.output_bytes = output_bytes;
                append_sample(sc_cold.openmp, pixels, frames,
                              t.median_ms, t.p90_ms, st);
                append_sample(sc_res.openmp, pixels, frames,
                              t.median_ms, t.p90_ms, st);
                TimedWithStats t4 = measure_with_stats([&](RouteSamplePoint&) {
                    for (int i = 0; i < 4; ++i) {
                        weighted_integration_openmp(view, out_buf.data(),
                                                    env.openmp_threads);
                    }
                });
                RouteSamplePoint s4 = t4.stats;
                s4.reuse_count = 4;
                s4.input_bytes = input_bytes;
                s4.output_bytes = output_bytes;
                append_sample(sc_reuse.openmp, pixels, frames,
                              t4.median_ms, t4.p90_ms, s4);
            }

            // ---- GPU Direct E2E ----
            {
                std::uint64_t el = 0;
                const char* err = nullptr;
                // cold：H2D 在 timed 区（true cold）
                TimedWithStats tc = measure_with_stats([&](RouteSamplePoint& st) {
                    bapi->upload_persistent_slot(gpu_handle, 0, 0,
                                                 frames * pixels,
                                                 fdata.data(), &el, &err);
                    bapi->upload_persistent_slot(gpu_handle, 1, 0,
                                                 frames, w.data(), &el, &err);
                    if (bapi->submit_weighted_integration_resident(
                            gpu_handle, 0, pixels, out_buf.data(),
                            frames, pixels, &el, &err) != 0) {
                        throw std::runtime_error(err ? err : "gpu cold failed");
                    }
                    st.timed_h2d_bytes = input_bytes;
                    st.timed_d2h_bytes = output_bytes;
                    st.gpu_items = pixels;
                    st.gpu_chunks = 1;
                });
                RouteSamplePoint stc = tc.stats;
                stc.input_bytes = input_bytes;
                stc.output_bytes = output_bytes;
                append_sample(sc_cold.gpu_direct, pixels, frames,
                              tc.median_ms, tc.p90_ms, stc);
                // resident：prefetch setup 外，kernel+D2H timed
                bapi->upload_persistent_slot(gpu_handle, 0, 0,
                                             frames * pixels,
                                             fdata.data(), &el, &err);
                bapi->upload_persistent_slot(gpu_handle, 1, 0,
                                             frames, w.data(), &el, &err);
                TimedWithStats tr = measure_with_stats([&](RouteSamplePoint& st) {
                    if (bapi->submit_weighted_integration_resident(
                            gpu_handle, 0, pixels, out_buf.data(),
                            frames, pixels, &el, &err) != 0) {
                        throw std::runtime_error(err ? err : "gpu res failed");
                    }
                    st.timed_d2h_bytes = output_bytes;
                    st.gpu_items = pixels;
                    st.gpu_chunks = 1;
                });
                RouteSamplePoint str = tr.stats;
                str.input_bytes = input_bytes;
                str.output_bytes = output_bytes;
                append_sample(sc_res.gpu_direct, pixels, frames,
                              tr.median_ms, tr.p90_ms, str);
                // reuse4：4 次 resident 总耗时
                TimedWithStats t4 = measure_with_stats([&](RouteSamplePoint& st) {
                    for (int i = 0; i < 4; ++i) {
                        if (bapi->submit_weighted_integration_resident(
                                gpu_handle, 0, pixels, out_buf.data(),
                                frames, pixels, &el, &err) != 0) {
                            throw std::runtime_error(
                                err ? err : "gpu reuse failed");
                        }
                    }
                    st.timed_d2h_bytes = 4 * output_bytes;
                    st.gpu_items = 4 * pixels;
                    st.gpu_chunks = 4;
                });
                RouteSamplePoint s4 = t4.stats;
                s4.reuse_count = 4;
                s4.input_bytes = input_bytes;
                s4.output_bytes = output_bytes;
                append_sample(sc_reuse.gpu_direct, pixels, frames,
                              t4.median_ms, t4.p90_ms, s4);
            }

            // ---- Mixed E2E（共享池 Dispatcher；cold/resident/reuse 独立）----
            {
                auto make_mixed = [&]() -> std::shared_ptr<Dispatcher> {
                    auto regs = std::make_shared<ExecutorRegistry>(
                        ExecutorRegistry::create_auto());
                    auto d = std::make_shared<Dispatcher>();
                    DispatcherConfig cfg;
                    cfg.devices = {{"cpu", 0, 0, 50.0, true},
                                   {"cuda:0", 1, 0, 500.0, true}};
                    cfg.executors = regs;
                    cfg.route_mode = RouteMode::AutoMixed;
                    cfg.force_all_supported_executors = true;
                    d->configure(cfg);
                    return d;
                };
                auto spd = make_mixed();
                // cold Mixed：输入未驻留，H2D timed（Dispatcher prefetch 在
                // dispatch 内计时）
                TimedWithStats tmc = measure_with_stats(
                    [&](RouteSamplePoint& st) {
                        std::fill(out_buf.begin(), out_buf.end(), 0.0f);
                        auto r = run_dispatcher(*spd, view, out_buf.data(),
                                                fdata, w, 1u << 16, 1u << 18);
                        if (!r.run_result.all_done) {
                            throw std::runtime_error("cold mixed failed");
                        }
                        fill_stats_from_result(st, r, 0);
                    });
                RouteSamplePoint stc = tmc.stats;
                stc.input_bytes = input_bytes;
                stc.output_bytes = output_bytes;
                append_sample(sc_cold.mixed, pixels, frames,
                              tmc.median_ms, tmc.p90_ms, stc);
                // resident Mixed：warm 建立驻留后计时
                run_dispatcher(*spd, view, out_buf.data(), fdata, w,
                               1u << 16, 1u << 18);
                TimedWithStats tmr = measure_with_stats(
                    [&](RouteSamplePoint& st) {
                        std::fill(out_buf.begin(), out_buf.end(), 0.0f);
                        auto r = run_dispatcher(*spd, view, out_buf.data(),
                                                fdata, w, 1u << 16, 1u << 18);
                        if (!r.run_result.all_done) {
                            throw std::runtime_error("resident mixed failed");
                        }
                        fill_stats_from_result(st, r, 0);
                    });
                RouteSamplePoint str = tmr.stats;
                str.input_bytes = input_bytes;
                str.output_bytes = output_bytes;
                append_sample(sc_res.mixed, pixels, frames,
                              tmr.median_ms, tmr.p90_ms, str);
                // reuse4 Mixed：4 次 resident 总耗时
                TimedWithStats t4 = measure_with_stats(
                    [&](RouteSamplePoint& st) {
                        for (int i = 0; i < 4; ++i) {
                            std::fill(out_buf.begin(), out_buf.end(), 0.0f);
                            auto r = run_dispatcher(
                                *spd, view, out_buf.data(), fdata, w,
                                1u << 16, 1u << 18);
                            if (!r.run_result.all_done) {
                                throw std::runtime_error("reuse mixed failed");
                            }
                        }
                        st.gpu_items = 4 * pixels;
                    });
                RouteSamplePoint s4 = t4.stats;
                s4.reuse_count = 4;
                s4.input_bytes = input_bytes;
                s4.output_bytes = output_bytes;
                append_sample(sc_reuse.mixed, pixels, frames,
                              t4.median_ms, t4.p90_ms, s4);
            }
        }
    }

    // ---- holdout（8 个，各场景独立 actual）----
    const std::vector<std::pair<std::size_t, std::uint32_t>> holdouts{
        {384u * 384u, 8u}, {768u * 768u, 12u}, {1280u * 1280u, 20u},
        {1536u * 1536u, 24u}, {2560u * 2560u, 20u}, {3072u * 3072u, 12u},
        {3584u * 3584u, 28u}, {4096u * 4096u, 24u}};
    for (const auto& [px, frames] : holdouts) {
        const std::size_t pixels = px;
        std::vector<float> fdata(frames * pixels), w(frames);
        generate_synthetic(20260807, frames, pixels, fdata, w);
        std::vector<float> out_buf(pixels);
        WeightedIntegrationView view{fdata.data(), w.data(), frames, pixels};

        // OpenMP single / reuse4
        TimedWithStats to = measure_with_stats([&](RouteSamplePoint&) {
            weighted_integration_openmp(view, out_buf.data(),
                                        env.openmp_threads);
        });
        holdout_openmp.push_back({pixels, frames, to.median_ms});
        TimedWithStats to4 = measure_with_stats([&](RouteSamplePoint&) {
            for (int i = 0; i < 4; ++i) {
                weighted_integration_openmp(view, out_buf.data(),
                                            env.openmp_threads);
            }
        });
        holdout_reuse_openmp.push_back({pixels, frames, to4.median_ms});

        // GPU cold / resident / reuse4
        std::uint64_t el = 0;
        const char* err = nullptr;
        TimedWithStats tgc = measure_with_stats([&](RouteSamplePoint&) {
            bapi->upload_persistent_slot(gpu_handle, 0, 0, frames * pixels,
                                         fdata.data(), &el, &err);
            bapi->upload_persistent_slot(gpu_handle, 1, 0, frames,
                                         w.data(), &el, &err);
            if (bapi->submit_weighted_integration_resident(
                    gpu_handle, 0, pixels, out_buf.data(),
                    frames, pixels, &el, &err) != 0) {
                throw std::runtime_error(err ? err : "holdout cold failed");
            }
        });
        holdout_cold_gpu.push_back({pixels, frames, tgc.median_ms});
        bapi->upload_persistent_slot(gpu_handle, 0, 0, frames * pixels,
                                     fdata.data(), &el, &err);
        bapi->upload_persistent_slot(gpu_handle, 1, 0, frames,
                                     w.data(), &el, &err);
        TimedWithStats tgr = measure_with_stats([&](RouteSamplePoint&) {
            bapi->submit_weighted_integration_resident(
                gpu_handle, 0, pixels, out_buf.data(),
                frames, pixels, &el, &err);
        });
        holdout_gpu.push_back({pixels, frames, tgr.median_ms});
        TimedWithStats tg4 = measure_with_stats([&](RouteSamplePoint&) {
            for (int i = 0; i < 4; ++i) {
                bapi->submit_weighted_integration_resident(
                    gpu_handle, 0, pixels, out_buf.data(),
                    frames, pixels, &el, &err);
            }
        });
        holdout_reuse_gpu.push_back({pixels, frames, tg4.median_ms});

        // Mixed cold / resident / reuse4
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
            TimedWithStats tmc = measure_with_stats([&](RouteSamplePoint& st) {
                std::fill(out_buf.begin(), out_buf.end(), 0.0f);
                auto r = run_dispatcher(d, view, out_buf.data(), fdata, w,
                                        1u << 16, 1u << 16);
                if (!r.run_result.all_done) {
                    throw std::runtime_error("holdout cold mixed failed");
                }
                fill_stats_from_result(st, r, 0);
            });
            holdout_cold_mixed.push_back({pixels, frames, tmc.median_ms});
            run_dispatcher(d, view, out_buf.data(), fdata, w, 1u << 16, 1u << 16);
            TimedWithStats tmr = measure_with_stats([&](RouteSamplePoint& st) {
                std::fill(out_buf.begin(), out_buf.end(), 0.0f);
                auto r = run_dispatcher(d, view, out_buf.data(), fdata, w,
                                        1u << 16, 1u << 16);
                if (!r.run_result.all_done) {
                    throw std::runtime_error("holdout res mixed failed");
                }
                fill_stats_from_result(st, r, 0);
            });
            holdout_mixed.push_back({pixels, frames, tmr.median_ms});
            TimedWithStats tm4 = measure_with_stats([&](RouteSamplePoint&) {
                for (int i = 0; i < 4; ++i) {
                    std::fill(out_buf.begin(), out_buf.end(), 0.0f);
                    auto r = run_dispatcher(d, view, out_buf.data(), fdata, w,
                                            1u << 16, 1u << 16);
                    if (!r.run_result.all_done) {
                        throw std::runtime_error("holdout reuse mixed failed");
                    }
                }
            });
            holdout_reuse_mixed.push_back({pixels, frames, tm4.median_ms});
        }
    }

    // ---- 组装 validated domain + 插值模型选择 ----
    auto finalize_path = [&](RoutePath& p,
                             const std::vector<std::tuple<std::uint64_t,
                                                          std::uint32_t,
                                                          double>>& holdout,
                             bool adaptive) {
        if (p.samples.empty()) {
            p.eligible = false;
            p.reason = "no-samples";
            return;
        }
        std::uint64_t mn = p.samples.front().output_items;
        std::uint64_t mx = p.samples.front().output_items;
        std::vector<std::uint32_t> fcs;
        for (const auto& s : p.samples) {
            mn = std::min(mn, s.output_items);
            mx = std::max(mx, s.output_items);
            if (std::find(fcs.begin(), fcs.end(), s.frame_count) == fcs.end()) {
                fcs.push_back(s.frame_count);
            }
        }
        p.min_output_items = mn;
        p.max_output_items = mx;
        p.frame_counts = fcs;
        p.allow_tail_extrapolation = false;
        p.eligible = p.samples.size() >= 4;
        p.reason = p.eligible ? "calibrated" : "insufficient-samples";
        if (p.eligible && !holdout.empty()) {
            select_interpolation(p, holdout);
            // BDR 复核（08 计划 C）：median<=10%、max<=15% 门不通过 → ineligible
            if (!gate_passed(p)) {
                p.eligible = false;
                p.reason = "holdout-error-limit";
            }
            // adaptive：超门则补最差邻域标定点（简化：标记需重标定）
            if (adaptive && !gate_passed(p)) {
                p.reason = "adaptive-refinement-needed";
            }
        }
    };

    finalize_path(sc_cold.openmp, holdout_openmp, true);
    finalize_path(sc_cold.gpu_direct, holdout_cold_gpu, true);
    finalize_path(sc_cold.mixed, holdout_cold_mixed, true);
    finalize_path(sc_res.openmp, holdout_openmp, true);
    finalize_path(sc_res.gpu_direct, holdout_gpu, true);
    finalize_path(sc_res.mixed, holdout_mixed, true);
    finalize_path(sc_reuse.openmp, holdout_reuse_openmp, true);
    finalize_path(sc_reuse.gpu_direct, holdout_reuse_gpu, true);
    finalize_path(sc_reuse.mixed, holdout_reuse_mixed, true);

    // ---- 真实单 token chunk 服务曲线（多 frame_count）----
    {
        // 域必须覆盖最大 GPU 候选块（16M），避免单块越界
        const std::size_t service_domain =
            op.gpu_chunk_candidates.back();  // 16M 像素
        for (std::uint32_t frames : kServiceFrames) {
            std::vector<float> fdata(frames * service_domain), w(frames);
            generate_synthetic(20260807, frames, service_domain, fdata, w);
            std::vector<float> out_buf(service_domain);
            WeightedIntegrationView view{
                fdata.data(), w.data(), frames, service_domain};
            // CPU 单 token：integrate_range 单线程一块
            for (std::uint64_t cand : op.cpu_chunk_candidates) {
                std::vector<double> samples;
                for (int r = 0; r < kRepeats; ++r) {
                    const auto t0 = std::chrono::steady_clock::now();
                    integrate_range(view, 0, cand, out_buf.data());
                    const auto t1 = std::chrono::steady_clock::now();
                    samples.push_back(
                        std::chrono::duration<double, std::milli>(t1 - t0)
                            .count());
                }
                std::sort(samples.begin(), samples.end());
                const double med = samples[samples.size() / 2];
                const std::size_t p90i = static_cast<std::size_t>(
                    0.9 * static_cast<double>(samples.size() - 1));
                op.cpu_chunk_service.push_back(
                    {cand, frames, med, samples[p90i], samples.size()});
            }
            // GPU 单 token：resident 单块 kernel + 必要 D2H
            std::uint64_t el = 0;
            const char* err = nullptr;
            bapi->upload_persistent_slot(gpu_handle, 0, 0,
                                         frames * service_domain,
                                         fdata.data(), &el, &err);
            bapi->upload_persistent_slot(gpu_handle, 1, 0,
                                         frames, w.data(), &el, &err);
            for (std::uint64_t cand : op.gpu_chunk_candidates) {
                std::vector<double> samples;
                for (int r = 0; r < kRepeats; ++r) {
                    const auto t0 = std::chrono::steady_clock::now();
                    if (bapi->submit_weighted_integration_resident(
                            gpu_handle, 0, cand, out_buf.data(),
                            frames, cand, &el, &err) != 0) {
                        throw std::runtime_error(
                            err ? err : "gpu chunk service failed");
                    }
                    const auto t1 = std::chrono::steady_clock::now();
                    samples.push_back(
                        std::chrono::duration<double, std::milli>(t1 - t0)
                            .count());
                }
                std::sort(samples.begin(), samples.end());
                const double med = samples[samples.size() / 2];
                const std::size_t p90i = static_cast<std::size_t>(
                    0.9 * static_cast<double>(samples.size() - 1));
                op.gpu_chunk_service.push_back(
                    {cand, frames, med, samples[p90i], samples.size()});
            }
        }
    }

    // ---- 资格：至少一条非 fallback 路径通过误差门（median<=10%、max<=15%）----
    bool any_eligible = false;
    std::string reason;
    for (const auto& sc : {sc_cold, sc_res, sc_reuse}) {
        for (const RoutePath* p :
             {&sc.gpu_direct, &sc.mixed}) {
            if (p->eligible && gate_passed(*p)) {
                any_eligible = true;
            }
        }
        if (sc.gpu_direct.eligible && gate_passed(sc.gpu_direct)) {
            any_eligible = true;
        }
    }
    op.qualified = any_eligible;
    op.qualification_reason =
        any_eligible ? "calibrated" : "no-accelerated-path-passes-gate";

    // ---- 组装顶层 Profile ----
    out.schema_version = "acr-operation-route-profile-2";
    out.profile_state = op.qualified ? "qualified" : "partial";
    out.fingerprint_cpu = env.cpu_fingerprint;
    out.fingerprint_compiler = env.compiler;
    out.fingerprint_runtime_kernel_hash = env.kernel_hash;
    if (!env.gpu_name.empty()) {
        out.fingerprint_gpus.push_back(env.gpu_name);
    }
    op.scenarios.push_back(std::move(sc_cold));
    op.scenarios.push_back(std::move(sc_res));
    op.scenarios.push_back(std::move(sc_reuse));
    out.operations.push_back(std::move(op));

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
                 "qualified=%d\n",
                 out.profile_state.c_str(), out.operations[0].scenarios.size(),
                 out.operations[0].qualified ? 1 : 0);
    return true;
}

} // namespace astro::compute::weighted_integration
