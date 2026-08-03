// lib/acr/tests/unit/test_scheduler.cpp — Phase F scheduler 单元测试
// 覆盖：
//   - CoverageBitmap：mark_done / all_done / pending_indices
//   - partition_range：不重叠 + 完整覆盖
//   - partition_range_into：均分
//   - partition_tiles：tile 覆盖完整
//   - partition_tiles_into：max_chunks 限制
//   - QueueAwareEstimator：finish 估算 + pick_best_device + should_prefer_cpu
//   - ReductionMerger：局部合并 + finalize
//   - FallbackPolicy：ToCpu / ToNextDevice / None
//   - MixedRunner：coverage 完整不重复
//   - Dispatcher：dispatch_range + pick_backend + handle_failure
#include <gtest/gtest.h>

#include "dispatcher.hpp"
#include "fallback.hpp"
#include "mixed_runner.hpp"
#include "partitioner.hpp"
#include "queue_aware.hpp"
#include "reduction_merger.hpp"

#include "../core/task_descriptor.hpp"
#include "../cost/cost_estimator.hpp"

#include <algorithm>
#include <atomic>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute::scheduler;

// ============================================================================
// CoverageBitmap
// ============================================================================

TEST(SchedulerCoverage, InitialAllPending) {
    CoverageBitmap bm(10);
    EXPECT_EQ(bm.chunk_count(), 10u);
    EXPECT_EQ(bm.done_count(), 0u);
    EXPECT_FALSE(bm.all_done());
    EXPECT_EQ(bm.pending_indices().size(), 10u);
}

TEST(SchedulerCoverage, MarkDoneIncrementsCount) {
    CoverageBitmap bm(10);
    bm.mark_done(2);
    EXPECT_EQ(bm.done_count(), 1u);
    EXPECT_TRUE(bm.is_done(2));
    EXPECT_FALSE(bm.is_done(3));
    bm.mark_done(2);  // 重复标记不增加
    EXPECT_EQ(bm.done_count(), 1u);
    bm.mark_done(5);
    EXPECT_EQ(bm.done_count(), 2u);
}

TEST(SchedulerCoverage, AllDoneAfterAllMarked) {
    CoverageBitmap bm(5);
    for (std::size_t i = 0; i < 5; ++i) bm.mark_done(i);
    EXPECT_TRUE(bm.all_done());
    EXPECT_TRUE(bm.pending_indices().empty());
}

TEST(SchedulerCoverage, PendingIndicesCorrect) {
    CoverageBitmap bm(8);
    bm.mark_done(1);
    bm.mark_done(4);
    auto pending = bm.pending_indices();
    std::vector<std::size_t> expected = {0, 2, 3, 5, 6, 7};
    EXPECT_EQ(pending, expected);
}

TEST(SchedulerCoverage, OutOfRangeSafe) {
    CoverageBitmap bm(4);
    EXPECT_FALSE(bm.is_done(100));
    bm.mark_done(100);  // 不崩溃
    EXPECT_EQ(bm.done_count(), 0u);
}

// ============================================================================
// partition_range
// ============================================================================

TEST(SchedulerPartition, RangeNoOverlapComplete) {
    auto chunks = partition_range(0, 100, 30);
    EXPECT_EQ(chunks.size(), 4u);
    // 不重叠
    for (std::size_t i = 1; i < chunks.size(); ++i) {
        EXPECT_EQ(chunks[i].begin, chunks[i-1].end);
    }
    // 完整覆盖
    EXPECT_EQ(chunks.front().begin, 0u);
    EXPECT_EQ(chunks.back().end, 100u);
    // 索引顺序
    for (std::size_t i = 0; i < chunks.size(); ++i) {
        EXPECT_EQ(chunks[i].index, i);
    }
}

TEST(SchedulerPartition, RangeLastChunkClamped) {
    auto chunks = partition_range(0, 100, 30);
    EXPECT_EQ(chunks.back().begin, 90u);
    EXPECT_EQ(chunks.back().end, 100u);  // 10 个元素，不是 30
}

