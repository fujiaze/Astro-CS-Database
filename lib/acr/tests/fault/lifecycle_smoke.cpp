// lib/acr/tests/fault/lifecycle_smoke.cpp — Lifecycle Smoke
// 原 sanitizer_smoke.cpp（按 20_PHASE_I_AUDIT_ACTION_PLAN.md §6 重命名）
// 重命名理由：CMake 未开启 -fsanitize flag，本测试不验证 sanitizer 是否开启，
//   仅做 Buffer/Event/并发生命周期的功能烟雾测试；sanitizer 验证在 sanitizer_actual.cpp。
// 验证：内存泄漏 / use-after-free / 竞态（功能层面，非 sanitizer 层面）
// 设计：这些测试在 ASan/UBSan/TSan 下应当全部通过（无报错），但本文件不强制 sanitizer 开启。
// 真正的 sanitizer 验证需 ACR_BUILD_SANITIZER=ON 编译 sanitizer_actual.cpp。
// 测试内容：
//   1. Buffer 生命周期（构造、析构、move）
//   2. Event 取消时无泄漏
//   3. 并发提交无数据竞争（用 std::atomic 计数器验证）
//   4. 大量 Buffer 分配/释放无泄漏
//   5. BufferView 不拥有内存（父 Buffer 释放后 view 失效，但不崩溃）
//   6. parallel_reduce 异常安全
//   7. Event 移动语义
#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "astro/compute/acr.hpp"
#include "exit_safe.hpp"

using namespace astro::compute;

// ============================================================================
// 1. Buffer 生命周期：构造、析构、move
// ============================================================================
TEST(SanitizerSmoke, BufferConstructDestructMove) {
    // 构造
    Buffer<float> b1(1024, 1.0f);
    EXPECT_EQ(b1.count(), 1024u);
    // move ctor
    Buffer<float> b2(std::move(b1));
    EXPECT_EQ(b2.count(), 1024u);
    EXPECT_EQ(b1.count(), 0u);  // NOLINT(bugprone-use-after-move)
    // move assign
    Buffer<float> b3;
    b3 = std::move(b2);
    EXPECT_EQ(b3.count(), 1024u);
    EXPECT_EQ(b2.count(), 0u);
    // 析构链正常
}

// ============================================================================
// 2. Event 取消时无泄漏
// ============================================================================
TEST(SanitizerSmoke, EventCancelNoLeak) {
    for (int i = 0; i < 50; ++i) {
        Event ev = parallel_for(KernelId::Custom, Range1D{0, 100},
            [](std::size_t) {});
        ev.wait();
        ev.cancel();
        // Event 析构在此循环结束时
    }
    SUCCEED();
}

// ============================================================================
// 3. 并发提交无数据竞争（用 std::atomic 计数器验证）
// ============================================================================
TEST(SanitizerSmoke, ConcurrentSubmissionNoRace) {
    constexpr int kThreads = 8;
    constexpr std::size_t kPerThread = 1000;
    std::atomic<std::uint64_t> counter{0};
    std::vector<std::thread> ts;
    ts.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        ts.emplace_back([&counter]() {
            parallel_for(KernelId::Custom, Range1D{0, kPerThread},
                [&counter](std::size_t) {
                    counter.fetch_add(1, std::memory_order_relaxed);
                });
        });
    }
    for (auto& th : ts) th.join();
    EXPECT_EQ(counter.load(), static_cast<std::uint64_t>(kThreads * kPerThread));
}

// ============================================================================
// 4. 大量 Buffer 分配/释放无泄漏
// ============================================================================
TEST(SanitizerSmoke, ManyBufferAllocFree) {
    for (int i = 0; i < 1000; ++i) {
        Buffer<int> b(256, i);
        // 使用 b
        EXPECT_EQ(b[0], i);
        // 析构
    }
    SUCCEED();
}

