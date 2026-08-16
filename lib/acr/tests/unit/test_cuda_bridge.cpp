// lib/acr/tests/unit/test_cuda_bridge.cpp — CUDA 桥接真实 GPU 测试
//
// 23 §6：
// - 无 GPU/无桥接 DLL → GTEST_SKIP（不得返回 correct=true 冒充通过）；
// - GPU-only：AXPY/COPY/REDUCE/CONV3x3 经 KernelRegistry + 真实 CUDA launcher；
// - 真实 Mixed：CPU 与 GPU 均完成非零工作（cpu_done>0 && gpu_done>0）；
// - actual device ID 来自 completion（SubmitHandle.device）。
#include <gtest/gtest.h>

#include "dispatcher.hpp"
#include "device_executor.hpp"
#include "../backends/classic/classic_kernels.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "astro/compute/kernel_registry.hpp"
#include "../core/task_descriptor.hpp"
#include "../cost/cost_estimator.hpp"

using namespace astro::compute;
using namespace astro::compute::scheduler;

namespace {

bool cuda_available() {
    ExecutorRegistry reg = ExecutorRegistry::create_auto();
    for (auto* e : reg.available_executors()) {
        if (e->backend_type() == "cuda") return true;
    }
    return false;
}

// 仅 GPU 的注册表（禁用 CPU executor）
std::shared_ptr<ExecutorRegistry> make_gpu_only_registry() {
    auto regs = std::make_shared<ExecutorRegistry>(ExecutorRegistry::create_auto());
    auto* cpu = regs->find("cpu");
    if (cpu) {
        static_cast<CpuExecutor*>(cpu)->set_available(false);
    }
    return regs;
}

std::shared_ptr<ExecutorRegistry> make_cpu_plus_gpu_registry() {
    return std::make_shared<ExecutorRegistry>(ExecutorRegistry::create_auto());
}

cost::CostEstimate make_estimate(DeviceId dev, std::size_t rec,
                                 std::size_t min_chunk) {
    cost::CostEstimate est;
    cost::DeviceCost dc;
    dc.device_id = dev;
    dc.backend = (dev == kHwCpuDeviceId) ? "cpu" : "cuda:0";
    dc.recommended_chunk = rec;
    dc.min_effective_chunk = min_chunk;
    dc.feasible = true;
    dc.profile_available = true;
    dc.reason = "test";
    est.per_device.push_back(dc);
    est.preferred_device = dev;
    est.profile_available = true;
    return est;
}

TaskDescriptor make_task(std::size_t n) {
    TaskDescriptor task;
    task.range = Range1D{0, n};
    task.item_count = n;
    return task;
}

struct CudaGuard {
    CudaGuard() { classic::register_classic_kernels(); }
};

} // anonymous namespace

// ============================================================================
// 1. GPU executor 注册（无 GPU 时 SKIPPED，不冒充通过）
// ============================================================================
TEST(CudaBridge, GpuExecutorRegisteredWhenAvailable) {
    CudaGuard guard;
    if (!cuda_available()) {
        GTEST_SKIP() << "no CUDA bridge/device available; real GPU tests skipped";
    }
    ExecutorRegistry reg = ExecutorRegistry::create_auto();
    auto* gpu = reg.find("cuda:0");
    ASSERT_NE(gpu, nullptr);
    EXPECT_EQ(gpu->backend_type(), "cuda");
    EXPECT_TRUE(gpu->available());
    EXPECT_TRUE(gpu->supports("kernel.axpy"));
    EXPECT_FALSE(gpu->supports("kernel.not_registered"));
}

