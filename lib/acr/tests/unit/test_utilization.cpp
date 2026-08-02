// lib/acr/tests/unit/test_utilization.cpp — Phase G utilization 单元测试
// 覆盖：
//   - CpuController：set_target / decide / 策略切换
//   - GpuController：register_backend / decide / 多 backend
//   - MemoryBudgetController：limit = min(total*ratio, total-fixed_reserve)
//   - IoBudgetController：budget=0 不限速 / budget>0 超限检测
//   - ConfigHotReader：init / update_hot / 单项 set / ColdStatic 冻结
//   - ActualTracker：record / recent / statistics
#include <gtest/gtest.h>

#include "actual_tracker.hpp"
#include "config_hot_read.hpp"
#include "cpu_controller.hpp"
#include "gpu_controller.hpp"
#include "io_budget.hpp"
#include "memory_budget.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace astro::compute::utilization;

// ============================================================================
// CpuController
// ============================================================================

TEST(UtilCpu, DefaultTargetIs95Percent) {
    CpuController c;
    EXPECT_NEAR(c.target(), 0.95, 1e-9);
}

TEST(UtilCpu, SetTargetClamped) {
    CpuController c;
    c.set_target(2.0);
    EXPECT_NEAR(c.target(), 1.0, 1e-9);
    c.set_target(-1.0);
    EXPECT_NEAR(c.target(), 0.0, 1e-9);
    c.set_target(0.5);
    EXPECT_NEAR(c.target(), 0.5, 1e-9);
}

TEST(UtilCpu, DecideRecordsActualAndError) {
    CpuController c;
    c.set_target(0.95);
    auto d = c.decide(0.85);
    EXPECT_NEAR(d.target_ratio, 0.95, 1e-9);
    EXPECT_NEAR(d.actual_ratio, 0.85, 1e-9);
    EXPECT_NEAR(d.error_ratio, -0.10, 1e-9);
}

TEST(UtilCpu, BatchSizeStrategyAdjustsOnHighUtilization) {
    CpuController c;
    c.set_target(0.80);
    c.set_strategy(ControlStrategy::BatchSize);
    // actual=0.95 > target+0.05 → batch_size 应该减小（=1）
    auto d = c.decide(0.95);
    EXPECT_EQ(d.batch_size, 1u);
    // actual=0.50 < target-0.05 → batch_size 应该增大（=8）
    d = c.decide(0.50);
    EXPECT_EQ(d.batch_size, 8u);
}

TEST(UtilCpu, YieldStrategySetsShouldYield) {
    CpuController c;
    c.set_target(0.80);
    c.set_strategy(ControlStrategy::Yield);
    // actual > target + 0.05 → should_yield = true
    auto d = c.decide(0.95);
    EXPECT_TRUE(d.should_yield);
    // actual < target - 0.05 → should_yield = false
    d = c.decide(0.50);
    EXPECT_FALSE(d.should_yield);
}

TEST(UtilCpu, StatusJsonContainsFields) {
    CpuController c;
    c.set_target(0.90);
    c.decide(0.85);
    std::string s = c.status_json();
    EXPECT_NE(s.find("\"target\":0.9"), std::string::npos);
    EXPECT_NE(s.find("\"last_actual\""), std::string::npos);
}

// ============================================================================
// GpuController
// ============================================================================

TEST(UtilGpu, RegisterBackendAdds) {
    GpuController c;
    c.register_backend("cuda:0");
    c.register_backend("cuda:1");
    auto bs = c.backends();
    EXPECT_EQ(bs.size(), 2u);
    EXPECT_EQ(bs[0], "cuda:0");
    EXPECT_EQ(bs[1], "cuda:1");
}

TEST(UtilGpu, RegisterBackendIdempotent) {
    GpuController c;
    c.register_backend("cuda:0");
    c.register_backend("cuda:0");  // 重复
    EXPECT_EQ(c.backends().size(), 1u);
}

TEST(UtilGpu, DecideThrottleOnSevereExceed) {
    GpuController c;
    c.set_target(0.80);
    c.register_backend("cuda:0");
    // actual=1.0 > target+0.15 → throttle=true
    auto d = c.decide("cuda:0", 1.0);
    EXPECT_TRUE(d.throttle);
    EXPECT_EQ(d.queue_depth, 1u);
}

TEST(UtilGpu, DecideLowUtilizationNoThrottle) {
    GpuController c;
    c.set_target(0.95);
    c.register_backend("cuda:0");
    auto d = c.decide("cuda:0", 0.50);
    EXPECT_FALSE(d.throttle);
    EXPECT_EQ(d.queue_depth, 4u);
}

// ============================================================================
// MemoryBudgetController
// ============================================================================

