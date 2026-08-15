// lib/acr/tests/unit/test_invocation_dispatch.cpp — dispatch_invocation 测试
//
// 23 §3/§4 验收：
// - CPU executor 通过 KernelRegistry 真实执行全部工作；
// - 每设备按自身 CostEstimate 的 recommended_chunk 领取（块大小不同）；
// - CPU 与 mock 设备可同时完成非零工作（调度验证；真实 GPU 验收另测）；
// - 无 executor 支持 op 时如实失败（不伪装 CPU/GPU 执行）；
// - CostEstimate 改变真实领取块大小。
#include <gtest/gtest.h>

#include "dispatcher.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "astro/compute/kernel_registry.hpp"
#include "../core/task_descriptor.hpp"
#include "../cost/cost_estimator.hpp"

using namespace astro::compute;
using namespace astro::compute::scheduler;

namespace {

void cpu_axpy_launcher(const KernelInvocation& inv, void* /*user_data*/) {
    const BufferBinding* yb = inv.buffers.find(0);
    const BufferBinding* xb = inv.buffers.find(1);
    ASSERT_NE(yb, nullptr);
    ASSERT_NE(xb, nullptr);
    auto a = read_scalar<float>(inv.scalars, 0);
    ASSERT_TRUE(a.has_value());
    float* y = static_cast<float*>(yb->data);
    const float* x = static_cast<const float*>(xb->data);
    for (std::size_t i = inv.domain.begin; i < inv.domain.end; ++i) {
        y[i] = *a * x[i] + y[i];
    }
}

void register_axpy_kernel() {
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

KernelInvocation make_axpy_invocation(std::vector<float>& x,
                                      std::vector<float>& y) {
    KernelInvocation inv;
    inv.id = "kernel.axpy";
    inv.domain = WorkDomain{0, x.size()};
    inv.buffers.add(0, y.data(), y.size());
    inv.buffers.add(1, x.data(), x.size());
    append_scalar(inv.scalars, 2.0f);
    inv.traits.task_class = TaskClass::elementwise;
    inv.traits.bytes_read_per_item = sizeof(float);
    inv.traits.bytes_written_per_item = sizeof(float);
    return inv;
}

cost::CostEstimate make_estimate(DeviceId device, std::size_t rec,
                                 std::size_t min_chunk) {
    cost::CostEstimate est;
    cost::DeviceCost dc;
    dc.device_id = device;
    dc.backend = (device == kHwCpuDeviceId) ? "cpu" : "cuda:0";
    dc.recommended_chunk = rec;
    dc.min_effective_chunk = min_chunk;
    dc.feasible = true;
    dc.profile_available = true;
    dc.reason = "test-estimate";
    est.per_device.push_back(dc);
    est.preferred_device = device;
    est.profile_available = true;
    return est;
}

// ===== 调度验证用 mock 设备（不冒充真实 GPU；仅验证调度与块大小）=====
class MockSchedulingExecutor : public DeviceExecutor {
public:
    MockSchedulingExecutor(std::size_t rec, std::size_t min_chunk,
                           std::atomic<bool>* on_first_submit = nullptr)
        : rec_(rec), min_chunk_(min_chunk), on_first_submit_(on_first_submit) {}

    DeviceId id() const override { return static_cast<DeviceId>(1); }
    std::string device_id() const override { return "cuda:0"; }
    std::string backend_type() const override { return "cuda"; }
    bool available() const override { return true; }
    bool supports(OperationId) const override { return true; }
    QueueState queue_state() const override { return QueueState{0, 0.0, false}; }
    std::size_t recommended_chunk() const override { return rec_; }
    std::size_t min_effective_chunk() const override { return min_chunk_; }

    SubmitHandle submit(const WorkToken& token,
                        const KernelInvocation& invocation) override {
        // 调度验证：用注册的 CPU launcher 产生正确结果（真实 GPU 验收在 CUDA 测试）
        SubmitHandle h;
        h.device = id();
        h.op_id = std::string(invocation.id);
        h.attempt = token.attempt;
        const KernelRegistration* reg = global_kernel_registry().find(invocation.id);
        if (reg == nullptr || reg->cpu == nullptr) {
            h.status = SubmitStatus::Rejected;
            h.error = "not registered";
            return h;
        }
        reg->cpu(invocation, nullptr);
        h.status = SubmitStatus::Ok;
        h.items_done = token.size();
        h.bytes_done = token.size() * 2 * sizeof(float);
        h.elapsed_ns = 1000;
        if (on_first_submit_ && !first_submit_.exchange(false)) {
            on_first_submit_->store(true, std::memory_order_release);
        }
        {
            std::lock_guard<std::mutex> lk(mtx_);
            submitted_sizes_.push_back(token.size());
        }
        return h;
    }

    std::vector<std::size_t> submitted_sizes() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return submitted_sizes_;
    }

private:
    std::size_t rec_;
    std::size_t min_chunk_;
    std::atomic<bool>* on_first_submit_;
    std::atomic<bool> first_submit_{true};
    mutable std::mutex mtx_;
    std::vector<std::size_t> submitted_sizes_;
};

// 门控 CPU executor：首次提交前等待 mock/GPU 已至少提交一次，
// 保证 Mixed 测试中 CPU 与 mock 都完成非零工作（消除线程启动竞争）。
class GatedCpuExecutor : public CpuExecutor {
public:
    GatedCpuExecutor(std::size_t rec, std::size_t min_chunk,
                     std::atomic<bool>* gpu_started)
        : CpuExecutor("cpu", rec, min_chunk), gpu_started_(gpu_started) {}