// ============================================================================
// 2. GPU-only AXPY（真实 GPU kernel + 完成统计）
// ============================================================================
TEST(CudaBridge, GpuOnlyAxpyMatchesCpu) {
    CudaGuard guard;
    if (!cuda_available()) {
        GTEST_SKIP() << "no CUDA bridge/device available";
    }
    constexpr std::size_t kN = 1 << 17;
    std::vector<float> x(kN, 1.0f);
    std::vector<float> y(kN, 2.0f);

    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
    cfg.executors = make_gpu_only_registry();
    cfg.enable_memory_budget = false;
    d.configure(cfg);

    KernelInvocation inv;
    inv.id = "kernel.axpy";
    inv.domain = WorkDomain{0, kN};
    inv.buffers.add(0, y.data(), kN);
    inv.buffers.add(1, x.data(), kN);
    append_scalar(inv.scalars, 2.0f);
    inv.traits.bytes_read_per_item = sizeof(float);
    inv.traits.bytes_written_per_item = sizeof(float);

    auto est = make_estimate(static_cast<DeviceId>(1), 65536, 256);
    auto r = d.dispatch_invocation(make_task(kN), est, inv);

    ASSERT_TRUE(r.run_result.all_done) << r.run_result.error_message;
    EXPECT_EQ(r.actual_primary_backend, "cuda:0");
    EXPECT_EQ(r.chunks_on_cpu, 0u);
    EXPECT_GT(r.chunks_on_gpu, 0u);
    EXPECT_GT(r.coverage.done, 0u);
    for (std::size_t i = 0; i < kN; ++i) {
        EXPECT_FLOAT_EQ(y[i], 4.0f);
    }
}

// ============================================================================
// 3. 真实 CPU+GPU Mixed：两端均完成非零工作
// ============================================================================
TEST(CudaBridge, RealMixedCpuAndGpu) {
    CudaGuard guard;
    if (!cuda_available()) {
        GTEST_SKIP() << "no CUDA bridge/device available";
    }
    constexpr std::size_t kN = 1 << 18;
    std::vector<float> x(kN, 1.0f);
    std::vector<float> y(kN, 2.0f);

    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
    cfg.executors = make_cpu_plus_gpu_registry();
    cfg.enable_memory_budget = false;
    cfg.invocation_cpu_workers = 2;
    d.configure(cfg);

    KernelInvocation inv;
    inv.id = "kernel.axpy";
    inv.domain = WorkDomain{0, kN};
    inv.buffers.add(0, y.data(), kN);
    inv.buffers.add(1, x.data(), kN);
    append_scalar(inv.scalars, 2.0f);
    inv.traits.bytes_read_per_item = sizeof(float);
    inv.traits.bytes_written_per_item = sizeof(float);

    cost::CostEstimate est;
    cost::DeviceCost cpu_dc;
    cpu_dc.device_id = kHwCpuDeviceId;
    cpu_dc.backend = "cpu";
    cpu_dc.recommended_chunk = 256;
    cpu_dc.min_effective_chunk = 64;
    cpu_dc.feasible = true;
    cpu_dc.profile_available = true;
    cost::DeviceCost gpu_dc;
    gpu_dc.device_id = static_cast<DeviceId>(1);
    gpu_dc.backend = "cuda:0";
    gpu_dc.recommended_chunk = 65536;
    gpu_dc.min_effective_chunk = 256;
    gpu_dc.feasible = true;
    gpu_dc.profile_available = true;
    est.per_device = {cpu_dc, gpu_dc};
    est.preferred_device = static_cast<DeviceId>(1);
    est.profile_available = true;

    auto r = d.dispatch_invocation(make_task(kN), est, inv);

    ASSERT_TRUE(r.run_result.all_done) << r.run_result.error_message;
    EXPECT_GT(r.chunks_on_cpu, 0u);
    EXPECT_GT(r.chunks_on_gpu, 0u);
    EXPECT_EQ(r.coverage.failed, 0u);
    for (std::size_t i = 0; i < kN; ++i) {
        EXPECT_FLOAT_EQ(y[i], 4.0f);
    }
}