TEST(SchedulerPartition, RangeIntoEvenSplit) {
    auto chunks = partition_range_into(0, 100, 4);
    EXPECT_EQ(chunks.size(), 4u);
    for (std::size_t i = 1; i < chunks.size(); ++i) {
        EXPECT_EQ(chunks[i].begin, chunks[i-1].end);
    }
    EXPECT_EQ(chunks.front().begin, 0u);
    EXPECT_EQ(chunks.back().end, 100u);
    // 每个 25
    for (const auto& c : chunks) {
        EXPECT_EQ(c.end - c.begin, 25u);
    }
}

TEST(SchedulerPartition, RangeIntoUnevenSplit) {
    auto chunks = partition_range_into(0, 100, 3);
    EXPECT_EQ(chunks.size(), 3u);
    EXPECT_EQ(chunks.front().begin, 0u);
    EXPECT_EQ(chunks.back().end, 100u);
    // 100 / 3 = 33 rem 1，前 1 个 34，后 2 个 33
    EXPECT_EQ(chunks[0].end - chunks[0].begin, 34u);
    EXPECT_EQ(chunks[1].end - chunks[1].begin, 33u);
    EXPECT_EQ(chunks[2].end - chunks[2].begin, 33u);
}

// ============================================================================
// partition_tiles
// ============================================================================

TEST(SchedulerPartition, TilesCoverAll) {
    auto tiles = partition_tiles(10, 10, 4, 4);
    // 3x3 = 9 tiles
    EXPECT_EQ(tiles.size(), 9u);
    std::vector<int> covered(100, 0);
    for (const auto& t : tiles) {
        for (std::size_t y = t.tile_y * 4; y < t.tile_y * 4 + t.tile_h; ++y) {
            for (std::size_t x = t.tile_x * 4; x < t.tile_x * 4 + t.tile_w; ++x) {
                ASSERT_LT(y * 10 + x, 100u);
                covered[y * 10 + x] = 1;
            }
        }
    }
    int total = std::accumulate(covered.begin(), covered.end(), 0);
    EXPECT_EQ(total, 100);
}

TEST(SchedulerPartition, TilesIntoMaxChunks) {
    auto tiles = partition_tiles_into(100, 100, 9);
    EXPECT_LE(tiles.size(), 9u);
    EXPECT_GE(tiles.size(), 1u);
}

TEST(SchedulerPartition, RangeEmpty) {
    auto chunks = partition_range(0, 0, 10);
    EXPECT_TRUE(chunks.empty());
    auto chunks2 = partition_range(10, 5, 10);
    EXPECT_TRUE(chunks2.empty());
}

TEST(SchedulerPartition, RangeZeroChunkSize) {
    auto chunks = partition_range(0, 100, 0);
    EXPECT_TRUE(chunks.empty());
}

// ============================================================================
// QueueAwareEstimator
// ============================================================================

TEST(SchedulerQueueAware, EstimateFinishCpuNoTransfer) {
    QueueAwareEstimator est;
    DeviceState cpu{"cpu", 0, 0, 50.0, true};
    TaskEstimate task{1024, 10, 1000.0, 100.0};
    std::uint64_t f = est.estimate_finish(cpu, task);
    // CPU 无传输：finish = 0 + 0 + 10000 + 1000 = 11000
    EXPECT_EQ(f, 11000u);
}

TEST(SchedulerQueueAware, EstimateFinishGpuWithTransfer) {
    QueueAwareEstimator est;
    DeviceState gpu{"cuda:0", 1000, 0, 10.0, true};  // 10 GB/s PCIe
    TaskEstimate task{1024 * 1024, 10, 1000.0, 100.0};  // 1MB per chunk
    std::uint64_t f = est.estimate_finish(gpu, task);
    // 传输 = 10MB / 10GB/s = 1ms = 1e6 ns
    // 计算 = 10000 ns，合并 = 1000 ns
    // 总 = 1000 + 1e6 + 10000 + 1000
    EXPECT_GT(f, 1000u);
    EXPECT_GE(f, 1000000u);
}

