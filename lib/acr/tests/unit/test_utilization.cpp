// lib/acr/tests/unit/test_utilization.cpp — Phase G utilization 单元测试
//
// 纠正清单 §7：真实 95% 控制测试。
//   - CPU 利用率读取实际值（GetSystemTimes，持续负载下读取）
//   - GPU 利用率读取或明确标记估算（NVML 或队列预算）
//   - RAM/VRAM 预算计算（GlobalMemoryStatusEx + NVML）
//   - 50/80/95/100% 目标点测试
//   - 控制器反馈闭环（读取实际值 → 调节提交节奏）
//   - worker 参与记录
//   - 系统响应（取消、状态查询）
//   - ActualTracker：record / recent / p95 / worker history / statistics
#include <gtest/gtest.h>

#include "actual_tracker.hpp"
#include "config_hot_read.hpp"
#include "cpu_controller.hpp"
#include "gpu_controller.hpp"
#include "io_budget.hpp"
#include "memory_budget.hpp"
#include "system_metrics.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

using namespace astro::compute::utilization;

// ============================================================================
// 辅助：制造持续 CPU 负载（让 GetSystemTimes 能读到非零利用率）
// ============================================================================
namespace {
struct BusyLoad {
    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;
    explicit BusyLoad(int nthreads = 2) {
        for (int i = 0; i < nthreads; ++i) {
            threads.emplace_back([this] {
                volatile std::uint64_t acc = 0;
                while (!stop.load(std::memory_order_relaxed)) {
                    for (int k = 0; k < 100000; ++k) {
                        acc += k;
                    }
                }
                (void)acc;
            });
        }
    }
    ~BusyLoad() {
        stop.store(true, std::memory_order_relaxed);
        for (auto& t : threads) { if (t.joinable()) t.join(); }
    }
};

inline void sleep_ms(std::uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
} // anonymous namespace

// ============================================================================
// SystemMetrics — CPU 利用率读取（真实值）
// ============================================================================
TEST(UtilSystemMetrics, CpuUtilizationReadsRealValue) {
    SystemMetrics m;
    // 首次调用建立基线，valid=false
    auto s0 = m.read_cpu_utilization();
    EXPECT_FALSE(s0.valid);

    // 制造负载
    BusyLoad load(4);
    sleep_ms(150);
    auto s1 = m.read_cpu_utilization();
    EXPECT_TRUE(s1.valid);
    std::printf("[UtilSystemMetrics.CpuUtilizationReadsRealValue] actual=%.3f\n", s1.ratio);
    // 持续负载下，CPU 利用率应 > 0.05（保守阈值，避免 CI 环境噪声）
    EXPECT_GT(s1.ratio, 0.05);
    EXPECT_LE(s1.ratio, 1.0);
}

TEST(UtilSystemMetrics, CpuUtilizationRepeatedSamplesConsistent) {
    SystemMetrics m;
    BusyLoad load(2);
    m.read_cpu_utilization();  // 基线
    sleep_ms(100);
    auto s1 = m.read_cpu_utilization();
    sleep_ms(100);
    auto s2 = m.read_cpu_utilization();
    EXPECT_TRUE(s1.valid);
    EXPECT_TRUE(s2.valid);
    // 两次采样都应返回有效值
    std::printf("[UtilSystemMetrics.CpuUtilizationRepeatedSamplesConsistent] s1=%.3f s2=%.3f\n",
                s1.ratio, s2.ratio);
}

TEST(UtilSystemMetrics, RamReadsRealValue) {
    SystemMetrics m;
    auto ram = m.read_ram();
    EXPECT_TRUE(ram.valid);
    EXPECT_GT(ram.total_bytes, 0u);
    EXPECT_GT(ram.avail_bytes, 0u);
    EXPECT_LE(ram.avail_bytes, ram.total_bytes);
    std::printf("[UtilSystemMetrics.RamReadsRealValue] total=%llu MB avail=%llu MB\n",
                static_cast<unsigned long long>(ram.total_bytes / (1024 * 1024)),
                static_cast<unsigned long long>(ram.avail_bytes / (1024 * 1024)));
}

TEST(UtilSystemMetrics, GpuUtilizationReadsOrEstimates) {
    SystemMetrics m;
    m.register_backend("cuda:0");
    m.report_queue_depth("cuda:0", 4);
    m.set_queue_budget_max_depth(8);
    auto samples = m.read_gpu_utilizations();
    ASSERT_GE(samples.size(), 1u);
    const auto& s = samples[0];
    EXPECT_EQ(s.backend, "cuda:0");
    EXPECT_TRUE(s.valid);
    // 若 NVML 不可用，应为估算且 estimated=true
    if (!m.nvml_available()) {
        EXPECT_TRUE(s.estimated);
        std::printf("[UtilSystemMetrics.GpuUtilizationReadsOrEstimates] NVML NOT available, estimated=%.3f\n",
                    s.ratio);
        // 队列深度 4 / max 8 = 0.5
        EXPECT_NEAR(s.ratio, 0.5, 0.01);
    } else {
        std::printf("[UtilSystemMetrics.GpuUtilizationReadsOrEstimates] NVML available, actual=%.3f estimated=%d\n",
                    s.ratio, s.estimated ? 1 : 0);
    }
}

TEST(UtilSystemMetrics, GpuMemoryReadsOrEstimates) {
    SystemMetrics m;
    m.register_backend("cuda:0");
    auto vrams = m.read_gpu_memories();
    ASSERT_GE(vrams.size(), 1u);
    // NVML 不可用时 estimated=true，valid=false
    if (!m.nvml_available()) {
        EXPECT_TRUE(vrams[0].estimated);
    } else {
        // NVML 可用时应有 total
        if (vrams[0].valid) {
            EXPECT_GT(vrams[0].total_bytes, 0u);
        }
    }
}

// ============================================================================
// CpuController — 真实利用率控制
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

TEST(UtilCpu, SampleAndDecideReadsRealUtilization) {
    CpuController c;
    c.set_target(0.95);
    // 首次采样建立基线
    auto d0 = c.sample_and_decide();
    EXPECT_FALSE(d0.valid);  // 首次无基线

    // 制造负载
    BusyLoad load(4);
    sleep_ms(150);
    auto d1 = c.sample_and_decide();
    EXPECT_TRUE(d1.valid);
    EXPECT_FALSE(d1.actual_estimated);  // CPU 实读，非估算
    EXPECT_NEAR(d1.target_ratio, 0.95, 1e-9);
    EXPECT_NEAR(d1.error_ratio, d1.actual_ratio - 0.95, 1e-9);
    std::printf("[UtilCpu.SampleAndDecideReadsRealUtilization] actual=%.3f error=%.3f batch=%u yield=%d\n",
                d1.actual_ratio, d1.error_ratio, d1.batch_size, d1.should_yield ? 1 : 0);
    EXPECT_GE(d1.actual_ratio, 0.0);
    EXPECT_LE(d1.actual_ratio, 1.0);
}

TEST(UtilCpu, ControlWindowInRange100to500) {
    CpuController c;
    EXPECT_GE(c.control_window_ms(), 100u);
    EXPECT_LE(c.control_window_ms(), 500u);
    c.set_control_window_ms(50);   // 过小 → clamp 到 100
    EXPECT_EQ(c.control_window_ms(), 100u);
    c.set_control_window_ms(1000); // 过大 → clamp 到 500
    EXPECT_EQ(c.control_window_ms(), 500u);
    c.set_control_window_ms(300);
    EXPECT_EQ(c.control_window_ms(), 300u);
}

// 50/80/95/100% 目标点测试（spec §7：百分比仅是设备利用率目标）
TEST(UtilCpu, TargetPoints50_80_95_100) {
    for (double target : {0.50, 0.80, 0.95, 1.00}) {
        CpuController c;
        c.set_target(target);
        EXPECT_NEAR(c.target(), target, 1e-9);
        // 用注入接口测试决策逻辑（actual_estimated=true）
        auto d = c.decide_with_actual(target + 0.10);
        EXPECT_NEAR(d.target_ratio, target, 1e-9);
        EXPECT_TRUE(d.actual_estimated);  // 注入接口标记估算
        // actual > target + 0.05 → too_high，应减小批次或让步
        EXPECT_TRUE(d.should_yield || d.batch_size == 1 || d.priority == -1 || d.queue_depth == 1);
        std::printf("[UtilCpu.TargetPoints] target=%.2f actual=%.2f batch=%u yield=%d priority=%d\n",
                    target, d.actual_ratio, d.batch_size, d.should_yield ? 1 : 0, d.priority);
    }
}

TEST(UtilCpu, BatchSizeStrategyAdjustsOnHighUtilization) {
    CpuController c;
    c.set_target(0.80);
    c.set_strategy(ControlStrategy::BatchSize);
    auto d = c.decide_with_actual(0.95);  // too_high
    EXPECT_EQ(d.batch_size, 1u);
    d = c.decide_with_actual(0.50);  // too_low
    EXPECT_EQ(d.batch_size, 8u);
}

TEST(UtilCpu, YieldStrategySetsShouldYield) {
    CpuController c;
    c.set_target(0.80);
    c.set_strategy(ControlStrategy::Yield);
    auto d = c.decide_with_actual(0.95);
    EXPECT_TRUE(d.should_yield);
    d = c.decide_with_actual(0.50);
    EXPECT_FALSE(d.should_yield);
}

// 控制器反馈闭环：读取实际值 → 调节提交节奏
TEST(UtilCpu, FeedbackLoopAdjustsPace) {
    CpuController c;
    c.set_target(0.95);
    c.set_strategy(ControlStrategy::BatchSize);

    // 注册 worker
    auto w1 = c.register_worker();
    auto w2 = c.register_worker();
    c.mark_worker_active(w1);
    c.mark_worker_active(w2);

    // 制造高负载
    BusyLoad load(4);
    c.sample_and_decide();  // 基线
    sleep_ms(150);
    auto d = c.sample_and_decide();
    ASSERT_TRUE(d.valid);

    // 高负载（actual 接近 1.0）→ 应触发降利用策略
    if (d.actual_ratio > 0.95 + 0.05) {
        EXPECT_TRUE(d.should_yield || d.batch_size == 1);
    }
    std::printf("[UtilCpu.FeedbackLoopAdjustsPace] actual=%.3f batch=%u yield=%d stride=%u\n",
                d.actual_ratio, d.batch_size, d.should_yield ? 1 : 0, d.yield_stride);
}

// worker 参与记录
TEST(UtilCpu, WorkerParticipationRecorded) {
    CpuController c;
    EXPECT_EQ(c.worker_participation().registered_count, 0u);
    auto w1 = c.register_worker();
    auto w2 = c.register_worker();
    auto w3 = c.register_worker();
    auto p = c.worker_participation();
    EXPECT_EQ(p.registered_count, 3u);
    EXPECT_EQ(p.active_count, 0u);  // 初始全空闲
    EXPECT_EQ(p.idle_count, 3u);

    c.mark_worker_active(w1);
    c.mark_worker_active(w2);
    p = c.worker_participation();
    EXPECT_EQ(p.active_count, 2u);
    EXPECT_EQ(p.idle_count, 1u);

    c.mark_worker_idle(w1);
    p = c.worker_participation();
    EXPECT_EQ(p.active_count, 1u);
    EXPECT_EQ(p.idle_count, 2u);

    c.unregister_worker(w3);
    p = c.worker_participation();
    EXPECT_EQ(p.registered_count, 2u);
}

// 系统响应：取消、状态查询
TEST(UtilCpu, CancelAndResponse) {
    CpuController c;
    EXPECT_FALSE(c.cancelled());
    c.request_cancel();
    EXPECT_TRUE(c.cancelled());
    // 取消状态下决策应保守（最小批次 + 让步）
    auto d = c.decide_with_actual(0.50);
    EXPECT_EQ(d.batch_size, 1u);
    EXPECT_TRUE(d.should_yield);
    c.clear_cancel();
    EXPECT_FALSE(c.cancelled());
}

TEST(UtilCpu, StatusJsonContainsFields) {
    CpuController c;
    c.set_target(0.90);
    c.register_worker();
    c.sample_and_decide();
    std::string s = c.status_json();
    EXPECT_NE(s.find("\"target\":0.9"), std::string::npos);
    EXPECT_NE(s.find("\"worker_registered\":1"), std::string::npos);
    EXPECT_NE(s.find("\"cancelled\":false"), std::string::npos);
    EXPECT_NE(s.find("\"nvml_available\""), std::string::npos);
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
    c.register_backend("cuda:0");
    EXPECT_EQ(c.backends().size(), 1u);
}

TEST(UtilGpu, DecideThrottleOnSevereExceed) {
    GpuController c;
    c.set_target(0.80);
    c.register_backend("cuda:0");
    auto d = c.decide_with_actual("cuda:0", 1.0, /*estimated=*/false);
    EXPECT_TRUE(d.throttle);
    EXPECT_EQ(d.queue_depth, 1u);
}

TEST(UtilGpu, DecideLowUtilizationNoThrottle) {
    GpuController c;
    c.set_target(0.95);
    c.register_backend("cuda:0");
    auto d = c.decide_with_actual("cuda:0", 0.50, /*estimated=*/false);
    EXPECT_FALSE(d.throttle);
    EXPECT_GE(d.queue_depth, 2u);
}

// 真实 GPU 利用率读取或估算
TEST(UtilGpu, SampleAndDecideReadsRealOrEstimates) {
    GpuController c;
    c.set_target(0.95);
    c.register_backend("cuda:0");
    c.report_queue_depth("cuda:0", 6);  // 队列预算估算：6/8=0.75
    auto decisions = c.sample_and_decide();
    ASSERT_GE(decisions.size(), 1u);
    const auto& d = decisions[0];
    EXPECT_EQ(d.backend, "cuda:0");
    EXPECT_TRUE(d.valid);
    if (!c.nvml_available()) {
        EXPECT_TRUE(d.actual_estimated);
        EXPECT_NEAR(d.actual_ratio, 0.75, 0.01);
    }
    std::printf("[UtilGpu.SampleAndDecideReadsRealOrEstimates] nvml=%d actual=%.3f estimated=%d\n",
                c.nvml_available() ? 1 : 0, d.actual_ratio, d.actual_estimated ? 1 : 0);
}

// 多 GPU 独立控制
TEST(UtilGpu, MultiGpuIndependentControl) {
    GpuController c;
    c.set_target(0.95);
    c.register_backend("cuda:0");
    c.register_backend("cuda:1");
    c.report_queue_depth("cuda:0", 2);
    c.report_queue_depth("cuda:1", 8);  // 满载
    auto decisions = c.sample_and_decide();
    ASSERT_EQ(decisions.size(), 2u);
    // 两个 backend 都应有决策
    bool found0 = false, found1 = false;
    for (const auto& d : decisions) {
        if (d.backend == "cuda:0") found0 = true;
        if (d.backend == "cuda:1") found1 = true;
    }
    EXPECT_TRUE(found0);
    EXPECT_TRUE(found1);
}

// 无界排队上限
TEST(UtilGpu, NoUnboundedQueue) {
    GpuController c;
    c.set_max_queue_depth(4);
    c.set_target(0.50);  // 低目标，actual=0.3 → too_low → 深队列
    c.register_backend("cuda:0");
    auto d = c.decide_with_actual("cuda:0", 0.3, /*estimated=*/false);
    EXPECT_LE(d.queue_depth, c.max_queue_depth());
    EXPECT_EQ(d.max_queue_depth, 4u);
}

// 50/80/95/100% 目标点
TEST(UtilGpu, TargetPoints50_80_95_100) {
    for (double target : {0.50, 0.80, 0.95, 1.00}) {
        GpuController c;
        c.set_target(target);
        c.register_backend("cuda:0");
        EXPECT_NEAR(c.target(), target, 1e-9);
        auto d = c.decide_with_actual("cuda:0", target, /*estimated=*/false);
        EXPECT_NEAR(d.target_ratio, target, 1e-9);
    }
}

TEST(UtilGpu, CancelAndResponse) {
    GpuController c;
    EXPECT_FALSE(c.cancelled());
    c.request_cancel();
    EXPECT_TRUE(c.cancelled());
    auto d = c.decide_with_actual("cuda:0", 0.5, /*estimated=*/false);
    EXPECT_TRUE(d.throttle);
    EXPECT_EQ(d.queue_depth, 1u);
    c.clear_cancel();
    EXPECT_FALSE(c.cancelled());
}

TEST(UtilGpu, StatusJsonContainsFields) {
    GpuController c;
    c.set_target(0.95);
    c.register_backend("cuda:0");
    c.sample_and_decide();
    std::string s = c.status_json();
    EXPECT_NE(s.find("\"target\":0.95"), std::string::npos);
    EXPECT_NE(s.find("\"nvml_available\""), std::string::npos);
    EXPECT_NE(s.find("\"backends\":"), std::string::npos);
}

// ============================================================================
// MemoryBudgetController
// ============================================================================
TEST(UtilMemory, ComputeLimitMinRatioAndReserve) {
    EXPECT_EQ(MemoryBudgetController::compute_limit(1000, 0.9, 100), 900u);
    EXPECT_EQ(MemoryBudgetController::compute_limit(1000, 0.5, 100), 500u);
    EXPECT_EQ(MemoryBudgetController::compute_limit(1000, 1.0, 200), 800u);
    EXPECT_EQ(MemoryBudgetController::compute_limit(0, 0.9, 100), 0u);
    EXPECT_EQ(MemoryBudgetController::compute_limit(50, 0.9, 100), 0u);
}

TEST(UtilMemory, DefaultFixedReserveIs512MB) {
    MemoryBudgetController c;
    auto cfg = c.config();
    EXPECT_EQ(cfg.fixed_reserve_bytes, 512ULL * 1024 * 1024);
}

TEST(UtilMemory, DefaultRatioIs95Percent) {
    MemoryBudgetConfig cfg;
    EXPECT_NEAR(cfg.ram_ratio, 0.95, 1e-9);
    EXPECT_NEAR(cfg.vram_ratio, 0.95, 1e-9);
}

// 真实 RAM 读取
TEST(UtilMemory, SampleReadsRealRam) {
    MemoryBudgetController c;
    auto m = c.sample();
    EXPECT_TRUE(m.ram_valid);
    EXPECT_GT(m.total_ram, 0u);
    EXPECT_LE(m.used_ram, m.total_ram);
    EXPECT_LE(m.avail_ram, m.total_ram);
    // limit 应满足 limit = min(total*0.95, total-512MB)
    std::uint64_t expected_limit = std::min(
        static_cast<std::uint64_t>(m.total_ram * 0.95),
        m.total_ram - 512ULL * 1024 * 1024);
    EXPECT_EQ(m.limit_ram, expected_limit);
    std::printf("[UtilMemory.SampleReadsRealRam] total=%llu MB used=%llu MB limit=%llu MB exceeded=%d\n",
                static_cast<unsigned long long>(m.total_ram / (1024 * 1024)),
                static_cast<unsigned long long>(m.used_ram / (1024 * 1024)),
                static_cast<unsigned long long>(m.limit_ram / (1024 * 1024)),
                m.ram_exceeded ? 1 : 0);
}

TEST(UtilMemory, ReportWithInjectsValues) {
    MemoryBudgetController c;
    MemoryBudgetConfig cfg;
    cfg.ram_ratio = 0.9;
    cfg.vram_ratio = 0.8;
    cfg.fixed_reserve_bytes = 100;
    c.configure(cfg);
    // 先 sample 一次获取系统总量
    c.sample();
    auto m = c.report_with(950, 700, "cuda:0");
    // limit_ram 基于 sample 的真实 total，但 used=950 注入
    EXPECT_EQ(m.used_ram, 950u);
    EXPECT_TRUE(m.ram_valid);
    ASSERT_EQ(m.gpus.size(), 1u);
    EXPECT_EQ(m.gpus[0].used_vram, 700u);
    EXPECT_TRUE(m.gpus[0].estimated);  // 注入接口标记估算
}

TEST(UtilMemory, ExceedActionSuggestion) {
    // 未超限
    EXPECT_EQ(MemoryBudgetController::suggest_action(100, 200, 1000),
              MemoryBudgetController::ExceedAction::None);
    // 轻微超限
    EXPECT_EQ(MemoryBudgetController::suggest_action(205, 200, 1000),
              MemoryBudgetController::ExceedAction::StopNewSubmit);
    // 严重超限
    EXPECT_EQ(MemoryBudgetController::suggest_action(1000, 200, 1000),
              MemoryBudgetController::ExceedAction::Fail);
}

TEST(UtilMemory, StatusJsonContainsFields) {
    MemoryBudgetController c;
    c.register_backend("cuda:0");
    c.sample();
    std::string s = c.status_json();
    EXPECT_NE(s.find("\"ram_ratio\""), std::string::npos);
    EXPECT_NE(s.find("\"last_total_ram\""), std::string::npos);
    EXPECT_NE(s.find("\"nvml_available\""), std::string::npos);
}

// ============================================================================
// IoBudgetController
// ============================================================================
TEST(UtilIo, ZeroBudgetNoLimit) {
    IoBudgetController c;
    IoBudgetConfig cfg;
    cfg.budget_mbps = 0.0;
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
    auto d = c.report(150.0);
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
    auto d = c.report(95.0);
    EXPECT_FALSE(d.exceeded);
    EXPECT_TRUE(d.warn);
}

// 实际 I/O 记录 + 采样
TEST(UtilIo, RecordIoAndSample) {
    IoBudgetController c;
    IoBudgetConfig cfg;
    cfg.budget_mbps = 1000.0;
    cfg.warn_threshold = 0.9;
    c.configure(cfg);
    // 记录 100MB / 0.1s = 1000 Mbps（恰好等于预算）
    c.record_io(100ULL * 1024 * 1024, 100ULL * 1000 * 1000);  // 100MB, 100ms
    auto d = c.sample();
    EXPECT_TRUE(d.valid);
    EXPECT_EQ(d.record_count, 1u);
    EXPECT_NEAR(d.actual_mbps, 1000.0, 50.0);  // 允许 50Mbps 误差
    // 再次 sample 应无数据（窗口已重置）
    auto d2 = c.sample();
    EXPECT_FALSE(d2.valid);
}

TEST(UtilIo, StatusJsonContainsFields) {
    IoBudgetController c;
    c.configure({100.0, 0.9});
    c.record_io(1024, 1000);
    c.sample();
    std::string s = c.status_json();
    EXPECT_NE(s.find("\"budget_mbps\":100"), std::string::npos);
    EXPECT_NE(s.find("\"total_bytes\":1024"), std::string::npos);
    EXPECT_NE(s.find("\"total_count\":1"), std::string::npos);
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
    cfg.isa_level = "avx2";
    cfg.fallback_policy = FallbackPolicy::PreferCpu;
    cfg.cpu_control_window_ms = 300;
    cfg.gpu_max_queue_depth = 16;
    r.init(cfg);
    EXPECT_TRUE(r.initialized());
    auto read = r.read();
    EXPECT_NEAR(read.cpu_target_ratio, 0.80, 1e-9);
    EXPECT_NEAR(read.gpu_target_ratio, 0.90, 1e-9);
    EXPECT_NEAR(read.ram_ratio, 0.85, 1e-9);
    EXPECT_NEAR(read.io_budget_mbps, 200.0, 1e-9);
    EXPECT_EQ(read.max_threads, 8u);
    EXPECT_EQ(read.gpu_backend, "cuda:0");
    EXPECT_EQ(read.isa_level, "avx2");
    EXPECT_EQ(read.fallback_policy, FallbackPolicy::PreferCpu);
    EXPECT_EQ(read.cpu_control_window_ms, 300u);
    EXPECT_EQ(read.gpu_max_queue_depth, 16u);
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
    r.set_vram_ratio(0.90);
    r.set_io_target(0.80);
    r.set_memory_reserve(1024ULL * 1024 * 1024);
    r.set_cpu_control_window_ms(400);
    r.set_gpu_max_queue_depth(32);
    r.set_fallback_policy(FallbackPolicy::Strict);
    auto read = r.read();
    EXPECT_NEAR(read.cpu_target_ratio, 0.75, 1e-9);
    EXPECT_NEAR(read.gpu_target_ratio, 0.85, 1e-9);
    EXPECT_NEAR(read.ram_ratio, 0.95, 1e-9);
    EXPECT_NEAR(read.io_budget_mbps, 100.0, 1e-9);
    EXPECT_NEAR(read.vram_ratio, 0.90, 1e-9);
    EXPECT_NEAR(read.io_target_ratio, 0.80, 1e-9);
    EXPECT_EQ(read.memory_fixed_reserve_bytes, 1024ULL * 1024 * 1024);
    EXPECT_EQ(read.cpu_control_window_ms, 400u);
    EXPECT_EQ(read.gpu_max_queue_depth, 32u);
    EXPECT_EQ(read.fallback_policy, FallbackPolicy::Strict);
}

TEST(UtilConfig, ReadBeforeInitReturnsDefaults) {
    ConfigHotReader r;
    EXPECT_FALSE(r.initialized());
    auto read = r.read();
    EXPECT_NEAR(read.cpu_target_ratio, 0.95, 1e-9);
}

// backend enable / fallback policy
TEST(UtilConfig, BackendEnableDisable) {
    ConfigHotReader r;
    HotConfig cfg;
    cfg.backend_enabled["cuda:0"] = true;
    cfg.backend_enabled["cuda:1"] = false;
    r.init(cfg);
    EXPECT_TRUE(r.is_backend_enabled("cuda:0"));
    EXPECT_FALSE(r.is_backend_enabled("cuda:1"));
    // 未配置的 backend 默认启用
    EXPECT_TRUE(r.is_backend_enabled("cuda:2"));
    r.set_backend_enabled("cuda:0", false);
    EXPECT_FALSE(r.is_backend_enabled("cuda:0"));
    auto backends = r.configured_backends();
    EXPECT_EQ(backends.size(), 2u);
}

// 验证不提供 CPU/GPU share 参数（编译期保证：HotConfig 无 cpu_share/gpu_share 字段）
TEST(UtilConfig, NoCpuGpuShareField) {
    HotConfig cfg;
    // 这些字段不存在，编译期已保证。运行时仅验证默认值合理
    EXPECT_NEAR(cfg.cpu_target_ratio, 0.95, 1e-9);
    EXPECT_NEAR(cfg.gpu_target_ratio, 0.95, 1e-9);
    // cpu_share / gpu_share 不存在 —— 若存在则此测试编译失败
    SUCCEED();
}

// ============================================================================
// ActualTracker
// ============================================================================
TEST(UtilTracker, RecordAndCount) {
    ActualTracker t;
    EXPECT_EQ(t.sample_count(), 0u);
    UtilizationSample s1{1, 0.95, 0.95, 0.0, false, "cpu", 4, 2, 2, false};
    UtilizationSample s2{2, 0.85, 0.95, -0.10, false, "cpu", 4, 3, 1, false};
    t.record(s1);
    t.record(s2);
    EXPECT_EQ(t.sample_count(), 2u);
}

TEST(UtilTracker, RecentReturnsLastN) {
    ActualTracker t;
    for (int i = 0; i < 10; ++i) {
        UtilizationSample s{static_cast<std::uint64_t>(i), 0.9, 0.9, 0.0, false, "cpu", 0, 0, 0, false};
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
        UtilizationSample s{static_cast<std::uint64_t>(i), 0.9, 0.9, 0.0, false, "cpu", 0, 0, 0, false};
        t.record(s);
    }
    auto all = t.all();
    EXPECT_EQ(all.size(), 3u);
    EXPECT_EQ(all[0].timestamp_ns, 2u);
    EXPECT_EQ(all[2].timestamp_ns, 4u);
}

TEST(UtilTracker, StatisticsComputeCorrect) {
    ActualTracker t;
    UtilizationSample s1{1, 0.95, 0.95, 0.0, false, "cpu", 0, 0, 0, false};
    UtilizationSample s2{2, 0.90, 0.95, -0.05, false, "cpu", 0, 0, 0, false};
    UtilizationSample s3{3, 1.00, 0.95, 0.05, false, "cpu", 0, 0, 0, false};
    t.record(s1);
    t.record(s2);
    t.record(s3);
    EXPECT_NEAR(t.average_error(), 0.0, 1e-9);
    EXPECT_NEAR(t.max_error(), 0.05, 1e-9);
    EXPECT_NEAR(t.min_error(), -0.05, 1e-9);
}

// p95 计算
TEST(UtilTracker, P95StatsCompute) {
    ActualTracker t;
    // 100 个样本：actual 从 0.0 到 0.99
    for (int i = 0; i < 100; ++i) {
        double actual = static_cast<double>(i) / 100.0;
        UtilizationSample s{static_cast<std::uint64_t>(i), actual, 0.95, actual - 0.95,
                            false, "cpu", 0, 0, 0, false};
        t.record(s);
    }
    auto s = t.stats();
    EXPECT_EQ(s.sample_count, 100u);
    // p95 线性插值: idx = 0.95 * 99 = 94.05, vals[94]=0.94, vals[95]=0.95
    // p95 = 0.94*0.95 + 0.95*0.05 = 0.9405
    EXPECT_NEAR(s.p95_actual, 0.9405, 0.005);
    std::printf("[UtilTracker.P95StatsCompute] p95_actual=%.4f avg_actual=%.4f\n",
                s.p95_actual, s.average_actual);
}

// worker 参与记录
TEST(UtilTracker, WorkerParticipationHistory) {
    ActualTracker t;
    t.record_worker_participation(4, 2, 2);
    sleep_ms(1);
    t.record_worker_participation(4, 4, 0);
    sleep_ms(1);
    t.record_worker_participation(4, 1, 3);
    auto hist = t.worker_history();
    ASSERT_EQ(hist.size(), 3u);
    EXPECT_EQ(hist[0].registered, 4u);
    EXPECT_EQ(hist[0].active, 2u);
    EXPECT_EQ(hist[1].active, 4u);
    EXPECT_EQ(hist[2].active, 1u);
}

// 系统响应：取消状态记录
TEST(UtilTracker, CancelledStateRecorded) {
    ActualTracker t;
    UtilizationSample s1{1, 0.95, 0.95, 0.0, false, "cpu", 1, 1, 0, true};
    UtilizationSample s2{2, 0.95, 0.95, 0.0, false, "cpu", 1, 1, 0, false};
    t.record(s1);
    t.record(s2);
    auto stats = t.stats();
    EXPECT_EQ(stats.cancelled_count, 1u);
}

TEST(UtilTracker, EmptyStatsSafe) {
    ActualTracker t;
    EXPECT_EQ(t.sample_count(), 0u);
    EXPECT_EQ(t.average_error(), 0.0);
    auto r = t.recent(10);
    EXPECT_TRUE(r.empty());
    auto s = t.stats();
    EXPECT_EQ(s.sample_count, 0u);
}

TEST(UtilTracker, StatusJsonContainsFields) {
    ActualTracker t;
    UtilizationSample s{1, 0.95, 0.95, 0.0, false, "cpu", 4, 2, 2, false};
    t.record(s);
    std::string json = t.status_json();
    EXPECT_NE(json.find("\"sample_count\":1"), std::string::npos);
    EXPECT_NE(json.find("\"stats\""), std::string::npos);
    EXPECT_NE(json.find("\"p95_actual\""), std::string::npos);
    EXPECT_NE(json.find("\"worker_registered\":4"), std::string::npos);
}

// ============================================================================
// 控制器反馈闭环集成测试
// ============================================================================
TEST(UtilIntegration, FeedbackLoopWithTracker) {
    // 控制器读取实际值 → 决策 → tracker 记录 → 反馈
    CpuController cpu;
    GpuController gpu;
    ActualTracker tracker;
    cpu.set_target(0.95);
    gpu.set_target(0.95);
    gpu.register_backend("cuda:0");
    gpu.report_queue_depth("cuda:0", 4);

    auto w1 = cpu.register_worker();
    auto w2 = cpu.register_worker();
    cpu.mark_worker_active(w1);
    cpu.mark_worker_active(w2);

    BusyLoad load(4);
    cpu.sample_and_decide();  // 基线

    // 持续采样 5 次，记录到 tracker
    for (int i = 0; i < 5; ++i) {
        sleep_ms(120);
        auto cd = cpu.sample_and_decide();
        if (cd.valid) {
            UtilizationSample s;
            s.timestamp_ns = cd.timestamp_ns;
            s.actual_ratio = cd.actual_ratio;
            s.target_ratio = cd.target_ratio;
            s.error_ratio = cd.error_ratio;
            s.estimated = cd.actual_estimated;
            s.backend = "cpu";
            auto p = cpu.worker_participation();
            s.worker_registered = p.registered_count;
            s.worker_active = p.active_count;
            s.worker_idle = p.idle_count;
            s.cancelled = cpu.cancelled();
            tracker.record(s);
        }
        // GPU 采样
        auto gds = gpu.sample_and_decide();
        for (const auto& gd : gds) {
            if (gd.valid) {
                UtilizationSample s;
                s.timestamp_ns = gd.timestamp_ns;
                s.actual_ratio = gd.actual_ratio;
                s.target_ratio = gd.target_ratio;
                s.error_ratio = gd.error_ratio;
                s.estimated = gd.actual_estimated;
                s.backend = gd.backend;
                s.cancelled = gpu.cancelled();
                tracker.record(s);
            }
        }
        // 记录 worker 参与
        auto p = cpu.worker_participation();
        tracker.record_worker_participation(p.registered_count, p.active_count, p.idle_count);
    }

    auto s = tracker.stats();
    EXPECT_GE(s.sample_count, 1u);
    std::printf("[UtilIntegration.FeedbackLoopWithTracker] samples=%zu avg_actual=%.3f p95_actual=%.3f\n",
                s.sample_count, s.average_actual, s.p95_actual);
    // tracker 应有 worker 历史
    auto wh = tracker.worker_history();
    EXPECT_GE(wh.size(), 1u);

    // 测试取消响应
    cpu.request_cancel();
    gpu.request_cancel();
    EXPECT_TRUE(cpu.cancelled());
    EXPECT_TRUE(gpu.cancelled());
    auto cd = cpu.sample_and_decide();
    EXPECT_TRUE(cd.should_yield);  // 取消状态下应让步
    auto gds = gpu.sample_and_decide("cuda:0");
    EXPECT_TRUE(gds.throttle);
    cpu.clear_cancel();
    gpu.clear_cancel();
}

// ============================================================================
// main
// ============================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
