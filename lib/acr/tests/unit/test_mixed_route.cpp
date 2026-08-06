// lib/acr/tests/unit/test_mixed_route.cpp — 聚焦 Mixed 路由规划测试
//
// 08 号计划 §5 / 05 号规范：
//   - CPU/GPU 独立块大小
//   - host 与 resident 不同 GPU 最小收益规模
//   - 边际收益门停止慢设备新 claim
//   - 无合格 Profile 时安全回退（不伪造 GPU 路由）
//   - RouteMode CpuOnly/GpuOnly 强制单设备
#include <gtest/gtest.h>

#include "dispatcher.hpp"
#include "mixed_route_planner.hpp"
#include "focused/operation_profile.hpp"

#include <memory>
#include <string>
#include <vector>

#include "astro/compute/kernel_registry.hpp"
#include "../core/task_descriptor.hpp"
#include "../cost/cost_estimator.hpp"

using namespace astro::compute;
using namespace astro::compute::scheduler;
using astro::compute::qualification::focused::OperationProfile;

namespace {

void cpu_axpy_launcher(const KernelInvocation& inv, void*) {
    const BufferBinding* yb = inv.buffers.find(0);
    const BufferBinding* xb = inv.buffers.find(1);
    auto a = read_scalar<float>(inv.scalars, 0);
    if (!yb || !xb || !a) return;
    float* y = static_cast<float*>(yb->data);
    const float* x = static_cast<const float*>(xb->data);
    for (std::size_t i = inv.domain.begin; i < inv.domain.end; ++i) {
        y[i] = *a * x[i] + y[i];
    }
}

void register_axpy() {
    static std::once_flag flag;
    std::call_once(flag, [] {
        KernelRegistration reg;
        reg.id = "kernel.axpy";
        reg.args.buffer_count = 2;
        reg.args.scalar_bytes = sizeof(float);
        reg.cpu = &cpu_axpy_launcher;
        global_kernel_registry().register_kernel(reg);
    });
}

// 构造一个合格 OperationProfile（dense_pixel_accumulate.fp32）
OperationProfile make_qualified_profile() {
    OperationProfile p;
    p.profile_state = "qualified";
    p.fingerprint_cpu = "test-cpu";
    p.fingerprint_compiler = "test";
    p.fingerprint_runtime_kernel_hash = "0123456789abcdef";
    OperationProfile::Operation op;
    op.operation_id = "synthetic.dense_pixel_accumulate.fp32";
    op.precision = "fp32";
    op.accumulator = "fp32";
    op.qualified = true;
    op.sample_range = {1u << 18, 1u << 26, 7};
    op.cpu.ns_per_item = 1.0;
    op.cpu.recommended_chunk_items = 65536;
    op.cpu.minimum_chunk_items = 1024;
    op.gpu.ns_per_item = 0.5;
    op.gpu.fixed_us = 10.0;
    op.gpu.recommended_chunk_items = 1u << 20;
    op.gpu.minimum_chunk_items = 1u << 14;
    op.gpu.host_path_eligible = true;
    op.gpu.resident_path_eligible = true;
    op.gpu.min_profitable_items_host = 100000;
    op.gpu.min_profitable_items_resident = 40000;
    op.transfer.h2d_gbps = 10.0;
    op.transfer.d2h_gbps = 10.0;
    op.memory.host_bytes_per_item = 8.0;
    op.memory.device_bytes_per_item = 8.0;
    p.operations.push_back(op);
    return p;
}

} // anonymous namespace

// ============================================================================
// 1. 独立块大小 + host/resident 阈值
// ============================================================================
TEST(MixedRoute, IndependentChunksAndResidentThresholds) {
    OperationProfile p = make_qualified_profile();
    MixedRoutePlanner planner;
    planner.set_profile(&p);

    auto host_plan = planner.plan(
        "synthetic.dense_pixel_accumulate.fp32", 1u << 26,
        /*data_resident=*/false);
    ASSERT_TRUE(host_plan.profile_available);
    EXPECT_EQ(host_plan.cpu_chunk_items, 65536u);
    EXPECT_EQ(host_plan.gpu_chunk_items, 1u << 20);
    EXPECT_EQ(host_plan.gpu_min_host_items, 100000u);
    EXPECT_EQ(host_plan.gpu_min_resident_items, 40000u);

    auto res_plan = planner.plan(
        "synthetic.dense_pixel_accumulate.fp32", 1u << 26,
        /*data_resident=*/true);
    ASSERT_TRUE(res_plan.profile_available);
    // resident 阈值更低（GPU 在数据已驻留时更容易获得收益）
    EXPECT_LT(res_plan.gpu_min_resident_items,
              host_plan.gpu_min_host_items);
}

