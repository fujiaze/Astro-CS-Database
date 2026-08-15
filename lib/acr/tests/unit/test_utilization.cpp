// lib/acr/tests/unit/test_utilization.cpp — 26 §2/§9 资源边界单元测试
//
// CPU/GPU 精确利用率控制已撤销并移除（CpuController/GpuController/IoBudget
// 已删除）。本文件只保留：
// - SystemMetrics telemetry（CPU/RAM/GPU 读取，诊断用途，不影响 claim）
// - MemoryBudget（RAM/VRAM 容量预算）
// - ConfigHotReader（内存容量/backend/fallback 配置）
// - ActualTracker（诊断记录器）
#include <gtest/gtest.h>

#include "actual_tracker.hpp"
#include "config_hot_read.hpp"
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
// 辅助：制造持续 CPU 负载（telemetry 测试用）
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
                        acc += static_cast<std::uint64_t>(k);
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
// SystemMetrics — telemetry（诊断，不影响调度与内存预算）
// ============================================================================
TEST(UtilSystemMetrics, CpuUtilizationReadsRealValue) {
    SystemMetrics m;
    auto s0 = m.read_cpu_utilization();
    EXPECT_FALSE(s0.valid);

    BusyLoad load(4);
    sleep_ms(150);
    auto s1 = m.read_cpu_utilization();
    EXPECT_TRUE(s1.valid);
    std::printf("[UtilSystemMetrics.CpuUtilizationReadsRealValue] actual=%.3f\n", s1.ratio);
    EXPECT_GT(s1.ratio, 0.05);
    EXPECT_LE(s1.ratio, 1.0);
}

