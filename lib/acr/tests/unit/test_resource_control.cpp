// lib/acr/tests/unit/test_resource_control.cpp — 内存预算反压测试
// （26 号计划 §2/§9：CPU/GPU 利用率控制已移除，只测 MemoryBudget）
//
// 验收：
//   - 200ms 内存时间窗采样 + claim 前峰值估算动作实际进入执行链；
//   - gate 关闭只由内存动作触发，恢复只依据内存动作，期间不丢工作；
//   - StopNewSubmit 可恢复；ReleaseCache 调用真实 hook；
//   - Fail 保留准确未完成范围与错误（coverage 不完整 + all_done=false）。
#include <gtest/gtest.h>

#include "dispatcher.hpp"

#include <atomic>
#include <chrono>
#include <string>
#include <vector>

#include "../core/task_descriptor.hpp"
#include "../cost/cost_estimator.hpp"
#include "../utilization/memory_budget.hpp"

using namespace astro::compute;
using namespace astro::compute::scheduler;
using astro::compute::utilization::MemoryBudget;

namespace {

cost::CostEstimate make_cpu_only_estimate(std::size_t recommended_chunk) {
    cost::CostEstimate est;
    cost::DeviceCost dc;
    dc.device_id = kHwCpuDeviceId;
    dc.backend = "cpu";
    dc.recommended_chunk = recommended_chunk;
    dc.min_effective_chunk = 64;
    dc.feasible = true;
    dc.profile_available = true;
    dc.reason = "test";
    est.per_device.push_back(dc);
    est.preferred_device = kHwCpuDeviceId;
    est.profile_available = true;
    return est;
}

MemoryBudget make_memory(std::uint64_t used, std::uint64_t limit,
                         std::uint64_t total) {
    MemoryBudget m;
    m.total_ram = total;
    m.limit_ram = limit;
    m.used_ram = used;
    m.ram_valid = true;
    m.ram_exceeded = used > limit;
    return m;
}

// 运行 dispatch_range_cost_aware（CPU-only 路径），返回结果
CostAwareResult run_dispatch(DispatcherConfig cfg,
                             std::size_t n_items,
                             std::size_t recommended_chunk) {
    runtime_init();
    Dispatcher d;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    cfg.fallback_strategy = FallbackStrategy::ToCpu;
    d.configure(cfg);

    TaskDescriptor task;
    task.range = Range1D{0, n_items};
    auto est = make_cpu_only_estimate(recommended_chunk);
    std::vector<int> data(n_items, 0);
    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        auto* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };
    auto result = d.dispatch_range_cost_aware(task, est, fn, &data);
    runtime_shutdown();
    return result;
}

} // anonymous namespace

// ============================================================================
// 3. MemoryBudget ShrinkBlock 实际改变后续 claim
// ============================================================================
TEST(ResourceControl, MemoryShrinkChangesClaims) {
    DispatcherConfig cfg;
    cfg.memory_sampler_override = [] {
        // used=900, limit=800 → over_ratio=0.125 → ShrinkBlock
        return make_memory(900, 800, 1000);
    };

    auto r = run_dispatch(cfg, 100000, 1024);
    EXPECT_TRUE(r.run_result.all_done);
    EXPECT_GT(r.resource_control.batch_shrink_count, 0u);
    bool has_shrink = false;
    for (const auto& a : r.resource_control.control_actions) {
        if (a == "shrink_block") has_shrink = true;
    }
    EXPECT_TRUE(has_shrink);
    // 采样序列记录了内存动作
    EXPECT_FALSE(r.resource_control.mem_actions.empty());
}

// ============================================================================
// 4. ReleaseCache 调用真实 hook
// ============================================================================
TEST(ResourceControl, ReleaseCacheHookCalled) {
    runtime_init();
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    cfg.fallback_strategy = FallbackStrategy::ToCpu;
    cfg.memory_sampler_override = [] {
        // used=950, limit=800 → over_ratio=0.1875 → ReleaseCache
        return make_memory(950, 800, 1000);
    };
    d.configure(cfg);
    std::atomic<int> release_calls{0};
    d.set_cache_release_hook([&]() -> std::size_t {
        release_calls.fetch_add(1);
        return 1024;  // 释放 1KB（Evidence 记录实际释放字节）
    });

    TaskDescriptor task;
    task.range = Range1D{0, 100000};
    auto est = make_cpu_only_estimate(1024);
    std::vector<int> data(100000, 0);
    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        auto* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };
    auto r = d.dispatch_range_cost_aware(task, est, fn, &data);

    EXPECT_TRUE(r.run_result.all_done);
    EXPECT_GT(release_calls.load(), 0);
    runtime_shutdown();
}

