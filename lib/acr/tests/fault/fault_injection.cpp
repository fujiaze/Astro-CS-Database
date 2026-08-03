// lib/acr/tests/fault/fault_injection.cpp — Phase H 故障注入测试
// 测试场景：
//   1. 取消正在执行的 kernel
//   2. kernel 异常传播（throw → mark_failed）
//   3. 混合调度 fallback（设备失败 → CPU 回退）
//   4. 异常 chunk 在混合调度中被计为 failed
#include <gtest/gtest.h>

#include <dispatcher.hpp>
#include <fallback.hpp>
#include <mixed_runner.hpp>
#include <partitioner.hpp>

#include <atomic>
#include <cstdio>
#include <string>
#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::scheduler;

// ============================================================================
// 1. 取消正在执行的 kernel
// ============================================================================

TEST(FaultInjection, CancelRunningKernel) {
    std::atomic<int> cnt{0};
    Event ev = parallel_for(KernelId::Custom, Range1D{0, 1000},
        [&cnt](std::size_t) { cnt.fetch_add(1, std::memory_order_relaxed); });
    ev.wait();
    ev.cancel();
    EXPECT_TRUE(ev.cancelled());
    EXPECT_EQ(cnt.load(), 1000);
    EXPECT_EQ(ev.status(), StatusCode::Ok);
}

// ============================================================================
// 2. kernel 异常传播：throw → mark_failed(KernelFailed)
// ============================================================================

TEST(FaultInjection, KernelExceptionPropagatesAsFailed) {
    Event ev = parallel_for(KernelId::Custom, Range1D{0, 100},
        [](std::size_t) { throw std::runtime_error("boom"); });
    EXPECT_EQ(ev.status(), StatusCode::KernelFailed);
    EXPECT_TRUE(ev.ready());
}

// ============================================================================
// 3. 混合调度 fallback：设备失败 → CPU 回退
// ============================================================================

TEST(FaultInjection, MixedScheduleFallbackToCpu) {
    runtime_init();
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.fallback_strategy = FallbackStrategy::ToCpu;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    d.configure(cfg);

    std::vector<int> data(100, 0);
    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        std::vector<int>* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };
    auto r = d.dispatch_range(0, 100, 25, fn, &data);
    EXPECT_TRUE(r.all_done);
    EXPECT_EQ(r.failed_chunks, 0u);
    int sum = 0;
    for (auto v : data) sum += v;
    EXPECT_EQ(sum, 100);
    runtime_shutdown();
}

// ============================================================================
// 4. 异常 chunk 在混合调度中被计为 failed
// ============================================================================

TEST(FaultInjection, ExceptionChunkCountedAsFailed) {
    runtime_init();
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
    runtime_shutdown();
}

// ============================================================================
// main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    astro::compute::runtime_init();
    int result = RUN_ALL_TESTS();
    astro::compute::runtime_shutdown();
    return result;
}
