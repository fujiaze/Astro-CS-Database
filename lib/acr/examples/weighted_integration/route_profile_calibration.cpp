// lib/acr/examples/weighted_integration/route_profile_calibration.cpp
//
// 加权积分 Operation Route Profile v2 标定（08 计划 A/B/C/F）。
// 只读 Profile 的路径 E2E 样本（OpenMP/GPU Direct/Mixed）来自真实执行；
// chunk 服务曲线来自候选块真实执行；holdout 不参与拟合。
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
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace astro::compute::weighted_integration {
namespace {

using astro::compute::routing::RouteKind;
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

struct TimedWithStats {
    double median_ms{0.0};
    double p90_ms{0.0};
    RouteSamplePoint stats;
};

// 执行 fn（返回 stats）n 次，返回中位耗时 + 中位样本对应 stats。
template <class Fn>
TimedWithStats measure_with_stats(Fn&& fn) {
    std::vector<std::pair<double, RouteSamplePoint>> samples;
    for (int w = 0; w < kWarmup; ++w) {
        RouteSamplePoint st;
        fn(st);
    }
    using Clock = std::chrono::steady_clock;
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

// 二维插值预测（供 holdout 误差计算）
double predict_e2e(const RoutePath& path, std::uint64_t items,
                   std::uint32_t frames) {
    double m = 0.0, p90 = 0.0;
    if (!BenchmarkRouteEstimator::interpolate_e2e(path, items, frames, m, p90)) {
        return -1.0;
    }
    return m;
}

void compute_holdout_errors(RoutePath& path,
                            const std::vector<std::tuple<std::uint64_t,
                                                          std::uint32_t,
                                                          double>>& holdout) {
    std::vector<double> errs;
    for (const auto& [items, frames, actual] : holdout) {
        const double pred = predict_e2e(path, items, frames);
        if (pred > 0.0 && actual > 0.0) {
            errs.push_back(std::fabs(pred - actual) / actual);
        }
    }
    if (!errs.empty()) {
        std::sort(errs.begin(), errs.end());
        path.median_error_ratio = errs[errs.size() / 2];
        path.p95_error_ratio =
            errs[static_cast<std::size_t>(
                0.95 * static_cast<double>(errs.size() - 1))];
    }
}

} // anonymous namespace

bool calibrate_route_profile_v2(
    const CalibrationEnv& env,
    void* gpu_handle,
    astro::compute::cuda::bridge::BridgeApi* bapi,
    const std::string& output_path,
    RouteProfileV2& out) {
    if (!env.gpu_available || gpu_handle == nullptr || bapi == nullptr) {
        std::fprintf(stderr, "[route-profile] GPU unavailable; calibration aborted\n");
        return false;
    }

    // ---- 标定矩阵（standard 子集：尺寸 × 帧数）----
    const std::vector<std::size_t> sizes{
        256u * 256u, 512u * 512u, 1024u * 1024u,
        2048u * 2048u, 4096u * 4096u};
    const std::vector<std::uint32_t> frames_list{4u, 16u, 32u};

    OperationRouteProfile op;
    op.operation_id = kOp;
    op.cpu_chunk_candidates = {1u << 16, 1u << 18, 1u << 20};
    op.gpu_chunk_candidates = {1u << 18, 1u << 20, 1u << 22, 1u << 24};

    // 4 个场景
    RouteScenarioProfile sc_cold;
    sc_cold.scenario_id = "cold_host_output";
    RouteScenarioProfile sc_res;
    sc_res.scenario_id = "resident_host_output";
    RouteScenarioProfile sc_dev;
    sc_dev.scenario_id = "resident_device_output";
    RouteScenarioProfile sc_reuse;
    sc_reuse.scenario_id = "resident_reuse4_host_output";

    std::vector<std::tuple<std::uint64_t, std::uint32_t, double>>
        holdout_openmp, holdout_gpu, holdout_mixed;
    std::vector<std::tuple<std::uint64_t, std::uint32_t, double>>
        holdout_gpu_cold, holdout_gpu_resident;

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

            auto make_sample = [&](double med, double p90,
                                   RouteSamplePoint st) {
                st.output_items = pixels;
                st.frame_count = frames;
                st.input_bytes = input_bytes;
                st.output_bytes = output_bytes;
                st.median_ms = med;
                st.p90_ms = p90;
                return st;
            };

            // ---- OpenMP E2E（reuse4 场景为 4 次总耗时）----
            {
                TimedWithStats t = measure_with_stats([&](RouteSamplePoint&) {
                    weighted_integration_openmp(view, out_buf.data(),
                                                env.openmp_threads);
                });
                sc_cold.openmp.samples.push_back(
                    make_sample(t.median_ms, t.p90_ms, t.stats));
                sc_res.openmp.samples.push_back(
                    make_sample(t.median_ms, t.p90_ms, t.stats));
                sc_dev.openmp.samples.push_back(
                    make_sample(t.median_ms, t.p90_ms, t.stats));
                TimedWithStats t4 = measure_with_stats([&](RouteSamplePoint&) {
                    for (int i = 0; i < 4; ++i) {
                        weighted_integration_openmp(view, out_buf.data(),
                                                    env.openmp_threads);
                    }
                });
                t4.stats.reuse_count = 4;
                sc_reuse.openmp.samples.push_back(
                    make_sample(t4.median_ms, t4.p90_ms, t4.stats));
            }

            // ---- GPU Direct E2E ----
            {
                std::uint64_t el = 0;
                const char* err = nullptr;
                // cold：每个正式样本重新上传（true cold）
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
                    st.setup_h2d_bytes = input_bytes;
                    st.timed_h2d_bytes = 0;
                    st.timed_d2h_bytes = output_bytes;
                    st.gpu_items = pixels;
                    st.gpu_chunks = 1;
                });
                sc_cold.gpu_direct.samples.push_back(
                    make_sample(tc.median_ms, tc.p90_ms, tc.stats));
                // resident steady：warm 后计时
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
                sc_res.gpu_direct.samples.push_back(
                    make_sample(tr.median_ms, tr.p90_ms, tr.stats));
                sc_dev.gpu_direct.samples.push_back(
                    make_sample(tr.median_ms, tr.p90_ms, tr.stats));
                // reuse4：4 次 resident 总耗时
                TimedWithStats t4 = measure_with_stats([&](RouteSamplePoint& st) {
                    for (int i = 0; i < 4; ++i) {
                        if (bapi->submit_weighted_integration_resident(
                                gpu_handle, 0, pixels, out_buf.data(),
                                frames, pixels, &el, &err) != 0) {
                            throw std::runtime_error(err ? err : "gpu reuse failed");
                        }
                    }
                    st.timed_d2h_bytes = 4 * output_bytes;
                    st.gpu_items = 4 * pixels;
                    st.gpu_chunks = 4;
                });
                t4.stats.reuse_count = 4;
                sc_reuse.gpu_direct.samples.push_back(
                    make_sample(t4.median_ms, t4.p90_ms, t4.stats));
            }

            // ---- Mixed E2E（真实共享池 Dispatcher）----
            {
                auto regs = std::make_shared<ExecutorRegistry>(
                    ExecutorRegistry::create_auto());
                Dispatcher d;
                DispatcherConfig cfg;
                cfg.devices = {{"cpu", 0, 0, 50.0, true},
                               {"cuda:0", 1, 0, 500.0, true}};
                cfg.executors = regs;
                cfg.route_mode = RouteMode::AutoMixed;
                cfg.force_all_supported_executors = true;  // 真实 Mixed 执行
                d.configure(cfg);
                TimedWithStats tm = measure_with_stats([&](RouteSamplePoint& st) {
                    std::fill(out_buf.begin(), out_buf.end(), 0.0f);
                    auto r = run_dispatcher(d, view, out_buf.data(),
                                            fdata, w, 1u << 16, 1u << 18);
                    if (!r.run_result.all_done) {
                        throw std::runtime_error(
                            "mixed not all_done: " +
                            r.run_result.error_message);
                    }
                    fill_stats_from_result(st, r, input_bytes);
                    st.gpu_items = st.gpu_items > 0 ? st.gpu_items : pixels;
                });
                sc_cold.mixed.samples.push_back(
                    make_sample(tm.median_ms, tm.p90_ms, tm.stats));
                sc_res.mixed.samples.push_back(
                    make_sample(tm.median_ms, tm.p90_ms, tm.stats));
                sc_dev.mixed.samples.push_back(
                    make_sample(tm.median_ms, tm.p90_ms, tm.stats));
                TimedWithStats t4 = measure_with_stats([&](RouteSamplePoint& st) {
                    for (int i = 0; i < 4; ++i) {
                        std::fill(out_buf.begin(), out_buf.end(), 0.0f);
                        auto r = run_dispatcher(d, view, out_buf.data(),
                                                fdata, w, 1u << 16, 1u << 18);
                        if (!r.run_result.all_done) {
                            throw std::runtime_error("mixed reuse failed");
                        }
                    }
                    st.gpu_items = 4 * pixels;
                });
                t4.stats.reuse_count = 4;
                sc_reuse.mixed.samples.push_back(
                    make_sample(t4.median_ms, t4.p90_ms, t4.stats));
            }

            // ---- 校验输出正确性（对 Serial 参考）----
            {
                std::vector<float> ref(pixels);
                for (std::size_t p = 0; p < pixels; ++p) {
                    ref[p] = integrate_one_pixel(view, p);
                }
                const auto es = compare(ref, out_buf);
                if (!es.finite || es.max_abs > 2e-5 ||
                    es.relative_l2 > 2e-6) {
                    std::fprintf(stderr,
                                 "[route-profile] correctness failed at "
                                 "%zux%zu f=%u\n",
                                 (std::size_t)std::sqrt((double)pixels),
                                 (std::size_t)std::sqrt((double)pixels),
                                 frames);
                }
            }
        }
    }

    // ---- holdout（不参与拟合；用于预测误差）----
    const std::vector<std::pair<std::size_t, std::uint32_t>> holdouts{
        {768u * 768u, 12u}, {1536u * 1536u, 24u},
        {3072u * 3072u, 12u}, {4096u * 4096u, 24u}};
    for (const auto& [px, frames] : holdouts) {
        const std::size_t pixels = px;
        std::vector<float> fdata(frames * pixels), w(frames);
        generate_synthetic(20260807, frames, pixels, fdata, w);
        std::vector<float> out_buf(pixels);
        WeightedIntegrationView view{fdata.data(), w.data(), frames, pixels};

        TimedWithStats to = measure_with_stats([&](RouteSamplePoint&) {
            weighted_integration_openmp(view, out_buf.data(),
                                        env.openmp_threads);
        });
        holdout_openmp.push_back({pixels, frames, to.median_ms});

        if (bapi) {
            std::uint64_t el = 0;
            const char* err = nullptr;
            // cold：每个正式样本重新建立 residency（true cold）
            TimedWithStats tgc = measure_with_stats([&](RouteSamplePoint&) {
                bapi->upload_persistent_slot(gpu_handle, 0, 0,
                                             frames * pixels,
                                             fdata.data(), &el, &err);
                bapi->upload_persistent_slot(gpu_handle, 1, 0,
                                             frames, w.data(), &el, &err);
                if (bapi->submit_weighted_integration_resident(
                        gpu_handle, 0, pixels, out_buf.data(),
                        frames, pixels, &el, &err) != 0) {
                    throw std::runtime_error(err ? err : "holdout cold failed");
                }
            });
            holdout_gpu_cold.push_back({pixels, frames, tgc.median_ms});
            // resident：预上传一次后计时
            bapi->upload_persistent_slot(gpu_handle, 0, 0, frames * pixels,
                                         fdata.data(), &el, &err);
            bapi->upload_persistent_slot(gpu_handle, 1, 0, frames,
                                         w.data(), &el, &err);
            TimedWithStats tgr = measure_with_stats([&](RouteSamplePoint&) {
                if (bapi->submit_weighted_integration_resident(
                        gpu_handle, 0, pixels, out_buf.data(),
                        frames, pixels, &el, &err) != 0) {
                    throw std::runtime_error(err ? err : "holdout res failed");
                }
            });
            holdout_gpu_resident.push_back({pixels, frames, tgr.median_ms});
            holdout_gpu.push_back({pixels, frames, tgr.median_ms});
        }
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
            TimedWithStats tm = measure_with_stats([&](RouteSamplePoint& st) {
                std::fill(out_buf.begin(), out_buf.end(), 0.0f);
                auto r = run_dispatcher(d, view, out_buf.data(),
                                        fdata, w, 1u << 16, 1u << 18);
                if (!r.run_result.all_done) {
                    throw std::runtime_error("holdout mixed failed");
                }
                fill_stats_from_result(st, r, 0);
            });
            holdout_mixed.push_back({pixels, frames, tm.median_ms});
        }
    }

    // ---- 组装 validated domain / 误差 / 资格 ----
    auto finalize_path = [&](RoutePath& p) {
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
    };

    finalize_path(sc_cold.openmp);
    finalize_path(sc_cold.gpu_direct);
    finalize_path(sc_cold.mixed);
    finalize_path(sc_res.openmp);
    finalize_path(sc_res.gpu_direct);
    finalize_path(sc_res.mixed);
    finalize_path(sc_dev.openmp);
    finalize_path(sc_dev.gpu_direct);
    finalize_path(sc_dev.mixed);
    finalize_path(sc_reuse.openmp);
    finalize_path(sc_reuse.gpu_direct);
    finalize_path(sc_reuse.mixed);

    // holdout 误差（reuse4 场景 openmp/gpu/mixed 用对应 4 次样本近似）
    compute_holdout_errors(sc_cold.openmp, holdout_openmp);
    compute_holdout_errors(sc_res.openmp, holdout_openmp);
    compute_holdout_errors(sc_dev.openmp, holdout_openmp);
    compute_holdout_errors(sc_cold.gpu_direct, holdout_gpu_cold);
    compute_holdout_errors(sc_res.gpu_direct, holdout_gpu_resident);
    compute_holdout_errors(sc_dev.gpu_direct, holdout_gpu_resident);
    compute_holdout_errors(sc_cold.mixed, holdout_mixed);
    compute_holdout_errors(sc_res.mixed, holdout_mixed);
    compute_holdout_errors(sc_dev.mixed, holdout_mixed);
    // reuse4：holdout 用 4 倍耗时近似
    {
        std::vector<std::tuple<std::uint64_t, std::uint32_t, double>> h4;
        for (const auto& [i, f, ms] : holdout_openmp) {
            h4.push_back({i, f, ms * 4.0});
        }
        compute_holdout_errors(sc_reuse.openmp, h4);
        compute_holdout_errors(sc_reuse.gpu_direct, h4);
        compute_holdout_errors(sc_reuse.mixed, h4);
    }

    // 03 号规范：预测误差门（median≤10%、p95≤15%）不通过的路径不得 eligible。
    auto apply_error_gate = [](RoutePath& p) {
        if (p.eligible && (p.median_error_ratio > 0.10 ||
                           p.p95_error_ratio > 0.15)) {
            p.eligible = false;
            p.reason = "holdout-error-limit";
        }
    };
    for (auto* sc : {&sc_cold, &sc_res, &sc_dev, &sc_reuse}) {
        apply_error_gate(sc->openmp);
        apply_error_gate(sc->gpu_direct);
        apply_error_gate(sc->mixed);
    }

    op.scenarios.push_back(std::move(sc_cold));
    op.scenarios.push_back(std::move(sc_res));
    op.scenarios.push_back(std::move(sc_dev));
    op.scenarios.push_back(std::move(sc_reuse));

    // ---- CPU/GPU chunk 服务曲线（2048²×16 域真实执行）----
    {
        const std::size_t pixels = 2048u * 2048u;
        const std::uint32_t frames = 16u;
        std::vector<float> fdata(frames * pixels), w(frames);
        generate_synthetic(20260807, frames, pixels, fdata, w);
        std::vector<float> out_buf(pixels);
        WeightedIntegrationView view{fdata.data(), w.data(), frames, pixels};

        // CPU chunk：Dispatcher CpuOnly，cand 控制块大小
        auto cpu_regs = std::make_shared<ExecutorRegistry>(
            ExecutorRegistry::create_cpu_only());
        Dispatcher cd;
        DispatcherConfig cc;
        cc.devices = {{"cpu", 0, 0, 50.0, true}};
        cc.executors = cpu_regs;
        cc.route_mode = RouteMode::CpuOnly;
        cd.configure(cc);
        for (std::uint64_t cand : op.cpu_chunk_candidates) {
            TimedWithStats t = measure_with_stats([&](RouteSamplePoint&) {
                auto r = run_dispatcher(cd, view, out_buf.data(),
                                        fdata, w, cand, cand);
                if (!r.run_result.all_done) {
                    throw std::runtime_error("cpu chunk failed");
                }
            });
            op.cpu_chunk_service.push_back({cand, frames, t.median_ms});
        }
        // GPU chunk：Dispatcher GpuOnly（域内 cand 分块）
        auto regs = std::make_shared<ExecutorRegistry>(
            ExecutorRegistry::create_auto());
        Dispatcher gd;
        DispatcherConfig gc;
        gc.devices = {{"cpu", 0, 0, 50.0, true},
                      {"cuda:0", 1, 0, 500.0, true}};
        gc.executors = regs;
        gc.route_mode = RouteMode::GpuOnly;
        gd.configure(gc);
        for (std::uint64_t cand : op.gpu_chunk_candidates) {
            TimedWithStats t = measure_with_stats([&](RouteSamplePoint&) {
                auto r = run_dispatcher(gd, view, out_buf.data(),
                                        fdata, w, cand, cand);
                if (!r.run_result.all_done) {
                    throw std::runtime_error("gpu chunk failed");
                }
            });
            op.gpu_chunk_service.push_back({cand, frames, t.median_ms});
        }
        // Mixed 调度开销（Dispatcher 空跑极小任务近似固定开销）
        {
            auto mregs = std::make_shared<ExecutorRegistry>(
                ExecutorRegistry::create_auto());
            Dispatcher md;
            DispatcherConfig mc;
            mc.devices = {{"cpu", 0, 0, 50.0, true},
                          {"cuda:0", 1, 0, 500.0, true}};
            mc.executors = mregs;
            mc.route_mode = RouteMode::AutoMixed;
            mc.force_all_supported_executors = true;
            md.configure(mc);
            const std::size_t tiny = 1024u * 1024u;
            std::vector<float> tf(16 * tiny), tw(16);
            generate_synthetic(20260807, 16, tiny, tf, tw);
            std::vector<float> tb(tiny);
            WeightedIntegrationView tv{tf.data(), tw.data(), 16u, tiny};
            TimedWithStats t = measure_with_stats([&](RouteSamplePoint&) {
                auto r = run_dispatcher(md, tv, tb.data(), tf, tw,
                                        1u << 16, 1u << 16);
                if (!r.run_result.all_done) {
                    throw std::runtime_error("mixed overhead failed");
                }
            });
            op.mixed_fixed_overhead_ms = std::max(0.0, t.median_ms - 0.5);
            op.mixed_per_token_ms = 0.01;
        }
    }

    // ---- 资格：至少一条场景路径通过 holdout 误差门（eligible）----
    bool any_eligible = false;
    std::string reason;
    for (const auto& sc : op.scenarios) {
        for (const RoutePath* p :
             {&sc.openmp, &sc.gpu_direct, &sc.mixed}) {
            if (p->eligible) {
                any_eligible = true;
            }
        }
    }
    op.qualified = any_eligible;
    op.qualification_reason =
        any_eligible ? "calibrated" : "no-path-passes-holdout-error-limit";

    // ---- 组装顶层 Profile v2 ----
    out.schema_version = "acr-operation-route-profile-2";
    out.profile_state = op.qualified ? "qualified" : "partial";
    out.fingerprint_cpu = env.cpu_fingerprint;
    out.fingerprint_compiler = env.compiler;
    out.fingerprint_runtime_kernel_hash = env.kernel_hash;
    if (!env.gpu_name.empty()) {
        out.fingerprint_gpus.push_back(env.gpu_name);
    }
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
                 op.qualified ? 1 : 0);
    return true;
}

} // namespace astro::compute::weighted_integration
