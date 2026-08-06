// lib/acr/tests/unit/test_focused_mixed.cpp — 聚焦 Mixed 能力与性能验收
//
// 08 号计划 §7 / 07 号规范：
//   - CPU 与真实 GPU 混合执行（CPU>0 && GPU>0，无固定份额）
//   - CPU/GPU 结果数值一致（对照标量参考）
//   - AutoMixed 中位耗时距 CPU-only/GPU-only 最佳值不超过 10%
//   - Mixed 无收益时 Auto 允许自然退化（不强制）
#include <gtest/gtest.h>

#include "dispatcher.hpp"
#include "focused/focused_operations.hpp"
#include "focused/focused_benchmark.hpp"
#include "focused/operation_profile.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "astro/compute/kernel_registry.hpp"
#include "../backends/cuda/bridge/cuda_bridge_api.hpp"
#include "../core/task_descriptor.hpp"
#include "../cost/cost_estimator.hpp"

using namespace astro::compute;
using namespace astro::compute::scheduler;
using astro::compute::qualification::focused::FocusedProfileKind;
using astro::compute::qualification::focused::FocusedBenchmark;
using astro::compute::qualification::focused::OperationProfile;

namespace {

bool gpu_available() {
    astro::compute::cuda::bridge::ensure_bridge_loaded();
    auto& api = astro::compute::cuda::bridge::api();
    return api.loaded() && api.device_count() > 0;
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

std::vector<float> make_input(std::size_t n) {
    std::vector<float> x(n);
    astro::compute::qualification::focused::fill_uniform_fp32(
        x.data(), n, 0xA57C5AC20260802ULL);
    return x;
}

} // anonymous namespace

// ============================================================================
// 1. Mixed 能力：CPU 与真实 GPU 均完成非零工作（无固定份额）
// ============================================================================
TEST(FocusedMixed, CpuAndGpuBothWork) {
    if (!gpu_available()) {
        GTEST_SKIP() << "no CUDA bridge/device; real mixed skipped";
    }
    astro::compute::runtime_init();
    astro::compute::qualification::focused::register_focused_kernels();

    auto regs = std::make_shared<ExecutorRegistry>(
        ExecutorRegistry::create_auto());
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
    cfg.executors = regs;
    cfg.force_all_supported_executors = true;  // 测试专用：保证 Mixed 发生
    cfg.route_mode = RouteMode::AutoMixed;
    d.configure(cfg);

    const std::size_t n = 1u << 20;  // 1M
    auto x = make_input(n);
    std::vector<float> y(n, 2.0f);
    KernelInvocation inv;
    inv.id = "synthetic.dense_pixel_accumulate.fp32";
    inv.domain = WorkDomain{0, n};
    inv.buffers.add(0, y.data(), y.size());
    inv.buffers.add(1, x.data(), x.size());
    inv.traits.bytes_read_per_item = 4;
    inv.traits.bytes_written_per_item = 4;
    inv.partition = PartitionKind::IndependentOutputTiles;

    auto est = make_estimate(1u << 16, 1u << 18);
    auto r = d.dispatch_invocation(make_task(n), est, inv);
    EXPECT_TRUE(r.run_result.all_done);
    EXPECT_GT(r.chunks_on_cpu, 0u);
    EXPECT_GT(r.chunks_on_gpu, 0u);
    EXPECT_EQ(r.coverage.done, r.coverage.total);
    // 数值对照：y[i] == 2 + x[i]（FP32 累加）
    for (std::size_t i = 0; i < n; i += (n / 16)) {
        EXPECT_FLOAT_EQ(y[i], 2.0f + x[i]);
    }
    astro::compute::runtime_shutdown();
}

// ============================================================================
// 2. AutoMixed 性能验收：中位耗时距最佳模式 ≤ 10%
// ============================================================================
TEST(FocusedMixed, AutoMixedWithinTenPercentOfBest) {
    if (!gpu_available()) {
        GTEST_SKIP() << "no CUDA bridge/device; performance compare skipped";
    }
    astro::compute::runtime_init();
    astro::compute::qualification::focused::register_focused_kernels();

    auto regs = std::make_shared<ExecutorRegistry>(
        ExecutorRegistry::create_auto());
    const std::size_t n = 1u << 21;  // 2M（足够摊薄 GPU 传输）

    // 先构建 OperationProfile（standard，真实 GPU）
    FocusedBenchmark bench;
    bench.run(FocusedProfileKind::Standard, /*enable_gpu=*/true);
    std::fprintf(stderr, "[FocusedMixed] benchmark done\n");
    std::fflush(stderr);
    OperationProfile profile = bench.build_profile(FocusedProfileKind::Standard);
    bench.qualify(FocusedProfileKind::Standard, profile);
    std::fprintf(stderr, "[FocusedMixed] profile built state=%s\n",
                 profile.profile_state.c_str());
    std::fflush(stderr);

    auto run_mode = [&](RouteMode mode) -> double {
        Dispatcher d;
        DispatcherConfig cfg;
        cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
        cfg.executors = regs;
        cfg.route_mode = mode;
        cfg.operation_profile = &profile;
        d.configure(cfg);
        auto x = make_input(n);
        std::vector<float> y(n, 2.0f);
        KernelInvocation inv;
        inv.id = "synthetic.dense_pixel_accumulate.fp32";
        inv.domain = WorkDomain{0, n};
        inv.buffers.add(0, y.data(), y.size());
        inv.buffers.add(1, x.data(), x.size());
        inv.traits.bytes_read_per_item = 4;
        inv.traits.bytes_written_per_item = 4;
        inv.partition = PartitionKind::IndependentOutputTiles;
        auto est = make_estimate(1u << 16, 1u << 18);
        const auto t0 = std::chrono::steady_clock::now();
        auto r = d.dispatch_invocation(make_task(n), est, inv);
        const auto t1 = std::chrono::steady_clock::now();
        EXPECT_TRUE(r.run_result.all_done);
        const double sec = std::chrono::duration<double>(t1 - t0).count();
        std::fprintf(stderr, "[FocusedMixed] mode=%d all_done=%d time=%.3fs\n",
                     static_cast<int>(mode), r.run_result.all_done, sec);
        std::fflush(stderr);
        return sec;
    };

    // 各模式 5 次取中位（降低系统负载噪声）
    auto median_ms = [&](RouteMode m) -> double {
        std::vector<double> times;
        for (int i = 0; i < 5; ++i) {
            times.push_back(run_mode(m) * 1000.0);
        }
        std::sort(times.begin(), times.end());
        return times[2];
    };

    const double cpu_ms = median_ms(RouteMode::CpuOnly);
    const double gpu_ms = median_ms(RouteMode::GpuOnly);
    const double auto_ms = median_ms(RouteMode::AutoMixed);
    const double best = std::min(cpu_ms, gpu_ms);
    std::printf("[FocusedMixed.AutoMixed] cpu=%.1fms gpu=%.1fms auto=%.1fms best=%.1fms\n",
                cpu_ms, gpu_ms, auto_ms, best);
    // 08 号计划 §7：AutoMixed 中位耗时不比实测最佳值差超过 10%
    EXPECT_LE(auto_ms, best * 1.10)
        << "AutoMixed 慢于最佳模式超过 10%";
    astro::compute::runtime_shutdown();
}

// ============================================================================
// 3. Mixed 无收益时 Auto 允许自然退化（小任务 → 不强制 GPU）
// ============================================================================
TEST(FocusedMixed, AutoDegradesForSmallTask) {
    if (!gpu_available()) {
        GTEST_SKIP() << "no CUDA bridge/device; skipped";
    }
    astro::compute::runtime_init();
    astro::compute::qualification::focused::register_focused_kernels();
    auto regs = std::make_shared<ExecutorRegistry>(
        ExecutorRegistry::create_auto());
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
    cfg.executors = regs;
    cfg.route_mode = RouteMode::AutoMixed;
    d.configure(cfg);

    const std::size_t n = 4096;  // 小任务：GPU 传输不划算
    auto x = make_input(n);
    std::vector<float> y(n, 2.0f);
    KernelInvocation inv;
    inv.id = "synthetic.dense_pixel_accumulate.fp32";
    inv.domain = WorkDomain{0, n};
    inv.buffers.add(0, y.data(), y.size());
    inv.buffers.add(1, x.data(), x.size());
    inv.traits.bytes_read_per_item = 4;
    inv.traits.bytes_written_per_item = 4;
    auto est = make_estimate(256, 1024);
    auto r = d.dispatch_invocation(make_task(n), est, inv);
    EXPECT_TRUE(r.run_result.all_done);
    // 小任务无 OperationProfile 时走 CPU；允许 GPU 不参与但结果正确
    EXPECT_EQ(r.coverage.done, r.coverage.total);
    for (std::size_t i = 0; i < n; ++i) {
        EXPECT_FLOAT_EQ(y[i], 2.0f + x[i]);
    }
    astro::compute::runtime_shutdown();
}