TEST(UtilMemory, ComputeLimitMinRatioAndReserve) {
    // total=1000, ratio=0.9, reserve=100 → min(900, 900) = 900
    EXPECT_EQ(MemoryBudgetController::compute_limit(1000, 0.9, 100), 900u);
    // total=1000, ratio=0.5, reserve=100 → min(500, 900) = 500
    EXPECT_EQ(MemoryBudgetController::compute_limit(1000, 0.5, 100), 500u);
    // total=1000, ratio=1.0, reserve=200 → min(1000, 800) = 800
    EXPECT_EQ(MemoryBudgetController::compute_limit(1000, 1.0, 200), 800u);
    // total=0 → 0
    EXPECT_EQ(MemoryBudgetController::compute_limit(0, 0.9, 100), 0u);
    // total < reserve → 0
    EXPECT_EQ(MemoryBudgetController::compute_limit(50, 0.9, 100), 0u);
}

TEST(UtilMemory, ReportDetectsExceeded) {
    MemoryBudgetController c;
    MemoryBudgetConfig cfg;
    cfg.ram_ratio = 0.9;
    cfg.vram_ratio = 0.8;
    cfg.fixed_reserve_bytes = 100;
    c.configure(cfg);
    c.set_system_memory(1000, 1000);
    // limit_ram = min(900, 900) = 900, limit_vram = min(800, 900) = 800
    auto m = c.report(950, 700);
    EXPECT_EQ(m.limit_ram, 900u);
    EXPECT_EQ(m.limit_vram, 800u);
    EXPECT_TRUE(m.ram_exceeded);
    EXPECT_FALSE(m.vram_exceeded);
}

TEST(UtilMemory, DefaultFixedReserveIs512MB) {
    MemoryBudgetController c;
    auto cfg = c.config();
    EXPECT_EQ(cfg.fixed_reserve_bytes, 512ULL * 1024 * 1024);
}

TEST(UtilMemory, StatusJsonContainsFields) {
    MemoryBudgetController c;
    c.set_system_memory(1000, 500);
    c.report(100, 50);
    std::string s = c.status_json();
    EXPECT_NE(s.find("\"total_ram\":1000"), std::string::npos);
    EXPECT_NE(s.find("\"total_vram\":500"), std::string::npos);
    EXPECT_NE(s.find("\"ram_ratio\""), std::string::npos);
}

// ============================================================================
// IoBudgetController
// ============================================================================

TEST(UtilIo, ZeroBudgetNoLimit) {
    IoBudgetController c;
    IoBudgetConfig cfg;
    cfg.budget_mbps = 0.0;  // 不限速
    c.configure(cfg);
    auto d = c.report(1000.0);
    EXPECT_FALSE(d.exceeded);
    EXPECT_FALSE(d.warn);
    EXPECT_EQ(d.utilization_ratio, 0.0);
}

TEST(UtilIo, ExceedBudgetDetected) {
    IoBudgetController c;
    IoBudgetConfig cfg;
    cfg.budget_mbps = 100.0;
    cfg.warn_threshold = 0.9;
    c.configure(cfg);
    auto d = c.report(150.0);  // 150%
    EXPECT_TRUE(d.exceeded);
    EXPECT_TRUE(d.warn);
    EXPECT_NEAR(d.utilization_ratio, 1.5, 1e-9);
}

TEST(UtilIo, WarnAtThreshold) {
    IoBudgetController c;
    IoBudgetConfig cfg;
    cfg.budget_mbps = 100.0;
    cfg.warn_threshold = 0.9;
    c.configure(cfg);
    auto d = c.report(95.0);  // 95% > 90% → warn
    EXPECT_FALSE(d.exceeded);
    EXPECT_TRUE(d.warn);
}

// ============================================================================
// ConfigHotReader
// ============================================================================

TEST(UtilConfig, InitSetsAllValues) {
    ConfigHotReader r;
    HotConfig cfg;
    cfg.cpu_target_ratio = 0.80;
    cfg.gpu_target_ratio = 0.90;
    cfg.ram_ratio = 0.85;
    cfg.io_budget_mbps = 200.0;
    cfg.max_threads = 8;
    cfg.gpu_backend = "cuda:0";
    r.init(cfg);
    EXPECT_TRUE(r.initialized());
    auto read = r.read();
    EXPECT_NEAR(read.cpu_target_ratio, 0.80, 1e-9);
    EXPECT_NEAR(read.gpu_target_ratio, 0.90, 1e-9);
    EXPECT_NEAR(read.ram_ratio, 0.85, 1e-9);
    EXPECT_NEAR(read.io_budget_mbps, 200.0, 1e-9);
    EXPECT_EQ(read.max_threads, 8u);
    EXPECT_EQ(read.gpu_backend, "cuda:0");
}

