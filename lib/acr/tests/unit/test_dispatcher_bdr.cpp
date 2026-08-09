// lib/acr/tests/unit/test_dispatcher_bdr.cpp
//
// Dispatcher Finalization（07 测试 D / CHECKLIST）：
//   - RouteProfileV2 + BenchmarkRouteEstimator 是 Dispatcher 顶层 Auto 唯一权威；
//   - 同一 Dispatcher 入口下，实际 launcher/设备 == chosen route：
//       OpenMP      → legacy launcher / CPU；
//       GPU Direct  → 仅 GPU executor（不创建 CPU worker）；
//       Mixed       → CPU+GPU 共享池（旧 planner 不做顶层资格）；
//   - unqualified scenario 实际回退 legacy OpenMP。
#include <gtest/gtest.h>

#include "dispatcher.hpp"

#include "routing/route_profile_v2.hpp"

#include "weighted_integration_kernels.hpp"

#include "astro/compute/kernel_registry.hpp"
#include "astro/compute/task_traits.hpp"
#include "../core/task_descriptor.hpp"
#include "../cost/cost_estimator.hpp"
#include "../backends/cuda/bridge/cuda_bridge_api.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace astro::compute;
using namespace astro::compute::scheduler;
using astro::compute::routing::ChunkServicePoint;
using astro::compute::routing::OperationRouteProfile;
using astro::compute::routing::RoutePath;
using astro::compute::routing::RouteProfileV2;
using astro::compute::routing::RouteSamplePoint;
using astro::compute::routing::RouteScenarioProfile;
using astro::compute::weighted_integration::kOperationId;

