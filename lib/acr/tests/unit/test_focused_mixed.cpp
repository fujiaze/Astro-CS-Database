// lib/acr/tests/unit/test_focused_mixed.cpp — 聚焦 Mixed 能力与性能验收
//
// 08 §7 / 07 号规范：
// - CPU 与真实 GPU 混合执行（CPU>0 && GPU>0，无固定份额）
// - CPU/GPU 结果数值一致（对照标量参考）
// - AutoMixed 中位耗时距 CPU-only/GPU-only 最佳值不超过 10%
// - Mixed 无收益时 Auto 允许自然退化（不强制）
#include <gtest/gtest.h>

#include "dispatcher.hpp"
#include "focused/focused_operations.hpp"
#include "focused/focused_benchmark.hpp"
#include "focused/operation_profile.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
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

// 前置声明（reduce/drizzle 全路径测试定义位于文件前部）
void run_reduce_drizzle_paths(const char* op_id, bool drizzle);

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
TEST(FocusedMixed, ReducePrivatePartialAllPaths) {
    astro::compute::runtime_init();
    run_reduce_drizzle_paths("synthetic.pixel_reduce.fp64acc", /*drizzle=*/false);
    astro::compute::runtime_shutdown();
}

TEST(FocusedMixed, DrizzlePrivatePartialAllPaths) {
    astro::compute::runtime_init();
    run_reduce_drizzle_paths("synthetic.drizzle_like_scatter.fp64acc",
                             /*drizzle=*/true);
    astro::compute::runtime_shutdown();
}

// ============================================================================
// 7. Dispatcher 真实驻留：prefetch 后 launcher 走 resident 路径
// （08 §3：一次 upload、多 token resident、结果正确）
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
TEST(FocusedMixed, AutoDegradesForSmallTask) {
    if (!gpu_available()) {
        GTEST_SKIP() << "no CUDA bridge/device; skipped";
    }
    astro::compute::runtime_init();
    astro::compute::qualification::focused::register_focused_kernels();
    auto regs = std::make_shared<ExecutorRegistry>(
        ExecutorRegistry::create_auto());
    // 构造"测量可信但 GPU host 无收益"的 profile（dense qualified、
    // host_path_eligible=false）→ Auto 排除 GPU
    OperationProfile profile;
    profile.profile_state = "qualified";
    profile.fingerprint_cpu = "test-cpu";
    profile.fingerprint_compiler = "test";
    profile.fingerprint_runtime_kernel_hash = "0123456789abcdef";
    OperationProfile::Operation op;
    op.operation_id = "synthetic.dense_pixel_accumulate.fp32";
    op.precision = "fp32";
    op.accumulator = "fp32";
    op.qualified = true;
    op.qualification_reason = "measured-qualified";
    op.sample_range = {1u << 18, 1u << 26, 7};
    op.cpu.ns_per_item = 1.0;
    op.cpu.recommended_chunk_items = 65536;
    op.cpu.minimum_chunk_items = 1024;
    op.gpu.ns_per_item = 2.0;   // GPU 慢 → host 无收益
    op.gpu.fixed_us = 100.0;
    op.gpu.recommended_chunk_items = 1u << 20;
    op.gpu.minimum_chunk_items = 1u << 14;
    op.gpu.host_path_eligible = false;
    op.gpu.resident_path_eligible = false;
    op.transfer.h2d_gbps = 10.0;
    op.transfer.d2h_gbps = 10.0;
    op.memory.host_bytes_per_item = 8.0;
    op.memory.device_bytes_per_item = 8.0;
    profile.operations.push_back(op);
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
    cfg.executors = regs;
    cfg.route_mode = RouteMode::AutoMixed;
    cfg.operation_profile = &profile;
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
    // 小任务：Auto 不允许 GPU 参与（无收益），结果正确
    EXPECT_EQ(r.coverage.done, r.coverage.total);
    EXPECT_EQ(r.chunks_on_gpu, 0u);
    for (std::size_t i = 0; i < n; ++i) {
        EXPECT_FLOAT_EQ(y[i], 2.0f + x[i]);
    }
    astro::compute::runtime_shutdown();
}