TEST(SchedulerQueueAware, PickBestDeviceSelectsLowerFinish) {
    QueueAwareEstimator est;
    std::vector<DeviceState> devs = {
        {"cpu", 0, 0, 50.0, true},
        {"cuda:0", 100000, 0, 10.0, true},  // 高 queue_load
    };
    TaskEstimate task{1024, 10, 1000.0, 100.0};
    std::string best = est.pick_best_device(devs, task);
    // CPU finish = 11000，GPU finish = 100000+传输+计算
    // CPU 应该更短
    EXPECT_EQ(best, "cpu");
}

TEST(SchedulerQueueAware, PickBestDeviceSkipsUnavailable) {
    QueueAwareEstimator est;
    std::vector<DeviceState> devs = {
        {"cpu", 0, 0, 50.0, false},  // 不可用
        {"cuda:0", 1000, 0, 10.0, true},
    };
    TaskEstimate task{1024, 10, 1000.0, 100.0};
    std::string best = est.pick_best_device(devs, task);
    EXPECT_EQ(best, "cuda:0");
}

TEST(SchedulerQueueAware, ShouldPreferCpuForSmallData) {
    QueueAwareEstimator est;
    DeviceState cpu{"cpu", 0, 0, 50.0, true};
    DeviceState gpu{"cuda:0", 0, 0, 1.0, true};  // 慢 PCIe
    // 小数据：bytes=1024, compute=1000000ns → 传输 < 计算*0.5
    TaskEstimate small{1024, 1, 1000000.0, 0.0};
    // 大数据：bytes=1e9, compute=1000ns → 传输 > 计算*0.5
    TaskEstimate large{1000000000ULL, 1, 1000.0, 0.0};
    EXPECT_FALSE(est.should_prefer_cpu(cpu, gpu, small));  // 小数据传输不主导
    EXPECT_TRUE(est.should_prefer_cpu(cpu, gpu, large));   // 大数据传输主导
}

// ============================================================================
// ReductionMerger
// ============================================================================

TEST(SchedulerMerger, InitAndFinalize) {
    ReductionMerger m;
    int identity = 0;
    auto merge = +[](void* dst, const void* src) {
        *static_cast<int*>(dst) += *static_cast<const int*>(src);
    };
    m.init(&identity, sizeof(int), merge);
    int a = 10, b = 20, c = 30;
    m.add_local(&a);
    m.add_local(&b);
    m.add_local(&c);
    EXPECT_EQ(m.local_count(), 3u);
    int result = 0;
    m.finalize(&result);
    EXPECT_EQ(result, 60);
}

TEST(SchedulerMerger, FinalizeWithNoLocalsReturnsIdentity) {
    ReductionMerger m;
    int identity = 42;
    auto merge = +[](void* dst, const void* src) {
        *static_cast<int*>(dst) += *static_cast<const int*>(src);
    };
    m.init(&identity, sizeof(int), merge);
    int result = -1;
    m.finalize(&result);
    EXPECT_EQ(result, 42);
}

TEST(SchedulerMerger, MultipleFinalizeStable) {
    ReductionMerger m;
    int identity = 0;
    auto merge = +[](void* dst, const void* src) {
        *static_cast<int*>(dst) += *static_cast<const int*>(src);
    };
    m.init(&identity, sizeof(int), merge);
    int a = 5, b = 7;
    m.add_local(&a);
    m.add_local(&b);
    int r1 = 0, r2 = 0;
    m.finalize(&r1);
    m.finalize(&r2);
    EXPECT_EQ(r1, 12);
    EXPECT_EQ(r2, 12);
}

// ============================================================================
// FallbackPolicy
// ============================================================================

TEST(SchedulerFallback, ToCpuStrategy) {
    FallbackPolicy p;
    p.set_strategy(FallbackStrategy::ToCpu);
    CoverageBitmap bm(10);
    bm.mark_done(0);
    bm.mark_done(1);
    auto d = p.decide("cuda:0", bm, {"cpu", "cuda:1"});
    EXPECT_EQ(d.strategy, FallbackStrategy::ToCpu);
    EXPECT_EQ(d.target_backend, "cpu");
    EXPECT_TRUE(d.skip_already_done);
    // 未完成 chunk 8 个（2,3,...,9）
    EXPECT_EQ(d.pending_chunks.size(), 8u);
}