// ============================================================================
// 5. StopNewSubmit 暂停后可恢复，不丢工作
// ============================================================================
TEST(ResourceControl, StopNewSubmitPauseResumeNoLoss) {
    DispatcherConfig cfg;
    auto mem_calls = std::make_shared<std::atomic<int>>(0);
    cfg.memory_sampler_override = [mem_calls] {
        // 前 2 次采样轻微超限（over_ratio<0.05）→ StopNewSubmit；之后恢复正常
        const bool stop =
            mem_calls->fetch_add(1, std::memory_order_relaxed) < 2;
        return make_memory(stop ? 810 : 0, 800, 1000);
    };

    auto r = run_dispatch(cfg, 100000, 1024);
    EXPECT_TRUE(r.run_result.all_done);
    EXPECT_EQ(r.coverage.done, r.coverage.total);
    EXPECT_GT(r.resource_control.gate_close_count, 0u);
    bool has_stop = false;
    for (const auto& a : r.resource_control.control_actions) {
        if (a == "stop_new_submit") has_stop = true;
    }
    EXPECT_TRUE(has_stop);
}

// ============================================================================
// 6. MemoryBudget Fail：保留准确未完成范围与错误
// ============================================================================
TEST(ResourceControl, MemoryFailKeepsAccurateCoverage) {
    DispatcherConfig cfg;
    cfg.memory_sampler_override = [] {
        // over_ratio=(1000-500)/500=1.0 >= 0.30 且 used >= total → Fail
        return make_memory(1000, 500, 1000);
    };

    auto r = run_dispatch(cfg, 100000, 1024);
    EXPECT_FALSE(r.run_result.all_done);
    // 未执行任何块：不得把未完成工作标记为 done（准确 coverage）
    EXPECT_EQ(r.coverage.done, 0u);
    bool has_fail = false;
    for (const auto& a : r.resource_control.control_actions) {
        if (a == "fail") has_fail = true;
    }
    EXPECT_TRUE(has_fail);
}

// ============================================================================
// 7. 25 号计划 §7：claim 前峰值估算（输入/输出/双缓冲/临时区等）触发
//    ShrinkBlock，缩块不丢工作
// ============================================================================
TEST(ResourceControl, PeakEstimateShrinkChangesClaims) {
    DispatcherConfig cfg;
    cfg.memory_sampler_override = [] {
        // used=10.5MB，limit=10MB，peak≈16.7KB → used+peak 轻微超限
        // （over_ratio ∈ [0.05,0.15)）→ ShrinkBlock
        return make_memory(10500000, 10000000, 100000000);
    };

    runtime_init();
    Dispatcher d;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    cfg.fallback_strategy = FallbackStrategy::ToCpu;
    d.configure(cfg);

    TaskDescriptor task;
    task.range = Range1D{0, 100000};
    task.traits.bytes_read_per_item = 4;
    task.traits.bytes_written_per_item = 4;
    auto est = make_cpu_only_estimate(1024);
    std::vector<int> data(100000, 0);
    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        auto* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };
    auto r = d.dispatch_range_cost_aware(task, est, fn, &data);
    runtime_shutdown();

    EXPECT_TRUE(r.run_result.all_done);
    EXPECT_EQ(r.coverage.done, r.coverage.total);
    // 峰值估算与动作均有原始记录
    EXPECT_FALSE(r.resource_control.mem_peak_estimates.empty());
    bool has_peak_shrink = false;
    for (const auto& a : r.resource_control.mem_peak_actions) {
        if (a == "shrink") has_peak_shrink = true;
    }
    EXPECT_TRUE(has_peak_shrink);
    EXPECT_GT(r.resource_control.mem_peak_max, 0u);
}

// ============================================================================
// 8. 25 号计划 §7：claim 前峰值估算超限到 Fail：保留准确未完成范围
// ============================================================================
TEST(ResourceControl, PeakFailKeepsAccurateCoverage) {
    DispatcherConfig cfg;
    cfg.memory_sampler_override = [] {
        // used=19.99MB，limit=10MB，total=20MB；used+peak ≈ 20.006MB ≥ total
        // → Fail（over_ratio ≥ 0.30 且达到总量）
        return make_memory(19990000, 10000000, 20000000);
    };

    runtime_init();
    Dispatcher d;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    cfg.fallback_strategy = FallbackStrategy::ToCpu;
    d.configure(cfg);

    TaskDescriptor task;
    task.range = Range1D{0, 100000};
    task.traits.bytes_read_per_item = 4;
    task.traits.bytes_written_per_item = 4;
    auto est = make_cpu_only_estimate(1024);
    std::vector<int> data(100000, 0);
    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        auto* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };
    auto r = d.dispatch_range_cost_aware(task, est, fn, &data);
    runtime_shutdown();

    EXPECT_FALSE(r.run_result.all_done);
    // 不得把未完成工作标记为 done（准确 coverage）
    EXPECT_EQ(r.coverage.done, 0u);
    bool has_fail = false;
    for (const auto& a : r.resource_control.mem_peak_actions) {
        if (a == "fail") has_fail = true;
    }
    EXPECT_TRUE(has_fail);
}
