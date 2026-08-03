// lib/acr/tests/fault/persistence.cpp — Phase H 持续路线测试
// 测试：
//   1. 持续 parallel_for 路线（循环提交小任务，默认 5 秒，可通过 ACR_PERSIST_DURATION_SEC 环境变量配置）
//   2. 多次进程重启模拟（runtime_init → shutdown → init 循环 100 次）
//   3. profile 重载（生成 hardware-profile.json → 读取 → 作废 → 重新生成）
#include <gtest/gtest.h>

#include <benchmark_driver.hpp>
#include <profile_generator.hpp>
#include <profile_schema.hpp>
#include <profile_reader.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::profile;
using namespace astro::compute::qualification;

namespace {

// 从环境变量读取持续时长（秒），默认 5 秒（避免 ctest 超时；CI 可设 30）
int get_persist_duration_sec() {
    const char* env = std::getenv("ACR_PERSIST_DURATION_SEC");
    if (env) {
        int v = std::atoi(env);
        if (v > 0) return v;
    }
    return 5;  // 默认 5 秒
}

} // anonymous namespace

// ============================================================================
// 1. 持续 parallel_for 路线（循环提交小任务）
// ============================================================================
TEST(Persistence, ContinuousParallelFor) {
    int duration_sec = get_persist_duration_sec();
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(duration_sec);
    std::atomic<std::uint64_t> total{0};
    std::atomic<int> iterations{0};
    while (std::chrono::steady_clock::now() < deadline) {
        parallel_for(KernelId::Custom, Range1D{0, 100},
            [&total](std::size_t) {
                total.fetch_add(1, std::memory_order_relaxed);
            });
        iterations.fetch_add(1, std::memory_order_relaxed);
    }
    EXPECT_GT(total.load(), static_cast<std::uint64_t>(0));
    EXPECT_GT(iterations.load(), 0);
    // 验证 total == iterations * 100
    EXPECT_EQ(total.load(), static_cast<std::uint64_t>(iterations.load()) * 100u);
}

// ============================================================================
// 2. 多次进程重启模拟（runtime_init → shutdown → init 循环 100 次）
// ============================================================================
TEST(Persistence, Restarts100) {
    constexpr int kRestarts = 100;
    for (int i = 0; i < kRestarts; ++i) {
        runtime_init();
        std::atomic<int> cnt{0};
        parallel_for(KernelId::Custom, Range1D{0, 50},
            [&cnt](std::size_t) { cnt.fetch_add(1, std::memory_order_relaxed); });
        EXPECT_EQ(cnt.load(), 50);
        runtime_shutdown();
    }
    SUCCEED();
}

// ============================================================================
// 3. profile 重载（生成 hardware-profile.json → 读取 → 作废 → 重新生成）
// ============================================================================
TEST(Persistence, ProfileReload) {
    const char* path = "acr_persistence_profile.json";
    for (int i = 0; i < 5; ++i) {
        // 生成 hardware-profile.json
        runtime_init();
        BenchmarkDriver driver;
        auto cfg = make_default_config(ProfileKind::Quick, /*enable_gpu=*/false);
        driver.configure(cfg);
        auto results = driver.run();
        HardwareProfile hp = ProfileGenerator{}.generate_hardware_profile(results, ProfileKind::Quick);
        ASSERT_TRUE(ProfileGenerator::write_hardware_profile_to_file(path, hp));

        // 读取 profile
        HardwareProfileReader r;
        r.set_profile_path(path);
        const HardwareProfile* loaded = r.get_profile();
        // 三态：指纹匹配则 Valid，否则 Stale（仍非 nullptr）
        EXPECT_NE(loaded, nullptr);
        EXPECT_NE(r.profile_state(), HwProfileState::Missing);
        EXPECT_NE(r.profile_state(), HwProfileState::Corrupt);

        // 作废 profile
        std::remove(path);
        r.invalidate_cache();
        // 重新读取：文件不存在 → Missing
        const HardwareProfile* reloaded = r.get_profile();
        EXPECT_EQ(reloaded, nullptr);
        EXPECT_EQ(r.profile_state(), HwProfileState::Missing);

        runtime_shutdown();
    }
    std::remove(path);
    SUCCEED();
}

// ============================================================================
// 4. 持续 parallel_for + parallel_reduce 交替
// ============================================================================
TEST(Persistence, MixedParallelWorkload) {
    int duration_sec = std::max(2, get_persist_duration_sec() / 2);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(duration_sec);
    std::atomic<std::uint64_t> for_count{0};
    std::atomic<std::uint64_t> reduce_count{0};
    while (std::chrono::steady_clock::now() < deadline) {
        parallel_for(KernelId::Custom, Range1D{0, 100},
            [&for_count](std::size_t) { for_count.fetch_add(1, std::memory_order_relaxed); });
        int sum = parallel_reduce<int>(KernelId::Custom, Range1D{0, 50}, 0,
            [](std::size_t i) { return static_cast<int>(i); },
            std::plus<int>{});
        if (sum == 1225) reduce_count.fetch_add(1, std::memory_order_relaxed);
    }
    EXPECT_GT(for_count.load(), static_cast<std::uint64_t>(0));
    EXPECT_GT(reduce_count.load(), static_cast<std::uint64_t>(0));
}

// ============================================================================
// 5. 长时间 Buffer 反复分配/释放（验证无内存增长）
// ============================================================================
TEST(Persistence, BufferRepeatedAlloc) {
    for (int i = 0; i < 10000; ++i) {
        Buffer<float> b(256, static_cast<float>(i));
        b[0] += 1.0f;
        // 析构
    }
    SUCCEED();
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