TEST(SchedulerFallback, ToNextDeviceStrategy) {
    FallbackPolicy p;
    p.set_strategy(FallbackStrategy::ToNextDevice);
    CoverageBitmap bm(5);
    auto d = p.decide("cuda:0", bm, {"cpu", "cuda:1"});
    EXPECT_EQ(d.strategy, FallbackStrategy::ToNextDevice);
    EXPECT_EQ(d.target_backend, "cuda:1");
}

TEST(SchedulerFallback, NoneStrategyNoTarget) {
    FallbackPolicy p;
    p.set_strategy(FallbackStrategy::None);
    CoverageBitmap bm(5);
    auto d = p.decide("cuda:0", bm, {"cpu"});
    EXPECT_EQ(d.strategy, FallbackStrategy::None);
    EXPECT_TRUE(d.target_backend.empty());
}

TEST(SchedulerFallback, ToNextDeviceFallbackToCpuWhenNoOtherGpu) {
    FallbackPolicy p;
    p.set_strategy(FallbackStrategy::ToNextDevice);
    CoverageBitmap bm(5);
    auto d = p.decide("cuda:0", bm, {"cpu"});
    // 只有 cpu 可用，ToNextDevice 退化为 ToCpu
    EXPECT_EQ(d.strategy, FallbackStrategy::ToCpu);
    EXPECT_EQ(d.target_backend, "cpu");
}

// ============================================================================
// MixedRunner
// ============================================================================

TEST(SchedulerMixedRunner, CoverageCompleteNoRepeat) {
    astro::compute::runtime_init();
    MixedRunner runner;
    MixedRunnerConfig cfg;
    runner.configure(cfg);

    struct UD { std::vector<int>* data; std::atomic<int>* count; };
    std::vector<int> data(1000, 0);
    std::atomic<int> call_count{0};
    UD ud{&data, &call_count};
    auto real_fn = +[](std::size_t, std::size_t b, std::size_t e, void* p) {
        UD* u = static_cast<UD*>(p);
        for (std::size_t i = b; i < e; ++i) (*u->data)[i] = 1;
        u->count->fetch_add(1, std::memory_order_relaxed);
    };
    auto r = runner.run_range(0, 1000, 100, real_fn, &ud);
    EXPECT_TRUE(r.all_done);
    EXPECT_EQ(r.total_chunks, 10u);
    EXPECT_EQ(r.executed_on_cpu, 10u);
    EXPECT_EQ(r.failed_chunks, 0u);
    EXPECT_EQ(call_count.load(), 10);

    // coverage 完整不重复
    int total = std::accumulate(data.begin(), data.end(), 0);
    EXPECT_EQ(total, 1000);
    const auto& bm = runner.last_coverage();
    EXPECT_EQ(bm.done_count(), 10u);
    EXPECT_TRUE(bm.all_done());

    astro::compute::runtime_shutdown();
}

TEST(SchedulerMixedRunner, EmptyRangeReturnsSuccess) {
    astro::compute::runtime_init();
    MixedRunner runner;
    auto r = runner.run_range(0, 0, 100,
        +[](std::size_t, std::size_t, std::size_t, void*) {}, nullptr);
    EXPECT_TRUE(r.all_done);
    EXPECT_EQ(r.total_chunks, 0u);
    astro::compute::runtime_shutdown();
}

TEST(SchedulerMixedRunner, ExceptionInChunkCountsAsFailed) {
    astro::compute::runtime_init();
    MixedRunner runner;
    MixedRunnerConfig cfg;
    runner.configure(cfg);
    std::vector<int> data(100, 0);
    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        std::vector<int>* d = static_cast<std::vector<int>*>(ud);
        if (b == 0) throw std::runtime_error("chunk 0 failed");
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };
    auto r = runner.run_range(0, 100, 50, fn, &data);
    EXPECT_FALSE(r.all_done);
    EXPECT_EQ(r.failed_chunks, 1u);
    EXPECT_EQ(r.executed_on_cpu, 1u);  // 第二个 chunk 成功
    astro::compute::runtime_shutdown();
}

