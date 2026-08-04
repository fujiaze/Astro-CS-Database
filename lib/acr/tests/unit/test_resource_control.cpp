// lib/acr/tests/unit/test_resource_control.cpp — 可恢复资源闭环测试（23 号计划 §5）
//
// 验收：
//   - 100-500ms 时间窗采样（不按 task 计数），所有 CPU worker 注册参与；
//   - gate 迟滞：close → wait → re-sample → reopen，期间不丢工作；
//   - batch size / claim size / MemoryBudget 动作实际进入执行链；
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
#include "../utilization/cpu_controller.hpp"
#include "../utilization/memory_budget.hpp"

using namespace astro::compute;
using namespace astro::compute::scheduler;
using astro::compute::utilization::CpuControlDecision;
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

CpuControlDecision make_cpu_decision(double actual, double target,
                                     std::uint32_t batch = 8) {
    CpuControlDecision d;
    d.batch_size = batch;
    d.queue_depth = 1;
    d.should_yield = false;
    d.yield_stride = 1;
    d.target_ratio = target;
    d.actual_ratio = actual;
    d.error_ratio = actual - target;
    d.valid = true;
    return d;
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
// 1. 时间窗采样 + worker 注册
// ============================================================================
TEST(ResourceControl, TimeWindowSamplesAndWorkersRegistered) {
    DispatcherConfig cfg;
    cfg.enable_utilization = true;
    cfg.control_window_ms = 100;
    cfg.cpu_sampler_override = [] {
        return make_cpu_decision(0.20, 0.95);
    };
    cfg.memory_sampler_override = [] {
        return make_memory(0, 1u << 30, 1u << 30);  // None
    };

    auto r = run_dispatch(cfg, 200000, 1024);
    EXPECT_TRUE(r.run_result.all_done);
    EXPECT_GT(r.resource_control.cpu_actual_samples.size(), 0u);
    EXPECT_GT(r.resource_control.workers_registered, 0u);
}

// ============================================================================
// 2. gate 迟滞：close → wait → re-sample → reopen，不丢工作
// ============================================================================
TEST(ResourceControl, GateCloseWaitRecoverNoWorkLoss) {
    DispatcherConfig cfg;
    cfg.enable_utilization = true;
    cfg.control_window_ms = 100;
    auto cpu_calls = std::make_shared<std::atomic<int>>(0);
    cfg.cpu_sampler_override = [cpu_calls] {
        // 前 2 次采样高负载（> target+0.10=0.60）→ 关闭 gate；
        // 之后低负载（< target-0.05=0.45）→ 迟滞恢复
        const bool high =
            cpu_calls->fetch_add(1, std::memory_order_relaxed) < 2;
        return make_cpu_decision(high ? 0.95 : 0.20, 0.50);
    };
    cfg.memory_sampler_override = [] {
        return make_memory(0, 1u << 30, 1u << 30);  // None
    };

    auto r = run_dispatch(cfg, 100000, 1024);
    EXPECT_TRUE(r.run_result.all_done);          // 不漏项
    EXPECT_EQ(r.coverage.done, r.coverage.total);
    EXPECT_GT(r.resource_control.gate_close_count, 0u);
    EXPECT_GT(r.resource_control.gate_recover_count, 0u);
    EXPECT_FALSE(r.resource_control.gate_aborted);
    bool has_close = false, has_recover = false;
    for (const auto& a : r.resource_control.control_actions) {
        if (a == "gate_close") has_close = true;
        if (a == "gate_recover") has_recover = true;
    }
    EXPECT_TRUE(has_close);
    EXPECT_TRUE(has_recover);
}

// ============================================================================
// 3. MemoryBudget ShrinkBlock 实际改变后续 claim
// ============================================================================
TEST(ResourceControl, MemoryShrinkChangesClaims) {
    DispatcherConfig cfg;
    cfg.enable_utilization = true;
    cfg.control_window_ms = 100;
    cfg.cpu_sampler_override = [] {
        return make_cpu_decision(0.20, 0.95);
    };
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
    cfg.enable_utilization = true;
    cfg.control_window_ms = 100;
    cfg.cpu_sampler_override = [] {
        return make_cpu_decision(0.20, 0.95);
    };
    cfg.memory_sampler_override = [] {
        // used=950, limit=800 → over_ratio=0.1875 → ReleaseCache
        return make_memory(950, 800, 1000);
    };
    d.configure(cfg);
    std::atomic<int> release_calls{0};
    d.set_cache_release_hook([&] { release_calls.fetch_add(1); });

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
    cfg.enable_utilization = true;
    cfg.control_window_ms = 100;
    cfg.cpu_sampler_override = [] {
        return make_cpu_decision(0.20, 0.95);
    };
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
    cfg.enable_utilization = true;
    cfg.control_window_ms = 100;
    cfg.cpu_sampler_override = [] {
        return make_cpu_decision(0.20, 0.95);
    };
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