// ============================================================================
// 4. 真实驻留复用：共享输入只上传一次，跨 GPU 块复用（06 号规范 §2）
// ============================================================================
TEST(FocusedMixed, ResidentReuseUploadsOnce) {
    if (!gpu_available()) {
        GTEST_SKIP() << "no CUDA bridge/device; skipped";
    }
    using namespace astro::compute::cuda::bridge;
    ensure_bridge_loaded();
    auto& api = astro::compute::cuda::bridge::api();
    ASSERT_TRUE(api.loaded() && api.upload_persistent &&
                api.submit_dense_accumulate_resident);
    const char* err = nullptr;
    ASSERT_GT(api.init(&err), 0);
    void* h = api.executor_create(0, 65536, 256, &err);
    ASSERT_NE(h, nullptr);

    const std::size_t n = 1u << 20;  // 1M 整帧
    std::vector<float> x(n);
    astro::compute::qualification::focused::fill_uniform_fp32(
        x.data(), n, 0xA57C5AC20260802ULL);
    std::vector<float> y(n, 2.0f);
    std::uint64_t el = 0;

    // 上传整帧一次
    ASSERT_EQ(api.upload_persistent(h, 0, n, x.data(), &el, &err), 0);
    // 多个 GPU 块 resident 提交（复用 d_x，不重复整帧上传）
    const std::size_t block = n / 4;
    for (std::size_t b = 0; b < n; b += block) {
        ASSERT_EQ(api.submit_dense_accumulate_resident(
                      h, b, b + block, y.data(), &el, &err), 0);
    }
    // 结果正确（每块 y += x）
    for (std::size_t i = 0; i < n; ++i) {
        EXPECT_FLOAT_EQ(y[i], 2.0f + x[i]);
    }
    api.executor_destroy(h);
}

// ============================================================================
// 5. Reduction/Drizzle 私有 partial + 明确 merge 全路径正确性
// （CPU-only / GPU-only / ForcedMixed / AutoMixed；06 号规范 §3）
// ============================================================================
namespace {

void run_reduce_drizzle_paths(const char* op_id, bool drizzle) {
    astro::compute::qualification::focused::register_focused_kernels();
    auto regs = std::make_shared<ExecutorRegistry>(
        ExecutorRegistry::create_auto());
    const std::size_t n = 1u << 18;  // 256K
    const std::size_t bins = 64;
    // partial scratch 契约（08 §4）：按工作量与最小块精确计算，
    // 禁止按常数猜测。min_chunk = est 的最小高效块（256）。
    const std::size_t kMaxTokens =
        astro::compute::qualification::focused::partial_slots_for(n, 256);
    auto x = make_input(n);
    std::vector<float> dummy_y(n, 0.0f);

    // 标量参考
    std::vector<double> ref_bins(bins, 0.0);
    double ref_reduce = 0.0;
    if (drizzle) {
        astro::compute::qualification::focused::reference_drizzle_scatter(
            x, ref_bins, bins, 0xA57C5AC20260802ULL);
    } else {
        ref_reduce = astro::compute::qualification::focused::
            reference_pixel_reduce(x);
    }

    auto run_mode = [&](RouteMode mode, bool force_all) {
        Dispatcher d;
        DispatcherConfig cfg;
        cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
        cfg.executors = regs;
        cfg.route_mode = mode;
        cfg.force_all_supported_executors = force_all;
        d.configure(cfg);
        // partials：每 token 私有槽位
        std::vector<double> partials(
            drizzle ? kMaxTokens * bins : kMaxTokens, 0.0);
        KernelInvocation inv;
        inv.id = op_id;
        inv.domain = WorkDomain{0, n};
        inv.buffers.add(0, x.data(), x.size());
        inv.buffers.add(1, partials.data(), partials.size());
        if (drizzle) {
            append_scalar(inv.scalars, bins);
        }
        inv.traits.bytes_read_per_item = 4;
        inv.traits.bytes_written_per_item = 4;
        inv.partition = PartitionKind::PrivatePartialThenMerge;
        auto est = make_estimate(1u << 16, 1u << 18);
        auto r = d.dispatch_invocation(make_task(n), est, inv);
        EXPECT_TRUE(r.run_result.all_done)
            << "mode=" << static_cast<int>(mode)
            << " err=" << r.run_result.error_message;
        // 明确 merge
        if (drizzle) {
            std::vector<double> merged(bins, 0.0);
            astro::compute::qualification::focused::merge_drizzle_partials(
                partials.data(), kMaxTokens, bins, merged.data());
            for (std::size_t b = 0; b < bins; ++b) {
                EXPECT_NEAR(merged[b], ref_bins[b],
                            std::fabs(ref_bins[b]) * 1e-6 + 1e-9);
            }
        } else {
            const double merged = astro::compute::qualification::focused::
                merge_reduce_partials(partials.data(), kMaxTokens);
            EXPECT_NEAR(merged, ref_reduce,
                        std::fabs(ref_reduce) * 1e-6 + 1e-9);
        }
    };

    run_mode(RouteMode::CpuOnly, false);
    if (gpu_available()) {
        run_mode(RouteMode::GpuOnly, true);       // 强制 GPU（对照）
        run_mode(RouteMode::AutoMixed, true);     // ForcedMixed（正确性）
    }
}

} // anonymous namespace