// ============================================================================
// Dispatcher
// ============================================================================

TEST(SchedulerDispatcher, DispatchRangeExecutesAll) {
    astro::compute::runtime_init();
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    d.configure(cfg);

    std::vector<int> data(100, 0);
    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        std::vector<int>* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };
    auto r = d.dispatch_range(0, 100, 25, fn, &data);
    EXPECT_TRUE(r.all_done);
    EXPECT_EQ(r.total_chunks, 4u);
    int total = std::accumulate(data.begin(), data.end(), 0);
    EXPECT_EQ(total, 100);
    astro::compute::runtime_shutdown();
}

TEST(SchedulerDispatcher, PickBackendSmallDataPrefersCpu) {
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.small_data_threshold_bytes = 1u << 20;  // 1MB
    cfg.devices = {
        {"cpu", 0, 0, 50.0, true},
        {"cuda:0", 0, 0, 100.0, true},  // 高带宽
    };
    d.configure(cfg);
    TaskEstimate small{1024, 1, 1000.0, 0.0};  // 1KB < 1MB → CPU
    EXPECT_EQ(d.pick_backend(small), "cpu");
}

TEST(SchedulerDispatcher, HandleFailureReturnsFallbackDecision) {
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.fallback_strategy = FallbackStrategy::ToCpu;
    cfg.devices = {
        {"cpu", 0, 0, 50.0, true},
        {"cuda:0", 0, 0, 100.0, true},
    };
    d.configure(cfg);
    CoverageBitmap bm(5);
    bm.mark_done(0);
    auto dec = d.handle_failure("cuda:0", bm);
    EXPECT_EQ(dec.strategy, FallbackStrategy::ToCpu);
    EXPECT_EQ(dec.target_backend, "cpu");
    EXPECT_EQ(dec.pending_chunks.size(), 4u);
}

// ============================================================================
// Dispatcher::dispatch_range_cost_aware (Commit F)
// ============================================================================

// 辅助：构造 CPU-only CostEstimate（profile_available=false → 纯 CPU 路径）
static astro::compute::cost::CostEstimate make_cpu_only_estimate(std::size_t recommended_chunk) {
    astro::compute::cost::CostEstimate est;
    est.profile_available = false;
    astro::compute::cost::DeviceCost cpu_cost;
    cpu_cost.device_id = astro::compute::kCpuDeviceId;
    cpu_cost.backend = "cpu";
    cpu_cost.feasible = true;
    cpu_cost.recommended_chunk = recommended_chunk;
    cpu_cost.profile_available = false;
    est.per_device.push_back(cpu_cost);
    est.preferred_device = astro::compute::kCpuDeviceId;
    return est;
}

TEST(SchedulerDispatcherCostAware, CpuOnlyExecutesAll) {
    astro::compute::runtime_init();
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    cfg.enable_utilization = false;  // 禁用 utilization 以走简单路径
    cfg.enable_fixed_tail_chunking = false;
    d.configure(cfg);

    astro::compute::TaskDescriptor task;
    task.range = astro::compute::Range1D{0, 100};

    auto est = make_cpu_only_estimate(25);
    std::vector<int> data(100, 0);
    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        auto* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };
    auto r = d.dispatch_range_cost_aware(task, est, fn, &data);
    EXPECT_TRUE(r.run_result.all_done);
    EXPECT_EQ(r.actual_primary_backend, "cpu");
    EXPECT_GT(r.total_chunks, 0u);
    int total = std::accumulate(data.begin(), data.end(), 0);
    EXPECT_EQ(total, 100);
    astro::compute::runtime_shutdown();
}