TEST(UtilConfig, UpdateHotChangesMutableOnly) {
    ConfigHotReader r;
    HotConfig cfg;
    cfg.cpu_target_ratio = 0.80;
    cfg.max_threads = 8;
    cfg.gpu_backend = "cuda:0";
    r.init(cfg);
    HotConfig update;
    update.cpu_target_ratio = 0.50;
    update.max_threads = 16;       // ColdStatic，不应改变
    update.gpu_backend = "cuda:1"; // ColdStatic，不应改变
    r.update_hot(update);
    auto read = r.read();
    EXPECT_NEAR(read.cpu_target_ratio, 0.50, 1e-9);
    EXPECT_EQ(read.max_threads, 8u);  // 未变
    EXPECT_EQ(read.gpu_backend, "cuda:0");  // 未变
}

TEST(UtilConfig, SingleFieldSettersWork) {
    ConfigHotReader r;
    HotConfig cfg;
    r.init(cfg);
    r.set_cpu_target(0.75);
    r.set_gpu_target(0.85);
    r.set_ram_ratio(0.95);
    r.set_io_budget(100.0);
    auto read = r.read();
    EXPECT_NEAR(read.cpu_target_ratio, 0.75, 1e-9);
    EXPECT_NEAR(read.gpu_target_ratio, 0.85, 1e-9);
    EXPECT_NEAR(read.ram_ratio, 0.95, 1e-9);
    EXPECT_NEAR(read.io_budget_mbps, 100.0, 1e-9);
}

TEST(UtilConfig, ReadBeforeInitReturnsDefaults) {
    ConfigHotReader r;
    EXPECT_FALSE(r.initialized());
    auto read = r.read();
    EXPECT_NEAR(read.cpu_target_ratio, 0.95, 1e-9);  // 默认 0.95
}

// ============================================================================
// ActualTracker
// ============================================================================

TEST(UtilTracker, RecordAndCount) {
    ActualTracker t;
    EXPECT_EQ(t.sample_count(), 0u);
    UtilizationSample s1{1, 0.95, 0.95, 0.0, "cpu"};
    UtilizationSample s2{2, 0.85, 0.95, -0.10, "cpu"};
    t.record(s1);
    t.record(s2);
    EXPECT_EQ(t.sample_count(), 2u);
}

TEST(UtilTracker, RecentReturnsLastN) {
    ActualTracker t;
    for (int i = 0; i < 10; ++i) {
        UtilizationSample s{static_cast<std::uint64_t>(i), 0.9, 0.9, 0.0, "cpu"};
        t.record(s);
    }
    auto r = t.recent(3);
    EXPECT_EQ(r.size(), 3u);
    EXPECT_EQ(r[0].timestamp_ns, 7u);
    EXPECT_EQ(r[2].timestamp_ns, 9u);
}

TEST(UtilTracker, CapacityEvictsOldest) {
    ActualTracker t;
    t.set_capacity(3);
    for (int i = 0; i < 5; ++i) {
        UtilizationSample s{static_cast<std::uint64_t>(i), 0.9, 0.9, 0.0, "cpu"};
        t.record(s);
    }
    auto all = t.all();
    EXPECT_EQ(all.size(), 3u);
    EXPECT_EQ(all[0].timestamp_ns, 2u);  // 最老的 0,1 被驱逐
    EXPECT_EQ(all[2].timestamp_ns, 4u);
}

TEST(UtilTracker, StatisticsComputeCorrect) {
    ActualTracker t;
    UtilizationSample s1{1, 0.95, 0.95, 0.0, "cpu"};
    UtilizationSample s2{2, 0.90, 0.95, -0.05, "cpu"};
    UtilizationSample s3{3, 1.00, 0.95, 0.05, "cpu"};
    t.record(s1);
    t.record(s2);
    t.record(s3);
    EXPECT_NEAR(t.average_error(), 0.0, 1e-9);   // (0 - 0.05 + 0.05) / 3
    EXPECT_NEAR(t.max_error(), 0.05, 1e-9);
    EXPECT_NEAR(t.min_error(), -0.05, 1e-9);
}

TEST(UtilTracker, EmptyStatsSafe) {
    ActualTracker t;
    EXPECT_EQ(t.sample_count(), 0u);
    EXPECT_EQ(t.average_error(), 0.0);
    auto r = t.recent(10);
    EXPECT_TRUE(r.empty());
}

TEST(UtilTracker, StatusJsonContainsFields) {
    ActualTracker t;
    UtilizationSample s{1, 0.95, 0.95, 0.0, "cpu"};
    t.record(s);
    std::string json = t.status_json();
    EXPECT_NE(json.find("\"sample_count\":1"), std::string::npos);
    EXPECT_NE(json.find("\"recent\""), std::string::npos);
    EXPECT_NE(json.find("\"average_error\""), std::string::npos);
}

// ============================================================================
// main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