TEST(FocusedMixed, DispatcherPrefetchThenResidentExecution) {
    if (!gpu_available()) {
        GTEST_SKIP() << "no CUDA bridge/device; skipped";
    }
    astro::compute::runtime_init();
    astro::compute::qualification::focused::register_focused_kernels();
    auto regs = std::make_shared<ExecutorRegistry>(
        ExecutorRegistry::create_auto());
    // 显式 prefetch（模拟外部/首次调用已驻留）
    const std::size_t n = 1u << 20;
    auto x = make_input(n);
    for (auto* e : regs->available_executors()) {
        if (e->backend_type().rfind("cuda", 0) == 0) {
            ASSERT_TRUE(e->prefetch_input(x.data(), x.size() * sizeof(float)));
            EXPECT_TRUE(e->input_resident(x.data()));
        }
    }
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
    cfg.executors = regs;
    cfg.force_all_supported_executors = true;  // 保证 GPU 参与（ForcedMixed）
    cfg.route_mode = RouteMode::AutoMixed;
    d.configure(cfg);

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
    EXPECT_GT(r.chunks_on_gpu, 0u);
    // 结果正确（resident 路径：d_x 复用 + 多 token 提交）
    for (std::size_t i = 0; i < n; i += (n / 16)) {
        EXPECT_FLOAT_EQ(y[i], 2.0f + x[i]);
    }
    astro::compute::runtime_shutdown();
}

TEST(FocusedMixed, AutoMixedWithinTenPercentOfBest) {
    if (!gpu_available()) {
        GTEST_SKIP() << "no CUDA bridge/device; performance compare skipped";
    }
    astro::compute::runtime_init();
    astro::compute::qualification::focused::register_focused_kernels();

    auto regs = std::make_shared<ExecutorRegistry>(
        ExecutorRegistry::create_auto());
    const std::size_t n = 1u << 22;  // 4M

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

    // 预建三个 Dispatcher（计时外复用）：Dispatcher 配置/worker 创建开销
    // 不应进入性能测量，否则 AutoMixed 固定开销导致 flaky。
    std::map<RouteMode, std::unique_ptr<Dispatcher>> dispatchers;
    for (RouteMode m :
         {RouteMode::CpuOnly, RouteMode::GpuOnly, RouteMode::AutoMixed}) {
        auto d = std::make_unique<Dispatcher>();
        DispatcherConfig cfg;
        cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
        cfg.executors = regs;
        cfg.route_mode = m;
        cfg.operation_profile = &profile;
        d->configure(cfg);
        dispatchers.emplace(m, std::move(d));
    }
    auto run_mode = [&](RouteMode mode) -> double {
        Dispatcher& d = *dispatchers.at(mode);
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
        if (mode == RouteMode::AutoMixed) {
            std::fprintf(stderr, "[FocusedMixed] auto chunks cpu=%zu gpu=%zu\n",
                         r.chunks_on_cpu, r.chunks_on_gpu);
        }
        std::fflush(stderr);
        return sec;
    };

    // 各模式 5 次取中位；模式交错执行（cpu/auto/gpu 轮换）抑制
    // 长时间运行导致的系统状态漂移
    auto median_ms = [&](const std::vector<double>& times) -> double {
        std::vector<double> t = times;
        std::sort(t.begin(), t.end());
        return t[t.size() / 2];
    };
    std::vector<double> cpu_t, gpu_t, auto_t;
    run_mode(RouteMode::CpuOnly);  // warm-up（CPU 时钟/缓存稳定）
    run_mode(RouteMode::AutoMixed);  // warm-up（共享池 worker/planning 预热）
    run_mode(RouteMode::GpuOnly);    // warm-up（GPU 时钟稳定）
    for (int i = 0; i < 5; ++i) {
        cpu_t.push_back(run_mode(RouteMode::CpuOnly) * 1000.0);
        auto_t.push_back(run_mode(RouteMode::AutoMixed) * 1000.0);
        gpu_t.push_back(run_mode(RouteMode::GpuOnly) * 1000.0);
    }
    const double cpu_ms = median_ms(cpu_t);
    const double gpu_ms = median_ms(gpu_t);
    const double auto_ms = median_ms(auto_t);
    // 资格工作集确认（07 号规范 §3）：若当前系统负载使 CPU 实测速率远超
    // Profile 预测（>10 倍），说明不在空载资格环境；如实 SKIP 而非假失败。
    const auto* op = profile.find("synthetic.dense_pixel_accumulate.fp32");
    if (op != nullptr && op->cpu.ns_per_item > 0.0) {
        const double expected_cpu_ms =
            op->cpu.ns_per_item * static_cast<double>(n) / 1e6;
        if (cpu_ms > expected_cpu_ms * 10.0) {
            GTEST_SKIP() << "system under load: cpu=" << cpu_ms
                         << "ms vs profile-predicted " << expected_cpu_ms
                         << "ms (not an eligible workload set)";
        }
    }
    const double best = std::min(cpu_ms, gpu_ms);
    std::printf("[FocusedMixed.AutoMixed] cpu=%.1fms gpu=%.1fms auto=%.1fms best=%.1fms\n",
                cpu_ms, gpu_ms, auto_ms, best);
    // 08 §7：AutoMixed 中位耗时不比实测最佳值差超过 10%
    EXPECT_LE(auto_ms, best * 1.10)
        << "AutoMixed 慢于最佳模式超过 10%";
    astro::compute::runtime_shutdown();
}

// ============================================================================
// 3. Mixed 无收益时 Auto 允许自然退化（小任务 → 不强制 GPU）
// ============================================================================