TEST(SchedulerDispatcherCostAware, FixedTailChunkingSplitsRange) {
    astro::compute::runtime_init();
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    cfg.enable_utilization = true;
    cfg.enable_fixed_tail_chunking = true;
    cfg.fixed_tail_threshold = 0.7;
    cfg.min_effective_chunk = 256;
    d.configure(cfg);

    // 范围需 > min_effective_chunk * 4 = 1024 才触发分段
    astro::compute::TaskDescriptor task;
    task.range = astro::compute::Range1D{0, 10000};

    auto est = make_cpu_only_estimate(500);
    std::vector<int> data(10000, 0);
    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        auto* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };
    auto r = d.dispatch_range_cost_aware(task, est, fn, &data);
    EXPECT_TRUE(r.run_result.all_done);
    EXPECT_TRUE(r.fixed_tail_chunking_used);
    EXPECT_GE(r.fixed_tail_min_chunk, cfg.min_effective_chunk);
    // 完整覆盖
    int total = std::accumulate(data.begin(), data.end(), 0);
    EXPECT_EQ(total, 10000);
    astro::compute::runtime_shutdown();
}

TEST(SchedulerDispatcherCostAware, FixedTailChunkingDisabledNoSplit) {
    astro::compute::runtime_init();
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    cfg.enable_utilization = true;
    cfg.enable_fixed_tail_chunking = false;  // 禁用
    cfg.min_effective_chunk = 256;
    d.configure(cfg);

    astro::compute::TaskDescriptor task;
    task.range = astro::compute::Range1D{0, 10000};

    auto est = make_cpu_only_estimate(500);
    std::vector<int> data(10000, 0);
    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        auto* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };
    auto r = d.dispatch_range_cost_aware(task, est, fn, &data);
    EXPECT_TRUE(r.run_result.all_done);
    EXPECT_FALSE(r.fixed_tail_chunking_used);
    int total = std::accumulate(data.begin(), data.end(), 0);
    EXPECT_EQ(total, 10000);
    astro::compute::runtime_shutdown();
}

TEST(SchedulerDispatcherCostAware, MemoryActionPopulated) {
    astro::compute::runtime_init();
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    cfg.enable_utilization = true;
    cfg.enable_fixed_tail_chunking = true;
    d.configure(cfg);

    astro::compute::TaskDescriptor task;
    task.range = astro::compute::Range1D{0, 1000};

    auto est = make_cpu_only_estimate(100);
    std::vector<int> data(1000, 0);
    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        auto* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };
    auto r = d.dispatch_range_cost_aware(task, est, fn, &data);
    EXPECT_TRUE(r.run_result.all_done);
    // mem_action 应被填充（非空字符串）
    EXPECT_FALSE(r.mem_action.empty());
    // 正常内存条件下应为 "none"
    EXPECT_EQ(r.mem_action, "none");
    astro::compute::runtime_shutdown();
}

TEST(SchedulerDispatcherCostAware, CurrentStateJsonPopulated) {
    astro::compute::runtime_init();
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    cfg.enable_utilization = false;
    cfg.enable_fixed_tail_chunking = false;
    d.configure(cfg);

    astro::compute::TaskDescriptor task;
    task.range = astro::compute::Range1D{0, 500};

    auto est = make_cpu_only_estimate(100);
    std::vector<int> data(500, 0);
    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        auto* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };
    auto r = d.dispatch_range_cost_aware(task, est, fn, &data);
    EXPECT_TRUE(r.run_result.all_done);
    EXPECT_FALSE(r.current_state_json.empty());
    // JSON 应包含 "cpu" 设备
    EXPECT_NE(r.current_state_json.find("cpu"), std::string::npos);
    astro::compute::runtime_shutdown();
}

// ============================================================================
// F-fix 1: actual vs predicted backend + coverage 真实导入
// ============================================================================