namespace {

constexpr const char* kOp = "synthetic.weighted_integration.fp64acc";

bool gpu_available() {
    astro::compute::cuda::bridge::ensure_bridge_loaded();
    auto& api = astro::compute::cuda::bridge::api();
    return api.loaded() && api.device_count() > 0;
}

// 构造指定最快路径的合成 qualified Profile（三场景全部 qualified）。
RouteProfileV2 make_profile(const char* fastest) {
    RouteProfileV2 p;
    p.schema_version = "acr-operation-route-profile-2";
    p.profile_state = "qualified";
    p.calibration_preset = "standard";
    p.calibration_head = "test-head";
    p.calibration_run_id = "test-run";
    p.fingerprint_cpu = "test-cpu";
    p.fingerprint_compiler = "gcc";
    p.fingerprint_runtime_kernel_hash = "0123456789abcdef0123456789abcdef";

    OperationRouteProfile op;
    op.operation_id = kOp;
    op.cpu_chunk_candidates = {1u << 16, 1u << 18};
    op.gpu_chunk_candidates = {1u << 18, 1u << 20};
    op.qualified = true;
    op.qualification_reason = "test";
    op.cpu_chunk_service.push_back({1u << 16, 16u, 0.4, 0.5, 7});
    op.cpu_chunk_service.push_back({1u << 18, 16u, 1.2, 1.5, 7});
    op.gpu_chunk_service.push_back({1u << 18, 16u, 0.5, 0.6, 7});
    op.gpu_chunk_service.push_back({1u << 20, 16u, 1.4, 1.7, 7});

    double ns_omp = 2.0, ns_gpu = 2.0, ns_mixed = 2.0;
    if (std::string(fastest) == "openmp") {
        ns_omp = 0.5;
    } else if (std::string(fastest) == "gpu") {
        ns_gpu = 0.5;
    } else {
        ns_mixed = 0.5;
    }

    const std::vector<std::uint64_t> items{1u << 18, 1u << 20, 1u << 22};
    const std::vector<std::uint32_t> frames{4u, 16u, 32u};
    auto fill = [&](RoutePath& path, double ns) {
        for (std::uint64_t n : items) {
            for (std::uint32_t f : frames) {
                RouteSamplePoint s;
                s.output_items = n;
                s.frame_count = f;
                s.reuse_count = 1;
                s.input_bytes = f * n * 4u + f * 4u;
                s.output_bytes = n * 4u;
                s.median_ms = n * ns * 1e-6 * (static_cast<double>(f) / 16.0);
                s.p90_ms = s.median_ms * 1.1;
                s.cpu_items = n;
                s.cpu_chunks = 1;
                s.gpu_items = n;
                s.gpu_chunks = 1;
                s.timed_d2h_bytes = n * 4u;
                s.absolute_peak_vram_bytes = n * 4u;
                path.samples.push_back(s);
            }
        }
        path.model_available = true;
        path.model_trusted = true;
        path.eligible = true;
        path.min_output_items = items.front();
        path.max_output_items = items.back();
        path.frame_counts = frames;
        path.final_holdout_count = 8;
        path.final_median_error_ratio = 0.03;
        path.final_max_error_ratio = 0.05;
        path.median_error_ratio = 0.03;
        path.max_error_ratio = 0.05;
        path.p95_error_ratio = 0.05;
        path.metrics_complete = true;
        path.adaptive_rounds_used = 1;
    };
    auto scene = [&](const char* id) {
        RouteScenarioProfile sc;
        sc.scenario_id = id;
        fill(sc.openmp, ns_omp);
        fill(sc.gpu_direct, ns_gpu);
        fill(sc.mixed, ns_mixed);
        sc.scenario_qualified = true;
        sc.routing_trusted = true;
        sc.qualification_reason = "test";
        sc.final_holdout_count = 8;
        sc.route_replay_count = 8;
        sc.route_replay_max_slowdown_ratio = 1.0;
        return sc;
    };
    op.scenarios.push_back(scene("cold_host_output"));
    op.scenarios.push_back(scene("resident_host_output"));
    op.scenarios.push_back(scene("resident_reuse4_host_output"));
    p.operations.push_back(std::move(op));
    return p;
}

cost::CostEstimate make_estimate(std::size_t rec_cpu, std::size_t rec_gpu) {
    cost::CostEstimate e;
    cost::DeviceCost dc;
    dc.device_id = kHwCpuDeviceId;
    dc.backend = "cpu";
    dc.recommended_chunk = rec_cpu;
    dc.min_effective_chunk = 256;
    dc.feasible = true;
    dc.profile_available = true;
    e.per_device.push_back(dc);
    cost::DeviceCost gdc;
    gdc.device_id = static_cast<DeviceId>(1);
    gdc.backend = "cuda:0";
    gdc.recommended_chunk = rec_gpu;
    gdc.min_effective_chunk = 256;
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

KernelInvocation make_inv(std::size_t n, std::uint32_t frames,
                          std::vector<float>& frames_buf,
                          std::vector<float>& weights,
                          std::vector<float>& out,
                          std::uint32_t reuse = 1,
                          const std::string& key_suffix = "",
                          std::size_t element_size = sizeof(float)) {
    KernelInvocation inv;
    inv.id = kOperationId;
    inv.domain = WorkDomain{0, n};
    inv.buffers.add(0, out.data(), n, 1, BufferRole::Output,
                    0, std::string("out") + key_suffix, element_size);
    inv.buffers.add(1, frames_buf.data(), frames_buf.size(), 1,
                    BufferRole::Input, 0,
                    std::string("frames") + key_suffix, element_size);
    inv.buffers.add(2, weights.data(), weights.size(), 1,
                    BufferRole::Input, 0,
                    std::string("weights") + key_suffix, element_size);
    append_scalar(inv.scalars, std::size_t{frames});
    append_scalar(inv.scalars, std::size_t{n});
    inv.traits.bytes_read_per_item = frames * 4u + 4u;
    inv.traits.bytes_written_per_item = 4;
    inv.traits.numeric.compute = NumericPolicy::Compute::fp32;
    inv.traits.numeric.accumulator = NumericPolicy::Accumulator::fp64;
    inv.partition = PartitionKind::IndependentOutputTiles;
    inv.frame_count = frames;
    inv.reuse_count_hint = reuse;
    return inv;
}

std::vector<float> make_frames(std::size_t n, std::uint32_t frames,
                               std::uint32_t seed) {
    std::vector<float> v(frames * n);
    std::uint32_t s = seed;
    for (auto& x : v) {
        s = s * 1664525u + 1013904223u;
        x = static_cast<float>((s >> 8) & 0xFFFF) / 65535.0f;
    }
    return v;
}

std::vector<float> make_weights(std::uint32_t frames, std::uint32_t seed) {
    std::vector<float> v(frames);
    std::uint32_t s = seed;
    for (auto& x : v) {
        s = s * 1664525u + 1013904223u;
        x = static_cast<float>((s >> 8) & 0xFFFF) / 65535.0f;
    }
    return v;
}

} // anonymous namespace

TEST(DispatcherBdr, OpenMPFastestRunsLegacyLauncher) {
    astro::compute::runtime_init();
    astro::compute::weighted_integration::register_weighted_integration_kernels();
    RouteProfileV2 profile = make_profile("openmp");

    auto regs = std::make_shared<ExecutorRegistry>(
        ExecutorRegistry::create_auto());
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
    cfg.executors = regs;
    cfg.route_mode = RouteMode::AutoMixed;
    cfg.route_profile_v2 = &profile;
    cfg.invocation_cpu_workers = 4;
    d.configure(cfg);

    const std::size_t n = 1u << 20;
    const std::uint32_t frames = 16u;
    auto fb = make_frames(n, frames, 0x1234u);
    auto w = make_weights(frames, 0x5678u);
    std::vector<float> out(n, 0.0f);
    KernelInvocation inv = make_inv(n, frames, fb, w, out);
    auto r = d.dispatch_invocation(make_task(n), make_estimate(1u << 16, 1u << 18),
                                   inv);
    EXPECT_TRUE(r.run_result.all_done);
    EXPECT_EQ(r.benchmark_route_decision, "openmp");
    EXPECT_EQ(r.actual_primary_backend, "cpu");
    EXPECT_EQ(r.chunks_on_gpu, 0u);
    EXPECT_EQ(r.actual_execution_shape, "legacy_openmp");
    ASSERT_EQ(r.per_device_stats.size(), 1u);
    EXPECT_EQ(r.per_device_stats[0].backend, "cpu");
    EXPECT_EQ(r.per_device_stats[0].items_done, n);
    // 数值非全零（正确性冒烟）
    bool nonzero = false;
    for (float v : out) {
        if (v != 0.0f) { nonzero = true; break; }
    }
    EXPECT_TRUE(nonzero);
    astro::compute::runtime_shutdown();
}

TEST(DispatcherBdr, UnqualifiedScenarioFallsBackOpenMP) {
    astro::compute::runtime_init();
    astro::compute::weighted_integration::register_weighted_integration_kernels();
    RouteProfileV2 profile = make_profile("gpu");
    profile.operations.front().scenarios[0].scenario_qualified = false;
    profile.operations.front().scenarios[0].routing_trusted = false;
    profile.operations.front().scenarios[0].qualification_reason =
        "replay-not-within-10";

    auto regs = std::make_shared<ExecutorRegistry>(
        ExecutorRegistry::create_auto());
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
    cfg.executors = regs;
    cfg.route_mode = RouteMode::AutoMixed;
    cfg.route_profile_v2 = &profile;
    cfg.invocation_cpu_workers = 4;
    d.configure(cfg);

    const std::size_t n = 1u << 20;
    const std::uint32_t frames = 16u;
    auto fb = make_frames(n, frames, 0x2222u);
    auto w = make_weights(frames, 0x3333u);
    std::vector<float> out(n, 0.0f);
    KernelInvocation inv = make_inv(n, frames, fb, w, out);
    inv.input_resident = false;  // cold_host_output
    auto r = d.dispatch_invocation(make_task(n), make_estimate(1u << 16, 1u << 18),
                                   inv);
    EXPECT_TRUE(r.run_result.all_done);
    EXPECT_EQ(r.benchmark_route_decision, "openmp");
    EXPECT_EQ(r.benchmark_route_reason,
              "scenario-not-qualified: cold_host_output");
    EXPECT_EQ(r.actual_primary_backend, "cpu");
    EXPECT_EQ(r.actual_execution_shape, "legacy_openmp");
    astro::compute::runtime_shutdown();
}

TEST(DispatcherBdr, GpuFastestRunsGpuOnly) {
    if (!gpu_available()) {
        GTEST_SKIP() << "no CUDA bridge/device; gpu direct skipped";
    }
    astro::compute::runtime_init();
    astro::compute::weighted_integration::register_weighted_integration_kernels();
    RouteProfileV2 profile = make_profile("gpu");

    auto regs = std::make_shared<ExecutorRegistry>(
        ExecutorRegistry::create_auto());
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
    cfg.executors = regs;
    cfg.route_mode = RouteMode::AutoMixed;
    cfg.route_profile_v2 = &profile;
    d.configure(cfg);

    const std::size_t n = 1u << 20;
    const std::uint32_t frames = 16u;
    auto fb = make_frames(n, frames, 0x4444u);
    auto w = make_weights(frames, 0x5555u);
    std::vector<float> out(n, 0.0f);
    // 第一次：cold（真实 Manager 无设备副本）→ GPU Direct 建立驻留
    KernelInvocation inv1 = make_inv(n, frames, fb, w, out, 1, "-gpu");
    auto r1 = d.dispatch_invocation(make_task(n),
                                    make_estimate(1u << 16, 1u << 18), inv1);
    ASSERT_TRUE(r1.run_result.all_done);
    ASSERT_EQ(r1.benchmark_route_decision, "gpu_direct");
    ASSERT_EQ(r1.actual_execution_shape, "gpu_direct");
    EXPECT_EQ(r1.benchmark_input_residency, "cold");
    EXPECT_GT(r1.benchmark_upload_required_bytes, 0u);
    // 第二次：同一 stable_key / 同一 Dispatcher → 真实 resident
    KernelInvocation inv2 = make_inv(n, frames, fb, w, out, 1, "-gpu");
    auto r = d.dispatch_invocation(make_task(n),
                                   make_estimate(1u << 16, 1u << 18), inv2);
    EXPECT_TRUE(r.run_result.all_done);
    EXPECT_EQ(r.benchmark_route_decision, "gpu_direct");
    EXPECT_EQ(r.actual_execution_shape, "gpu_direct");
    EXPECT_EQ(r.benchmark_input_residency, "resident");
    EXPECT_EQ(r.benchmark_upload_required_bytes, 0u);  // 真实复用，不重传
    EXPECT_EQ(r.chunks_on_cpu, 0u);
    EXPECT_GT(r.chunks_on_gpu, 0u);
    EXPECT_EQ(r.transfer_stats.h2d_bytes, 0u);
    bool nonzero = false;
    for (float v : out) {
        if (v != 0.0f) { nonzero = true; break; }
    }
    EXPECT_TRUE(nonzero);
    astro::compute::runtime_shutdown();
}

TEST(DispatcherBdr, MixedFastestRunsCpuAndGpu) {
    if (!gpu_available()) {
        GTEST_SKIP() << "no CUDA bridge/device; mixed skipped";
    }
    astro::compute::runtime_init();
    astro::compute::weighted_integration::register_weighted_integration_kernels();
    RouteProfileV2 profile = make_profile("mixed");

    auto regs = std::make_shared<ExecutorRegistry>(
        ExecutorRegistry::create_auto());
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
    cfg.executors = regs;
    cfg.route_mode = RouteMode::AutoMixed;
    cfg.route_profile_v2 = &profile;
    cfg.invocation_cpu_workers = 2;
    d.configure(cfg);

    const std::size_t n = 1u << 22;  // 4M：足够块数，避免 CPU worker 抢空池
    const std::uint32_t frames = 16u;
    auto fb = make_frames(n, frames, 0x6666u);
    auto w = make_weights(frames, 0x7777u);
    std::vector<float> out(n, 0.0f);
    CostAwareResult r;
    bool both_participated = false;
    // 第一次 cold → Mixed（真实 Manager 无设备副本）；第二次起真实 resident
    KernelInvocation inv1 = make_inv(n, frames, fb, w, out, 1, "-mix");
    r = d.dispatch_invocation(make_task(n),
                              make_estimate(1u << 16, 1u << 18), inv1);
    ASSERT_TRUE(r.run_result.all_done);
    ASSERT_EQ(r.benchmark_route_decision, "mixed");
    for (int attempt = 0; attempt < 3 && !both_participated; ++attempt) {
        std::fill(out.begin(), out.end(), 0.0f);
        KernelInvocation inv = make_inv(n, frames, fb, w, out, 1, "-mix");
        r = d.dispatch_invocation(make_task(n),
                                  make_estimate(1u << 16, 1u << 18), inv);
        both_participated = r.run_result.all_done &&
                            r.chunks_on_cpu > 0 && r.chunks_on_gpu > 0;
    }
    EXPECT_TRUE(r.run_result.all_done);
    EXPECT_EQ(r.benchmark_route_decision, "mixed");
    EXPECT_EQ(r.actual_execution_shape, "mixed_pool");
    EXPECT_EQ(r.benchmark_input_residency, "resident");
    EXPECT_TRUE(both_participated)
        << "BDR mixed route must actually engage CPU and GPU";
    EXPECT_NE(r.benchmark_cpu_chunk_items, 0u);
    EXPECT_NE(r.benchmark_gpu_chunk_items, 0u);
    bool nonzero = false;
    for (float v : out) {
        if (v != 0.0f) { nonzero = true; break; }
    }
    EXPECT_TRUE(nonzero);
    astro::compute::runtime_shutdown();
}

// ===== 04 号契约：generation 变化自动失效设备副本 =====
TEST(DispatcherBdr, GenerationChangeInvalidatesWeightsOnly) {
    if (!gpu_available()) {
        GTEST_SKIP() << "no CUDA bridge/device; generation test skipped";
    }
    astro::compute::runtime_init();
    astro::compute::weighted_integration::register_weighted_integration_kernels();
    RouteProfileV2 profile = make_profile("gpu");

    auto regs = std::make_shared<ExecutorRegistry>(
        ExecutorRegistry::create_auto());
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
    cfg.executors = regs;
    cfg.route_mode = RouteMode::AutoMixed;
    cfg.route_profile_v2 = &profile;
    d.configure(cfg);

    const std::size_t n = 1u << 20;
    const std::uint32_t frames = 16u;
    auto fb = make_frames(n, frames, 0x8888u);
    auto w = make_weights(frames, 0x9999u);
    std::vector<float> out(n, 0.0f);

    // 建立 frames + weights 驻留（cold → GPU Direct 上传一次）
    KernelInvocation inv1 = make_inv(n, frames, fb, w, out, 1, "-gen");
    auto r1 = d.dispatch_invocation(make_task(n),
                                    make_estimate(1u << 16, 1u << 18), inv1);
    ASSERT_TRUE(r1.run_result.all_done);
    ASSERT_EQ(r1.benchmark_route_decision, "gpu_direct");
    const std::uint64_t frames_uploads_after_first =
        r1.transfer_stats.frames_upload_count;
    ASSERT_GT(frames_uploads_after_first, 0u);

    // weights generation++（内容更新，同 stable_key）→ 仅 weights 需重传
    auto w2 = make_weights(frames, 0xAAA1u);
    KernelInvocation inv2 = make_inv(n, frames, fb, w2, out, 1, "-gen");
    inv2.buffers.bindings[2].generation = 1;  // weights 更新
    auto r2 = d.dispatch_invocation(make_task(n),
                                    make_estimate(1u << 16, 1u << 18), inv2);
    ASSERT_TRUE(r2.run_result.all_done);
    ASSERT_EQ(r2.benchmark_route_decision, "gpu_direct");
    ASSERT_EQ(r2.benchmark_input_residency, "cold");  // weights 失效 → 整体 cold
    EXPECT_GT(r2.transfer_stats.h2d_bytes, 0u);
    EXPECT_EQ(r2.transfer_stats.frames_upload_count,
              frames_uploads_after_first);  // frames 未重传

    // generation 未变 → 完全复用（无上传）
    KernelInvocation inv3 = make_inv(n, frames, fb, w2, out, 1, "-gen");
    inv3.buffers.bindings[2].generation = 1;
    auto r3 = d.dispatch_invocation(make_task(n),
                                    make_estimate(1u << 16, 1u << 18), inv3);
    ASSERT_TRUE(r3.run_result.all_done);
    ASSERT_EQ(r3.benchmark_route_decision, "gpu_direct");
    ASSERT_EQ(r3.benchmark_input_residency, "resident");
    EXPECT_EQ(r3.transfer_stats.h2d_bytes, 0u);
    astro::compute::runtime_shutdown();
}

// ===== 04 号契约：真实字节数（element_size_bytes）驱动记账 =====
TEST(DispatcherBdr, BufferBytesFollowElementSize) {
    astro::compute::runtime_init();
    astro::compute::weighted_integration::register_weighted_integration_kernels();

    const std::size_t n = 1u << 18;  // 256K
    const std::uint32_t frames = 4u;
    const std::size_t sizes[] = {1u, 4u, 8u};  // uint8/float/double 语义
    for (std::size_t es : sizes) {
        // OpenMP 最快 Profile：真实字节仍进入 benchmark_upload_required_bytes
        // （不触发 GPU 上传，避免非 float 字节数 overread；字节契约在
        //  Dispatcher 记账层验证）。
        RouteProfileV2 profile = make_profile("openmp");
        auto regs = std::make_shared<ExecutorRegistry>(
            ExecutorRegistry::create_auto());
        Dispatcher d;
        DispatcherConfig cfg;
        cfg.devices = {{"cpu", 0, 0, 50.0, true},
                       {"cuda:0", 1, 0, 500.0, true}};
        cfg.executors = regs;
        cfg.route_mode = RouteMode::AutoMixed;
        cfg.route_profile_v2 = &profile;
        d.configure(cfg);
        auto fb = make_frames(n, frames, 0xBBBBu);
        auto w = make_weights(frames, 0xCCCCu);
        std::vector<float> out(n, 0.0f);
        KernelInvocation inv =
            make_inv(n, frames, fb, w, out, 1,
                     "-bytes-" + std::to_string(es), es);
        auto r = d.dispatch_invocation(make_task(n),
                                       make_estimate(1u << 16, 1u << 18),
                                       inv);
        ASSERT_TRUE(r.run_result.all_done);
        ASSERT_EQ(r.benchmark_route_decision, "openmp");
        const std::uint64_t expected_input =
            (frames * n + frames) * es;
        EXPECT_EQ(r.benchmark_upload_required_bytes, expected_input)
            << "element_size=" << es;
        EXPECT_EQ(r.transfer_stats.h2d_bytes, 0u);  // OpenMP 不传 GPU
        EXPECT_EQ(r.transfer_stats.d2h_bytes, 0u);
        EXPECT_EQ(r.actual_execution_shape, "legacy_openmp");
    }

    // float（4 字节）真实 GPU Direct 执行：H2D/D2H 按真实字节记账
    if (gpu_available()) {
        RouteProfileV2 profile = make_profile("gpu");
        auto regs = std::make_shared<ExecutorRegistry>(
            ExecutorRegistry::create_auto());
        Dispatcher d;
        DispatcherConfig cfg;
        cfg.devices = {{"cpu", 0, 0, 50.0, true},
                       {"cuda:0", 1, 0, 500.0, true}};
        cfg.executors = regs;
        cfg.route_mode = RouteMode::AutoMixed;
        cfg.route_profile_v2 = &profile;
        d.configure(cfg);
        auto fb = make_frames(n, frames, 0xDDDDu);
        auto w = make_weights(frames, 0xEEEEu);
        std::vector<float> out(n, 0.0f);
        KernelInvocation inv = make_inv(n, frames, fb, w, out, 1, "-float", 4u);
        auto r = d.dispatch_invocation(make_task(n),
                                       make_estimate(1u << 16, 1u << 18),
                                       inv);
        ASSERT_TRUE(r.run_result.all_done);
        ASSERT_EQ(r.benchmark_route_decision, "gpu_direct");
        EXPECT_EQ(r.transfer_stats.h2d_bytes,
                  (frames * n + frames) * 4u);
        EXPECT_EQ(r.transfer_stats.d2h_bytes, n * 4u);
        EXPECT_EQ(r.actual_execution_shape, "gpu_direct");
    }
    astro::compute::runtime_shutdown();
}