// ============================================================================
// 2. 边际收益门：慢设备停止新 claim
// ============================================================================
TEST(MixedRoute, TailGateStopsSlowDevice) {
    OperationProfile p = make_qualified_profile();
    MixedRoutePlanner planner;
    planner.set_profile(&p);
    const auto plan = planner.plan(
        "synthetic.dense_pixel_accumulate.fp32", 1u << 20,
        /*data_resident=*/false);

    // 快 CPU（实测 0.5ns/item）→ 继续 claim
    EXPECT_TRUE(MixedRoutePlanner::should_claim(
        plan, "cpu", 1u << 20, 0, 0.5, 1.5, true));
    // 慢 CPU（实测 10ns/item，chunk=64K：block=655us）vs GPU（0.5，剩余
    // 500us）→ 小块也无法在 GPU 清空前完成 → 停止 claim
    EXPECT_FALSE(MixedRoutePlanner::should_claim(
        plan, "cpu", 1u << 20, 0, 10.0, 0.5, true));
    // 该设备是最快的 → 允许清尾（即使 remaining 大）
    EXPECT_TRUE(MixedRoutePlanner::should_claim(
        plan, "gpu", 1u << 20, 0, 0.2, 10.0, true));
    // Auto 模式（allow_first_block=false）：无实测时用保守 Profile 判断
    EXPECT_TRUE(MixedRoutePlanner::should_claim(
        plan, "gpu", 1u << 20, 0, 0.0, 0.0, false));
    // ForcedMixed（allow_first_block=true）：未执行设备允许首块
    EXPECT_TRUE(MixedRoutePlanner::should_claim(
        plan, "gpu", 1u << 20, 0, 0.0, 0.0, true));
}

// ============================================================================
// 5. makespan 模型：异速 CPU/GPU 仍可 Mixed（V2 审计 §4）
// ============================================================================
TEST(MixedRoute, HeterogeneousSpeedMixedAllowed) {
    OperationProfile p = make_qualified_profile();
    p.operations[0].cpu.ns_per_item = 5.0;    // CPU 慢
    p.operations[0].gpu.ns_per_item = 0.1;    // GPU 快 50 倍
    p.operations[0].gpu.host_path_eligible = true;
    p.operations[0].gpu.min_profitable_items_host = 1;
    MixedRoutePlanner planner;
    planner.set_profile(&p);
    const auto plan = planner.plan(
        "synthetic.dense_pixel_accumulate.fp32", 10u << 20,
        /*data_resident=*/false);
    ASSERT_TRUE(plan.profile_available);
    // CPU 小块（16K × 5ns = 80us）能在 GPU 清空剩余（0.1×~10M = 1ms）前
    // 完成 → 允许 CPU 参与（异速 Mixed 有收益）
    EXPECT_TRUE(MixedRoutePlanner::should_claim(
        plan, "cpu", 10u << 20, 0, 5.0, 0.1, false));
    // 但若 remaining 很小（GPU 即将清空），CPU 小块无法提前完成 → 停止
    EXPECT_FALSE(MixedRoutePlanner::should_claim(
        plan, "cpu", 1u << 12, 0, 5.0, 0.1, false));
}

// ============================================================================
// 6. 收益阈值不覆盖推荐块（V2 审计 §3）
// ============================================================================
TEST(MixedRoute, ThresholdDoesNotOverrideRecommendedChunk) {
    OperationProfile p = make_qualified_profile();
    // 推荐 GPU 块 1M；收益阈值很小（resident）
    p.operations[0].gpu.recommended_chunk_items = 1u << 20;
    p.operations[0].gpu.resident_path_eligible = true;
    p.operations[0].gpu.min_profitable_items_resident = 1000;
    MixedRoutePlanner planner;
    planner.set_profile(&p);
    const auto res_plan = planner.plan(
        "synthetic.dense_pixel_accumulate.fp32", 1u << 26,
        /*data_resident=*/true);
    ASSERT_TRUE(res_plan.profile_available);
    EXPECT_EQ(res_plan.gpu_chunk_items, 1u << 20);  // 不被 1000 覆盖
}