TEST(SchedulerDispatcherCostAware, PredictedVsActualBackend) {
    // 验证 predicted_primary_backend 与 actual_primary_backend 分别报告
    // profile_available=false → predicted 应为 cpu（fallback）, actual 也为 cpu
    astro::compute::runtime_init();
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    cfg.enable_utilization = false;
    cfg.enable_fixed_tail_chunking = false;
    d.configure(cfg);

    astro::compute::TaskDescriptor task;
    task.range = astro::compute::Range1D{0, 200};

    auto est = make_cpu_only_estimate(50);
    std::vector<int> data(200, 0);
    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        auto* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };
    auto r = d.dispatch_range_cost_aware(task, est, fn, &data);
    EXPECT_TRUE(r.run_result.all_done);
    // predicted 和 actual 都应为 cpu（无 GPU）
    EXPECT_FALSE(r.predicted_primary_backend.empty());
    EXPECT_FALSE(r.actual_primary_backend.empty());
    EXPECT_EQ(r.actual_primary_backend, "cpu");
    // 实际设备列表应包含 cpu
    bool has_cpu = false;
    for (const auto& dev : r.actual_devices_used) {
        if (dev == "cpu") has_cpu = true;
    }
    EXPECT_TRUE(has_cpu);
    astro::compute::runtime_shutdown();
}

TEST(SchedulerDispatcherCostAware, CoverageFromRealExecution) {
    // 验证 coverage 从真实执行导入，不是无条件 mark_done
    astro::compute::runtime_init();
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    cfg.enable_utilization = false;
    cfg.enable_fixed_tail_chunking = false;
    d.configure(cfg);

    astro::compute::TaskDescriptor task;
    task.range = astro::compute::Range1D{0, 100};

    auto est = make_cpu_only_estimate(25);
    std::vector<int> data(100, 0);
    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        auto* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };
    auto r = d.dispatch_range_cost_aware(task, est, fn, &data);
    EXPECT_TRUE(r.run_result.all_done);
    // coverage 统计应匹配
    EXPECT_EQ(r.coverage.total, r.total_chunks);
    EXPECT_EQ(r.coverage.done, r.total_chunks);
    EXPECT_EQ(r.coverage.failed, 0u);
    EXPECT_EQ(r.coverage.pending, 0u);
    astro::compute::runtime_shutdown();
}

TEST(SchedulerDispatcherCostAware, CoverageReflectsFailures) {
    // 验证失败的 chunk 不被标为 done
    astro::compute::runtime_init();
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    cfg.enable_utilization = false;
    cfg.enable_fixed_tail_chunking = false;
    d.configure(cfg);

    astro::compute::TaskDescriptor task;
    task.range = astro::compute::Range1D{0, 100};

    auto est = make_cpu_only_estimate(25);
    // kernel：前 50 个元素正常，后 50 个抛异常
    std::vector<int> data(100, 0);
    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        auto* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) {
            if (i >= 50) throw std::runtime_error("intentional failure");
            (*d)[i] = 1;
        }
    };
    auto r = d.dispatch_range_cost_aware(task, est, fn, &data);
    // 不应全部完成（有失败块）
    EXPECT_FALSE(r.run_result.all_done);
    // coverage 应反映失败
    EXPECT_GT(r.coverage.failed, 0u);
    EXPECT_LT(r.coverage.done, r.coverage.total);
    // actual_primary_backend 仍为 cpu（有成功的块在 cpu）
    EXPECT_EQ(r.actual_primary_backend, "cpu");
    astro::compute::runtime_shutdown();
}

TEST(SchedulerDispatcherCostAware, ActualBackendNoneWhenAllFail) {
    // 全部失败时 actual_primary_backend 应为 "none"
    astro::compute::runtime_init();
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    cfg.enable_utilization = false;
    cfg.enable_fixed_tail_chunking = false;
    d.configure(cfg);

    astro::compute::TaskDescriptor task;
    task.range = astro::compute::Range1D{0, 50};

    auto est = make_cpu_only_estimate(25);
    auto fn = +[](std::size_t, std::size_t, std::size_t, void*) -> void {
        throw std::runtime_error("all fail");
    };
    auto r = d.dispatch_range_cost_aware(task, est, fn, nullptr);
    EXPECT_FALSE(r.run_result.all_done);
    EXPECT_GT(r.coverage.failed, 0u);
    EXPECT_EQ(r.coverage.done, 0u);
    // 无成功块 → actual 为 none
    EXPECT_EQ(r.actual_primary_backend, "none");
    EXPECT_TRUE(r.actual_devices_used.empty());
    astro::compute::runtime_shutdown();
}

// ============================================================================
// main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