// ============================================================================
// 4. GPU-only COPY / REDUCE / CONV3x3 与 CPU 参考一致
// ============================================================================
TEST(CudaBridge, CopyReduceConvMatchCpu) {
    CudaGuard guard;
    if (!cuda_available()) {
        GTEST_SKIP() << "no CUDA bridge/device available";
    }
    constexpr std::size_t kN = 1 << 16;

    // ---- COPY ----
    {
        std::vector<float> x(kN, 3.5f);
        std::vector<float> y(kN, 0.0f);
        Dispatcher d;
        DispatcherConfig cfg;
        cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
        cfg.executors = make_gpu_only_registry();
        cfg.enable_memory_budget = false;
        d.configure(cfg);
        KernelInvocation inv;
        inv.id = "kernel.copy";
        inv.domain = WorkDomain{0, kN};
        inv.buffers.add(0, y.data(), kN);
        inv.buffers.add(1, x.data(), kN);
        auto r = d.dispatch_invocation(
            make_task(kN), make_estimate(static_cast<DeviceId>(1), 65536, 256), inv);
        ASSERT_TRUE(r.run_result.all_done);
        for (std::size_t i = 0; i < kN; ++i) EXPECT_FLOAT_EQ(y[i], 3.5f);
    }

    // ---- REDUCE（分块局部归约 + merge）----
    {
        std::vector<float> x(kN, 1.0f);
        const std::size_t max_chunks = kN / 256 + 8;
        std::vector<double> partials(max_chunks * classic::kReduceBlocks, 0.0);
        Dispatcher d;
        DispatcherConfig cfg;
        cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
        cfg.executors = make_gpu_only_registry();
        cfg.enable_memory_budget = false;
        d.configure(cfg);
        KernelInvocation inv;
        inv.id = "kernel.reduce";
        inv.domain = WorkDomain{0, kN};
        inv.buffers.add(0, x.data(), kN);
        inv.buffers.add(1, partials.data(), partials.size());
        // 注册声明 FP64 accumulator（24 §5.1）
        inv.traits.numeric.accumulator = NumericPolicy::Accumulator::fp64;
        auto r = d.dispatch_invocation(
            make_task(kN), make_estimate(static_cast<DeviceId>(1), 65536, 256), inv);
        ASSERT_TRUE(r.run_result.all_done);
        double total = 0.0;
        for (double v : partials) total += v;
        EXPECT_NEAR(total, static_cast<double>(kN), 1e-2);
    }

    // ---- CONV3x3（32x32 随机图，与 host 参考对比）----
    {
        constexpr std::size_t kW = 32, kH = 32;
        std::vector<float> img(kW * kH);
        std::vector<float> k9 = {1, 0, -1, 2, 0, -2, 1, 0, -1};
        unsigned seed = 12345;
        for (auto& v : img) {
            seed = seed * 1103515245u + 12345u;
            v = static_cast<float>((seed >> 8) % 100) / 100.0f;
        }
        std::vector<float> y(kW * kH, 0.0f);
        Dispatcher d;
        DispatcherConfig cfg;
        cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
        cfg.executors = make_gpu_only_registry();
        cfg.enable_memory_budget = false;
        d.configure(cfg);
        KernelInvocation inv;
        inv.id = "kernel.conv3x3";
        inv.domain = WorkDomain{0, kW * kH};
        inv.buffers.add(0, y.data(), kW * kH);
        inv.buffers.add(1, img.data(), kW * kH);
        append_scalar(inv.scalars, kW);
        append_scalar(inv.scalars, kH);
        for (float kv : k9) append_scalar(inv.scalars, kv);
        auto r = d.dispatch_invocation(
            make_task(kW * kH), make_estimate(static_cast<DeviceId>(1), 65536, 64), inv);
        ASSERT_TRUE(r.run_result.all_done);
        // host 参考
        for (std::size_t p = 0; p < kW * kH; ++p) {
            const int px = static_cast<int>(p % kW);
            const int py = static_cast<int>(p / kW);
            float ref = 0.0f;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const int nx = px + dx;
                    const int ny = py + dy;
                    if (nx < 0 || ny < 0 || nx >= static_cast<int>(kW) ||
                        ny >= static_cast<int>(kH)) continue;
                    ref += img[static_cast<size_t>(ny) * kW + nx] *
                           k9[static_cast<size_t>(dy + 1) * 3 + (dx + 1)];
                }
            }
            EXPECT_NEAR(y[p], ref, 1e-4) << "pixel " << p;
        }
    }
}