// ============================================================================
// 3. 无合格 Profile → 保守回退（不伪造 GPU 路由）
// ============================================================================
TEST(MixedRoute, NoProfileFallsBack) {
    MixedRoutePlanner planner;
    planner.set_profile(nullptr);
    auto plan = planner.plan("synthetic.dense_pixel_accumulate.fp32",
                             1u << 20, false);
    EXPECT_FALSE(plan.profile_available);

    OperationProfile p = make_qualified_profile();
    p.operations[0].qualified = false;  // 未合格
    planner.set_profile(&p);
    plan = planner.plan("synthetic.dense_pixel_accumulate.fp32",
                        1u << 20, false);
    EXPECT_FALSE(plan.profile_available);  // 未合格曲线不得进入路由
}

// ============================================================================
// 4. RouteMode 强制单设备（CpuOnly / GpuOnly）
// ============================================================================
TEST(MixedRoute, RouteModeForcesSingleDevice) {
    astro::compute::runtime_init();
    register_axpy();
    // 注册 CPU + mock GPU executor
    auto regs = std::make_shared<ExecutorRegistry>();
    regs->register_executor(std::make_unique<CpuExecutor>("cpu", 1024, 64));
    class MockGpu : public DeviceExecutor {
    public:
        DeviceId id() const override { return 1; }
        std::string device_id() const override { return "cuda:0"; }
        std::string backend_type() const override { return "cuda"; }
        bool available() const override { return true; }
        bool supports(OperationId) const override { return true; }
        QueueState queue_state() const override { return QueueState{}; }
        std::size_t recommended_chunk() const override { return 4096; }
        std::size_t min_effective_chunk() const override { return 64; }
        SubmitHandle submit(const WorkToken& t,
                            const KernelInvocation&) override {
            SubmitHandle h;
            h.status = SubmitStatus::Ok;
            h.items_done = t.size();
            h.elapsed_ns = 1000;
            return h;
        }
    };
    regs->register_executor(std::make_unique<MockGpu>());

    auto est = [] {
        cost::CostEstimate e;
        cost::DeviceCost dc;
        dc.device_id = kHwCpuDeviceId;
        dc.backend = "cpu";
        dc.recommended_chunk = 1024;
        dc.min_effective_chunk = 64;
        dc.feasible = true;
        dc.profile_available = true;
        e.per_device.push_back(dc);
        cost::DeviceCost gpu_dc;
        gpu_dc.device_id = static_cast<DeviceId>(1);
        gpu_dc.backend = "cuda:0";
        gpu_dc.recommended_chunk = 4096;
        gpu_dc.min_effective_chunk = 64;
        gpu_dc.feasible = true;
        gpu_dc.profile_available = true;
        e.per_device.push_back(gpu_dc);
        e.preferred_device = kHwCpuDeviceId;
        e.profile_available = true;
        return e;
    }();

    std::vector<float> x(8192, 1.0f), y(8192, 2.0f);
    KernelInvocation inv;
    inv.id = "kernel.axpy";
    inv.domain = WorkDomain{0, 8192};
    inv.buffers.add(0, y.data(), y.size());
    inv.buffers.add(1, x.data(), x.size());
    append_scalar(inv.scalars, 2.0f);
    inv.traits.bytes_read_per_item = 4;
    inv.traits.bytes_written_per_item = 4;

    TaskDescriptor task;
    task.range = Range1D{0, 8192};
    task.item_count = 8192;

    // CpuOnly：只有 CPU 执行
    {
        Dispatcher d;
        DispatcherConfig cfg;
        cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
        cfg.executors = regs;
        cfg.route_mode = RouteMode::CpuOnly;
        d.configure(cfg);
        auto r = d.dispatch_invocation(task, est, inv);
        std::printf("[MixedRoute.CpuOnly] error=%s\n",
                    r.run_result.error_message.c_str());
        EXPECT_TRUE(r.run_result.all_done);
        EXPECT_GT(r.chunks_on_cpu, 0u);
        EXPECT_EQ(r.chunks_on_gpu, 0u);
    }
    // GpuOnly：只有 GPU 执行
    {
        Dispatcher d;
        DispatcherConfig cfg;
        cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
        cfg.executors = regs;
        cfg.route_mode = RouteMode::GpuOnly;
        d.configure(cfg);
        auto r = d.dispatch_invocation(task, est, inv);
        EXPECT_TRUE(r.run_result.all_done);
        EXPECT_EQ(r.chunks_on_cpu, 0u);
        EXPECT_GT(r.chunks_on_gpu, 0u);
    }
    astro::compute::runtime_shutdown();
}