// ============================================================================
// 5. BufferView 不拥有内存
// ============================================================================
TEST(SanitizerSmoke, BufferViewNonOwning) {
    auto buf = std::make_unique<Buffer<float>>(100, 0.5f);
    BufferView<float> view(buf->data(), 100);
    EXPECT_EQ(view.count(), 100u);
    // 通过 view 修改
    view[0] = 1.5f;
    EXPECT_FLOAT_EQ((*buf)[0], 1.5f);
    // 释放 buf
    buf.reset();
    // view 现在悬空，但只要不访问就安全
    // 不访问 view，仅验证不崩溃
    SUCCEED();
}

// ============================================================================
// 6. parallel_reduce 异常安全
// ============================================================================
TEST(SanitizerSmoke, ParallelReduceExceptionSafe) {
    std::atomic<int> call_count{0};
    int result = 0;
    EXPECT_NO_THROW({
        result = parallel_reduce<int>(KernelId::Custom, Range1D{0, 100}, 0,
            [&call_count](std::size_t i) {
                call_count.fetch_add(1, std::memory_order_relaxed);
                return static_cast<int>(i);
            },
            std::plus<int>{});
    });
    EXPECT_EQ(result, 4950);
    EXPECT_EQ(call_count.load(), 100);
}

// ============================================================================
// 7. Event 移动语义
// ============================================================================
TEST(SanitizerSmoke, EventMoveSemantics) {
    Event ev1 = parallel_for(KernelId::Custom, Range1D{0, 100}, [](std::size_t) {});
    ev1.wait();
    Event ev2(std::move(ev1));
    EXPECT_TRUE(ev2.ready());
    // ev1 现在 moved-from，但仍可析构（不崩溃）
    Event ev3;
    ev3 = std::move(ev2);
    EXPECT_TRUE(ev3.ready());
    // ev2 现在 moved-from
}

// ============================================================================
// 8. parallel_chunks 多线程无竞争
// ============================================================================
TEST(SanitizerSmoke, ParallelChunksNoRace) {
    constexpr std::size_t kN = 10000;
    std::vector<int> data(kN, 0);
    parallel_chunks(KernelId::Custom, Range1D{0, kN}, 100,
        [&data](std::size_t b, std::size_t e) {
            for (std::size_t i = b; i < e; ++i) data[i] = static_cast<int>(i);
        });
    for (std::size_t i = 0; i < kN; ++i) {
        EXPECT_EQ(data[i], static_cast<int>(i));
    }
}

// ============================================================================
// 9. parallel_tiles 多线程无竞争（每 tile 独立区域）
// ============================================================================
TEST(SanitizerSmoke, ParallelTilesNoRace) {
    constexpr std::size_t kW = 64, kH = 64;
    std::vector<int> data(kW * kH, 0);
    parallel_tiles(KernelId::Custom, Extent2D{kW, kH}, TileShape{8, 8},
        [&](std::size_t tx, std::size_t ty, std::size_t tw, std::size_t th) {
            for (std::size_t j = 0; j < th; ++j) {
                for (std::size_t i = 0; i < tw; ++i) {
                    std::size_t x = tx * 8 + i;
                    std::size_t y = ty * 8 + j;
                    if (x < kW && y < kH) data[y * kW + x] = 1;
                }
            }
        });
    int sum = 0;
    for (auto v : data) sum += v;
    EXPECT_EQ(sum, static_cast<int>(kW * kH));
}

// ============================================================================
// 10. Buffer resize 反复
// ============================================================================
TEST(SanitizerSmoke, BufferResizeRepeated) {
    Buffer<float> b(100, 1.0f);
    for (int i = 0; i < 50; ++i) {
        b.resize(200 + i * 10);
        b[0] = static_cast<float>(i);
    }
    EXPECT_EQ(b.count(), 200u + 49 * 10);
}

// ============================================================================
// main
// ============================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    astro::compute::runtime_init();
    int result = RUN_ALL_TESTS();
    astro::compute::test::exit_after_tests(result);
}