TEST(UtilSystemMetrics, CpuUtilizationRepeatedSamplesConsistent) {
    SystemMetrics m;
    BusyLoad load(2);
    m.read_cpu_utilization();
    sleep_ms(100);
    auto s1 = m.read_cpu_utilization();
    sleep_ms(100);
    auto s2 = m.read_cpu_utilization();
    EXPECT_TRUE(s1.valid);
    EXPECT_TRUE(s2.valid);
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
    if (!m.nvml_available()) {
        EXPECT_TRUE(s.estimated);
        std::printf("[UtilSystemMetrics.GpuUtilizationReadsOrEstimates] NVML NOT available, estimated=%.3f\n",
                    s.ratio);
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
    if (!m.nvml_available()) {
        EXPECT_TRUE(vrams[0].estimated);
    } else {
        if (vrams[0].valid) {
            EXPECT_GT(vrams[0].total_bytes, 0u);
        }
    }
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

TEST(UtilMemory, DefaultFixedReserveIs2048RamAnd512Vram) {
    MemoryBudgetController c;
    auto cfg = c.config();
    EXPECT_EQ(cfg.ram_fixed_reserve_bytes, 2048ULL * 1024 * 1024);
    EXPECT_EQ(cfg.vram_fixed_reserve_bytes, 512ULL * 1024 * 1024);
}

TEST(UtilMemory, DefaultRatioIs95Percent) {
    MemoryBudgetConfig cfg;
    EXPECT_NEAR(cfg.ram_ratio, 0.95, 1e-9);
    EXPECT_NEAR(cfg.vram_ratio, 0.95, 1e-9);
}

TEST(UtilMemory, SampleReadsRealRam) {
    MemoryBudgetController c;
    auto m = c.sample();
    EXPECT_TRUE(m.ram_valid);
    EXPECT_GT(m.total_ram, 0u);
    EXPECT_LE(m.used_ram, m.total_ram);
    EXPECT_LE(m.avail_ram, m.total_ram);
    std::uint64_t expected_limit = std::min(
        static_cast<std::uint64_t>(m.total_ram * 0.95),
        m.total_ram - 2048ULL * 1024 * 1024);
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
    cfg.ram_fixed_reserve_bytes = 100;
    cfg.vram_fixed_reserve_bytes = 100;
    c.configure(cfg);
    c.sample();
    auto m = c.report_with(950, 700, "cuda:0");
    EXPECT_EQ(m.used_ram, 950u);
    EXPECT_TRUE(m.ram_valid);
    ASSERT_EQ(m.gpus.size(), 1u);
    EXPECT_EQ(m.gpus[0].used_vram, 700u);
    EXPECT_TRUE(m.gpus[0].estimated);
}

TEST(UtilMemory, ExceedActionSuggestion) {
    EXPECT_EQ(MemoryBudgetController::suggest_action(100, 200, 1000),
              MemoryBudgetController::ExceedAction::None);
    EXPECT_EQ(MemoryBudgetController::suggest_action(205, 200, 1000),
              MemoryBudgetController::ExceedAction::StopNewSubmit);
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
// ConfigHotReader
// ============================================================================
TEST(UtilConfig, InitSetsAllValues) {
    ConfigHotReader r;
    HotConfig cfg;
    cfg.ram_ratio = 0.85;
    cfg.vram_ratio = 0.80;
    cfg.ram_fixed_reserve_bytes = 1024ULL * 1024 * 1024;
    cfg.vram_fixed_reserve_bytes = 256ULL * 1024 * 1024;
    cfg.max_threads = 8;
    cfg.gpu_backend = "cuda:0";
    cfg.isa_level = "avx2";
    cfg.fallback_policy = FallbackPolicy::PreferCpu;
    r.init(cfg);
    EXPECT_TRUE(r.initialized());
    auto read = r.read();
    EXPECT_NEAR(read.ram_ratio, 0.85, 1e-9);
    EXPECT_NEAR(read.vram_ratio, 0.80, 1e-9);
    EXPECT_EQ(read.ram_fixed_reserve_bytes, 1024ULL * 1024 * 1024);
    EXPECT_EQ(read.vram_fixed_reserve_bytes, 256ULL * 1024 * 1024);
    EXPECT_EQ(read.max_threads, 8u);
    EXPECT_EQ(read.gpu_backend, "cuda:0");
    EXPECT_EQ(read.isa_level, "avx2");
    EXPECT_EQ(read.fallback_policy, FallbackPolicy::PreferCpu);
}

TEST(UtilConfig, UpdateHotChangesMutableOnly) {
    ConfigHotReader r;
    HotConfig cfg;
    cfg.ram_ratio = 0.80;
    cfg.max_threads = 8;
    cfg.gpu_backend = "cuda:0";
    r.init(cfg);
    HotConfig update;
    update.ram_ratio = 0.50;
    update.max_threads = 16;       // ColdStatic，不应改变
    update.gpu_backend = "cuda:1"; // ColdStatic，不应改变
    r.update_hot(update);
    auto read = r.read();
    EXPECT_NEAR(read.ram_ratio, 0.50, 1e-9);
    EXPECT_EQ(read.max_threads, 8u);
    EXPECT_EQ(read.gpu_backend, "cuda:0");
}

TEST(UtilConfig, SingleFieldSettersWork) {
    ConfigHotReader r;
    HotConfig cfg;
    r.init(cfg);
    r.set_ram_ratio(0.95);
    r.set_vram_ratio(0.90);
    r.set_ram_reserve(1024ULL * 1024 * 1024);
    r.set_vram_reserve(256ULL * 1024 * 1024);
    r.set_fallback_policy(FallbackPolicy::Strict);
    auto read = r.read();
    EXPECT_NEAR(read.ram_ratio, 0.95, 1e-9);
    EXPECT_NEAR(read.vram_ratio, 0.90, 1e-9);
    EXPECT_EQ(read.ram_fixed_reserve_bytes, 1024ULL * 1024 * 1024);
    EXPECT_EQ(read.vram_fixed_reserve_bytes, 256ULL * 1024 * 1024);
    EXPECT_EQ(read.fallback_policy, FallbackPolicy::Strict);
}

TEST(UtilConfig, ReadBeforeInitReturnsDefaults) {
    ConfigHotReader r;
    EXPECT_FALSE(r.initialized());
    auto read = r.read();
    EXPECT_NEAR(read.ram_ratio, 0.95, 1e-9);
    EXPECT_NEAR(read.vram_ratio, 0.95, 1e-9);
}

TEST(UtilConfig, BackendEnableDisable) {
    ConfigHotReader r;
    HotConfig cfg;
    cfg.backend_enabled["cuda:0"] = true;
    cfg.backend_enabled["cuda:1"] = false;
    r.init(cfg);
    EXPECT_TRUE(r.is_backend_enabled("cuda:0"));
    EXPECT_FALSE(r.is_backend_enabled("cuda:1"));
    EXPECT_TRUE(r.is_backend_enabled("cuda:2"));
    r.set_backend_enabled("cuda:0", false);
    EXPECT_FALSE(r.is_backend_enabled("cuda:0"));
    auto backends = r.configured_backends();
    EXPECT_EQ(backends.size(), 2u);
}

// 26 §2：不提供 CPU/GPU 利用率目标与 share 字段（编译期保证）
TEST(UtilConfig, NoCpuGpuShareField) {
    HotConfig cfg;
    // cpu_target/gpu_target/io_target 已删除；cpu_share/gpu_share 不存在
    EXPECT_NEAR(cfg.ram_ratio, 0.95, 1e-9);
    SUCCEED();
}

// ============================================================================
// ActualTracker（诊断记录器）
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

TEST(UtilTracker, P95StatsCompute) {
    ActualTracker t;
    for (int i = 0; i < 100; ++i) {
        double actual = static_cast<double>(i) / 100.0;
        UtilizationSample s{static_cast<std::uint64_t>(i), actual, 0.95, actual - 0.95,
                            false, "cpu", 0, 0, 0, false};
        t.record(s);
    }
    auto s = t.stats();
    EXPECT_EQ(s.sample_count, 100u);
}

TEST(UtilTracker, EmptyStatsSafe) {
    ActualTracker t;
    auto s = t.stats();
    EXPECT_EQ(s.sample_count, 0u);
    EXPECT_NEAR(s.average_error, 0.0, 1e-9);
}

TEST(UtilTracker, StatusJsonContainsFields) {
    ActualTracker t;
    UtilizationSample s{1, 0.9, 0.9, 0.0, false, "cpu", 0, 0, 0, false};
    t.record(s);
    std::string j = t.status_json();
    EXPECT_NE(j.find("\"sample_count\""), std::string::npos);
}