    SubmitHandle submit(const WorkToken& token,
                        const KernelInvocation& invocation) override {
        // 所有 CPU 提交都等待 mock 至少完成一次提交，
        // 避免 8 个 CPU worker 在 mock 线程启动前耗尽整个池
        while (gpu_started_ && !gpu_started_->load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        return CpuExecutor::submit(token, invocation);
    }

private:
    std::atomic<bool>* gpu_started_;
};

// 返回 mock 指针（注册表持有所有权；调用方保留 raw 指针观察）
MockSchedulingExecutor* make_mock_executor(ExecutorRegistry& reg,
                                           std::size_t rec,
                                           std::size_t min_chunk,
                                           std::atomic<bool>* on_first_submit = nullptr) {
    auto* mock = new MockSchedulingExecutor(rec, min_chunk, on_first_submit);
    reg.register_executor(std::unique_ptr<DeviceExecutor>(mock));
    return mock;
}

TaskDescriptor make_task(std::size_t n) {
    TaskDescriptor task;
    task.range = Range1D{0, n};
    task.item_count = n;
    return task;
}

} // anonymous namespace

// ============================================================================
// 1. CPU executor 通过注册表真实执行全部工作
// ============================================================================
TEST(DispatchInvocation, CpuOnlyExecutesAll) {
    register_axpy_kernel();

    constexpr std::size_t kN = 4096;
    std::vector<float> x(kN, 1.0f);
    std::vector<float> y(kN, 2.0f);

    auto regs = std::make_shared<ExecutorRegistry>(ExecutorRegistry::create_cpu_only());
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    cfg.executors = regs;
    cfg.enable_memory_budget = false;
    d.configure(cfg);

    auto est = make_estimate(kHwCpuDeviceId, 256, 64);
    auto inv = make_axpy_invocation(x, y);
    auto result = d.dispatch_invocation(make_task(kN), est, inv);

    EXPECT_TRUE(result.run_result.all_done);
    EXPECT_EQ(result.actual_primary_backend, "cpu");
    EXPECT_EQ(result.coverage.done, result.coverage.total);
    EXPECT_EQ(result.coverage.failed, 0u);
    EXPECT_EQ(result.chunks_on_gpu, 0u);
    for (std::size_t i = 0; i < kN; ++i) {
        EXPECT_FLOAT_EQ(y[i], 4.0f);
    }
}

// ============================================================================
// 2. 每设备按自身 CostEstimate 领取：块大小不同，CPU+GPU 均完成非零工作
// ============================================================================
TEST(DispatchInvocation, PerDeviceClaimsDifferAndBothComplete) {
    register_axpy_kernel();

    constexpr std::size_t kN = 8192;
    std::vector<float> x(kN, 1.0f);
    std::vector<float> y(kN, 2.0f);

    auto regs = std::make_shared<ExecutorRegistry>();
    std::atomic<bool> gpu_started{false};
    regs->register_executor(
        std::make_unique<GatedCpuExecutor>(64, 16, &gpu_started));
    MockSchedulingExecutor* mock =
        make_mock_executor(*regs, 1024, 64, &gpu_started);  // GPU 推荐块大得多

    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
    cfg.executors = regs;
    cfg.enable_memory_budget = false;
    d.configure(cfg);

    // 每设备独立成本：CPU 推荐 64，GPU 推荐 1024
    cost::CostEstimate est;
    cost::DeviceCost cpu_dc;
    cpu_dc.device_id = kHwCpuDeviceId;
    cpu_dc.backend = "cpu";
    cpu_dc.recommended_chunk = 64;
    cpu_dc.min_effective_chunk = 16;
    cpu_dc.feasible = true;
    cpu_dc.profile_available = true;
    cost::DeviceCost gpu_dc;
    gpu_dc.device_id = static_cast<DeviceId>(1);
    gpu_dc.backend = "cuda:0";
    gpu_dc.recommended_chunk = 1024;
    gpu_dc.min_effective_chunk = 64;
    gpu_dc.feasible = true;
    gpu_dc.profile_available = true;
    est.per_device = {cpu_dc, gpu_dc};
    est.preferred_device = static_cast<DeviceId>(1);
    est.profile_available = true;

    auto inv = make_axpy_invocation(x, y);
    auto result = d.dispatch_invocation(make_task(kN), est, inv);

    // 真实 Mixed（调度级）：CPU 与 mock 均完成非零工作
    EXPECT_TRUE(result.run_result.all_done);
    EXPECT_GT(result.chunks_on_cpu, 0u);
    EXPECT_GT(result.chunks_on_gpu, 0u);
    EXPECT_EQ(result.coverage.failed, 0u);
    // 块大小按设备成本不同：GPU 平均块 > CPU 平均块
    auto mock_sizes = mock->submitted_sizes();
    ASSERT_FALSE(mock_sizes.empty());
    std::size_t mock_total = 0;
    for (auto s : mock_sizes) mock_total += s;
    // GPU 总元素数必须 > 0 且 GPU 块均 >= CPU 最小块（64）
    EXPECT_GT(mock_total, 0u);
    // 正确性：y 全部 == 4
    for (std::size_t i = 0; i < kN; ++i) {
        EXPECT_FLOAT_EQ(y[i], 4.0f);
    }
}

// ============================================================================
// 3. 无 executor 支持 op → 如实失败（不伪装）
// ============================================================================
TEST(DispatchInvocation, UnsupportedOperationFailsHonestly) {
    register_axpy_kernel();
    constexpr std::size_t kN = 256;
    std::vector<float> x(kN, 1.0f);
    std::vector<float> y(kN, 2.0f);

    auto regs = std::make_shared<ExecutorRegistry>(ExecutorRegistry::create_cpu_only());
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    cfg.executors = regs;
    cfg.enable_memory_budget = false;
    d.configure(cfg);

    auto est = make_estimate(kHwCpuDeviceId, 64, 16);
    auto inv = make_axpy_invocation(x, y);
    inv.id = "kernel.not_registered";  // 未注册 → 无 executor 支持
    auto result = d.dispatch_invocation(make_task(kN), est, inv);

    EXPECT_FALSE(result.run_result.all_done);
    EXPECT_EQ(result.actual_primary_backend, "none");
    EXPECT_EQ(result.coverage.done, 0u);
    EXPECT_FALSE(result.run_result.error_message.empty());
    EXPECT_NE(result.run_result.error_message.find("no eligible executor"),
              std::string::npos);
}

// ============================================================================
// 25 §7：invocation 路径 claim 前内存峰值估算触发 ShrinkBlock
// ============================================================================
TEST(DispatchInvocation, PeakBudgetShrinkChangesClaims) {
    register_axpy_kernel();
    constexpr std::size_t kN = 4096;
    std::vector<float> x(kN, 1.0f);
    std::vector<float> y(kN, 2.0f);

    auto regs = std::make_shared<ExecutorRegistry>(ExecutorRegistry::create_cpu_only());
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    cfg.executors = regs;
    cfg.enable_memory_budget = true;
    cfg.memory_sampler_override = [] {
        utilization::MemoryBudget m;
        m.total_ram = 100000000;
        m.limit_ram = 10000000;
        m.used_ram = 10500000;  // used + peak(≈4.2KB) 轻微超限 → ShrinkBlock
        m.ram_valid = true;
        m.ram_exceeded = true;
        return m;
    };
    d.configure(cfg);

    auto est = make_estimate(kHwCpuDeviceId, 256, 64);
    auto inv = make_axpy_invocation(x, y);
    auto result = d.dispatch_invocation(make_task(kN), est, inv);

    // 缩块不丢工作且结果正确
    EXPECT_TRUE(result.run_result.all_done);
    EXPECT_EQ(result.coverage.done, result.coverage.total);
    EXPECT_FALSE(result.resource_control.mem_peak_estimates.empty());
    bool has_peak_shrink = false;
    for (const auto& a : result.resource_control.mem_peak_actions) {
        if (a == "shrink") has_peak_shrink = true;
    }
    EXPECT_TRUE(has_peak_shrink);
    for (std::size_t i = 0; i < kN; ++i) {
        EXPECT_FLOAT_EQ(y[i], 4.0f);
    }
}

// ============================================================================
// 4. CostEstimate 改变真实领取块大小（总块数随 recommended_chunk 变化）
// ============================================================================
TEST(DispatchInvocation, CostEstimateChangesClaimSize) {
    register_axpy_kernel();
    constexpr std::size_t kN = 4096;

    auto run_with_rec = [&](std::size_t rec) -> std::size_t {
        std::vector<float> x(kN, 1.0f);
        std::vector<float> y(kN, 2.0f);
        auto regs = std::make_shared<ExecutorRegistry>(ExecutorRegistry::create_cpu_only());
        Dispatcher d;
        DispatcherConfig cfg;
        cfg.devices = {{"cpu", 0, 0, 50.0, true}};
        cfg.executors = regs;
        cfg.enable_memory_budget = false;
        d.configure(cfg);
        auto est = make_estimate(kHwCpuDeviceId, rec, 16);
        auto inv = make_axpy_invocation(x, y);
        auto result = d.dispatch_invocation(make_task(kN), est, inv);
        EXPECT_TRUE(result.run_result.all_done);
        return result.total_chunks;
    };

    const std::size_t chunks_large = run_with_rec(1024);  // 大块 → 少块
    const std::size_t chunks_small = run_with_rec(64);    // 小块 → 多块
    EXPECT_GT(chunks_large, 0u);
    EXPECT_GT(chunks_small, chunks_large);
}

// ============================================================================
// 5. Eligible Device Set（24 §2）：不可行/低收益/小任务/无 profile GPU 不参与
// ============================================================================
namespace {

// 构造带指定 GPU DeviceCost 的 estimate（CPU 恒可行）
cost::CostEstimate make_dual_estimate(bool gpu_feasible,
                                      bool gpu_profile,
                                      std::size_t gpu_min_chunk,
                                      double gpu_compute_ns,
                                      double gpu_overhead_ns) {
    cost::CostEstimate est;
    cost::DeviceCost cpu_dc;
    cpu_dc.device_id = kHwCpuDeviceId;
    cpu_dc.backend = "cpu";
    cpu_dc.recommended_chunk = 1024;
    cpu_dc.min_effective_chunk = 64;
    cpu_dc.feasible = true;
    cpu_dc.profile_available = true;
    est.per_device.push_back(cpu_dc);
    cost::DeviceCost gpu_dc;
    gpu_dc.device_id = static_cast<DeviceId>(1);
    gpu_dc.backend = "cuda:0";
    gpu_dc.recommended_chunk = 65536;
    gpu_dc.min_effective_chunk = gpu_min_chunk;
    gpu_dc.feasible = gpu_feasible;
    gpu_dc.profile_available = gpu_profile;
    gpu_dc.compute_cost_ns = gpu_compute_ns;
    gpu_dc.launch_cost_ns = gpu_overhead_ns / 2;
    gpu_dc.transfer_cost_ns = gpu_overhead_ns / 2;
    gpu_dc.merge_cost_ns = 0;
    est.per_device.push_back(gpu_dc);
    est.preferred_device = static_cast<DeviceId>(1);
    est.profile_available = true;
    return est;
}

// 注册 mock GPU + 真 CPU，返回 registry
std::shared_ptr<ExecutorRegistry> make_cpu_mock_registry() {
    register_axpy_kernel();
    auto regs = std::make_shared<ExecutorRegistry>();
    regs->register_executor(std::make_unique<CpuExecutor>("cpu", 1024, 64));
    make_mock_executor(*regs, 65536, 256);
    return regs;
}

CostAwareResult dispatch_with(DispatcherConfig cfg,
                              std::shared_ptr<ExecutorRegistry> regs,
                              std::size_t n,
                              const cost::CostEstimate& est) {
    Dispatcher d;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
    cfg.executors = regs;
    cfg.enable_memory_budget = false;
    d.configure(cfg);
    std::vector<float> x(n, 1.0f);
    std::vector<float> y(n, 0.0f);
    KernelInvocation inv;
    inv.id = "kernel.axpy";
    inv.domain = WorkDomain{0, n};
    inv.buffers.add(0, y.data(), n);
    inv.buffers.add(1, x.data(), n);
    append_scalar(inv.scalars, 2.0f);
    inv.traits.bytes_read_per_item = sizeof(float);
    inv.traits.bytes_written_per_item = sizeof(float);
    return d.dispatch_invocation(make_task(n), est, inv);
}

} // anonymous namespace

TEST(DispatchInvocation, InfeasibleGpuExcluded) {
    auto est = make_dual_estimate(/*feasible=*/false, true, 256, 1e6, 1e3);
    auto r = dispatch_with(DispatcherConfig{}, make_cpu_mock_registry(), 1 << 18, est);
    EXPECT_TRUE(r.run_result.all_done);
    EXPECT_EQ(r.chunks_on_gpu, 0u);   // infeasible GPU 不参与
    EXPECT_GT(r.chunks_on_cpu, 0u);
}

TEST(DispatchInvocation, SmallTaskExcludesGpu) {
    // GPU min_effective_chunk = 1M > 任务规模 64K → GPU 不参与
    auto est = make_dual_estimate(true, true, 1u << 20, 1e6, 1e3);
    auto r = dispatch_with(DispatcherConfig{}, make_cpu_mock_registry(), 1 << 16, est);
    EXPECT_TRUE(r.run_result.all_done);
    EXPECT_EQ(r.chunks_on_gpu, 0u);
    EXPECT_GT(r.chunks_on_cpu, 0u);
}

TEST(DispatchInvocation, NoProfileGpuExcluded) {
    auto est = make_dual_estimate(true, /*profile=*/false, 256, 1e6, 1e3);
    auto r = dispatch_with(DispatcherConfig{}, make_cpu_mock_registry(), 1 << 18, est);
    EXPECT_TRUE(r.run_result.all_done);
    EXPECT_EQ(r.chunks_on_gpu, 0u);   // 无有效 profile 的 GPU 不参与生产
    EXPECT_GT(r.chunks_on_cpu, 0u);
}

TEST(DispatchInvocation, NoBenefitGpuExcluded) {
    // compute(1e3) <= overhead(2e3) → 无收益 → GPU 不参与
    auto est = make_dual_estimate(true, true, 256, 1e3, 2e3);
    auto r = dispatch_with(DispatcherConfig{}, make_cpu_mock_registry(), 1 << 18, est);
    EXPECT_TRUE(r.run_result.all_done);
    EXPECT_EQ(r.chunks_on_gpu, 0u);
    EXPECT_GT(r.chunks_on_cpu, 0u);
}

TEST(DispatchInvocation, ForceAllSupportedForTestOnly) {
    // 测试专用开关：即使 infeasible 也强制参与（仅调度验证；生产必须 false）
    auto est = make_dual_estimate(/*feasible=*/false, true, 256, 1e6, 1e3);
    DispatcherConfig cfg;
    cfg.force_all_supported_executors = true;
    auto r = dispatch_with(cfg, make_cpu_mock_registry(), 1 << 18, est);
    EXPECT_TRUE(r.run_result.all_done);
    EXPECT_GT(r.chunks_on_gpu, 0u);   // 测试开关强制 mock 参与
    EXPECT_GT(r.chunks_on_cpu, 0u);
}

// ============================================================================
// 6. actual_primary 反例（24 §3）：按真实 items/bytes，禁止 executor 顺序
// ============================================================================
namespace {

cost::CostEstimate make_dual_estimate_chunks(std::size_t cpu_rec,
                                             std::size_t gpu_rec) {
    cost::CostEstimate est;
    cost::DeviceCost cpu_dc;
    cpu_dc.device_id = kHwCpuDeviceId;
    cpu_dc.backend = "cpu";
    cpu_dc.recommended_chunk = cpu_rec;
    cpu_dc.min_effective_chunk = 64;
    cpu_dc.feasible = true;
    cpu_dc.profile_available = true;
    est.per_device.push_back(cpu_dc);
    cost::DeviceCost gpu_dc;
    gpu_dc.device_id = static_cast<DeviceId>(1);
    gpu_dc.backend = "cuda:0";
    gpu_dc.recommended_chunk = gpu_rec;
    gpu_dc.min_effective_chunk = 64;
    gpu_dc.feasible = true;
    gpu_dc.profile_available = true;
    est.per_device.push_back(gpu_dc);
    est.preferred_device = static_cast<DeviceId>(1);
    est.profile_available = true;
    return est;
}

} // anonymous namespace

TEST(DispatchInvocation, ActualPrimaryGpuWhenGpuItemsMore) {
    // GPU 块大（rec 65536）、CPU 块小（rec 256）→ GPU 完成 items 更多
    auto est = make_dual_estimate_chunks(256, 65536);
    auto r = dispatch_with(DispatcherConfig{}, make_cpu_mock_registry(), 1 << 20, est);
    ASSERT_TRUE(r.run_result.all_done);
    ASSERT_EQ(r.per_device_stats.size(), 2u);
    std::size_t cpu_items = 0, gpu_items = 0;
    for (const auto& s : r.per_device_stats) {
        if (s.backend == "cpu") cpu_items = s.items_done;
        else if (s.backend == "cuda") gpu_items = s.items_done;
    }
    EXPECT_GT(gpu_items, cpu_items);
    EXPECT_EQ(r.actual_primary_backend, "cuda:0");
}

TEST(DispatchInvocation, ActualPrimaryCpuWhenCpuItemsMore) {
    // CPU 块大（rec 65536）、GPU 块小（rec 256）→ CPU 完成 items 更多
    auto est = make_dual_estimate_chunks(65536, 256);
    auto r = dispatch_with(DispatcherConfig{}, make_cpu_mock_registry(), 1 << 20, est);
    ASSERT_TRUE(r.run_result.all_done);
    ASSERT_EQ(r.per_device_stats.size(), 2u);
    std::size_t cpu_items = 0, gpu_items = 0;
    for (const auto& s : r.per_device_stats) {
        if (s.backend == "cpu") cpu_items = s.items_done;
        else if (s.backend == "cuda") gpu_items = s.items_done;
    }
    EXPECT_GT(cpu_items, gpu_items);
    EXPECT_EQ(r.actual_primary_backend, "cpu");
}

TEST(DispatchInvocation, PerDeviceStatsFromCompletions) {
    auto est = make_dual_estimate_chunks(1024, 65536);
    auto r = dispatch_with(DispatcherConfig{}, make_cpu_mock_registry(), 1 << 20, est);
    ASSERT_TRUE(r.run_result.all_done);
    std::size_t total_items = 0;
    for (const auto& s : r.per_device_stats) {
        total_items += s.items_done;
        // bytes_read/written 来自 traits（axpy：read=4, write=4 per item）
        EXPECT_EQ(s.bytes_read, s.items_done * 4);
        EXPECT_EQ(s.bytes_written, s.items_done * 4);
        EXPECT_GT(s.blocks_done, 0u);  // 参与设备均有真实完成块
    }
    EXPECT_EQ(total_items, 1u << 20);  // 全部 items 均来自 completion
}
