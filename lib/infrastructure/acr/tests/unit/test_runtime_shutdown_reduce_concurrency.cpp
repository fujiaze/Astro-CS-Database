// ============================================================================
// test_runtime_shutdown_reduce_concurrency.cpp
//   ACR 归约路径 vs runtime_shutdown 排空闸门并发回归 (M42-CONC-FIX-01 / 缺陷 A)
//
// 背景（缺陷）：submit_reduce / submit_reduce_with_desc 直接 s.arena->execute(...),
// 既未持 kernel_gate 的 shared_lock, 也不查 s.initialized / s.arena;
// 而 runtime_shutdown() 在 kernel_gate 的 unique 锁内 s.arena.reset()。
// 两条路径并发 ⇒ 空指针解引用 / arena UAF。
// 兄弟路径 arena_parallel_for 早已在同一闸门内 (shared_lock + initialized/arena 检查
// + 无 arena 降级全局并行域), 归约路径此前绕过它 —— 混合态零覆盖。
//
// 本文件提供的覆盖（真实并发窗口, 非串行）:
//   T1 ReduceKernelBlocksShutdownUntilArenaDrained
//      确定性红: reduce 的 map 阶段在 arena 内阻塞, 主线程在 kernel 在飞时调用
//      runtime_shutdown(); 断言 shutdown **必须** 阻塞到 kernel 离开 arena 才返回。
//      修复前 submit_reduce 不持 shared 锁 ⇒ shutdown 立刻拿到 unique 锁并
//      reset() 掉正在执行的 arena ⇒ 该断言确定失败（不是概率崩）。
//   T2 ReduceSurvivesConcurrentInitShutdownStress
//      多线程 reduce 与 init/shutdown 高频交错, 覆盖 initialized/arena 降级分支。
//   T3 ReduceNumericEquivalenceUnchangedByGate
//      证明闸门只改同步、不改数值: 结果与串行参考逐位相等, 且 1 worker == N worker。
// ============================================================================
#include "astro/compute/acr.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <thread>
#include <vector>

using namespace astro::compute;

namespace {

constexpr int kRangeN = 400000;

// 在 reduce 的 map 阶段制造"kernel 在 arena 内停留"的窗口。
struct GateHook {
    std::atomic<int> entered{0};       // 进入 map 的总次数
    std::atomic<int> in_flight{0};     // 当前卡在窗口内的 map 调用数
    std::atomic<bool> release{false};  // 闸门打开信号
    int block_first{0};                // 前 N 次调用进入窗口
};

// 1) 确定性排空回归
TEST(RuntimeShutdownReduceConcurrency, ReduceKernelBlocksShutdownUntilArenaDrained) {
    runtime_shutdown();
    RuntimeConfig cfg;
    cfg.max_threads = 4;
    cfg.arena_concurrency = 4;
    runtime_init(cfg);
    ASSERT_TRUE(runtime_initialized());

    GateHook hook;
    hook.block_first = 8;

    // reduce 的 map 阶段: 前 8 次调用在 arena 内驻留, 直到主线程放行。
    auto map = [&hook](std::size_t i) -> int {
        int n = hook.entered.fetch_add(1, std::memory_order_acq_rel);
        if (n < hook.block_first) {
            hook.in_flight.fetch_add(1, std::memory_order_acq_rel);
            while (!hook.release.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            hook.in_flight.fetch_sub(1, std::memory_order_acq_rel);
        }
        return static_cast<int>(i % 7);
    };
    auto op = [](int a, int b) { return a + b; };

    std::atomic<int> result{0};
    std::thread worker([&] {
        result.store(parallel_reduce<int>(KernelId::Custom, Range1D{0, kRangeN}, 0,
                                          map, op));
    });

    // 等到确实有 map 调用驻留在 arena 内（并发窗口已打开）。
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (hook.in_flight.load(std::memory_order_acquire) == 0 &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    ASSERT_GT(hook.in_flight.load(std::memory_order_acquire), 0)
        << "并发窗口未建立: reduce 未能在 arena 内驻留, 本用例失去意义";

    // 并发调用 shutdown: 正确实现必须被排空闸门挡住。
    std::atomic<bool> shutdown_started{false};
    std::atomic<bool> shutdown_done{false};
    std::thread killer([&] {
        shutdown_started.store(true, std::memory_order_release);
        runtime_shutdown();
        shutdown_done.store(true, std::memory_order_release);
    });
    while (!shutdown_started.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 核心断言: shutdown 不得在 kernel 仍在 arena 内时返回。
    EXPECT_FALSE(shutdown_done.load(std::memory_order_acquire))
        << "runtime_shutdown() 未被排空闸门挡住: 归约路径没持 kernel_gate shared 锁,"
        << "arena 在 kernel 在飞时被 reset() (UAF) —— 缺陷 A 未修";
    EXPECT_GT(hook.in_flight.load(std::memory_order_acquire), 0)
        << "断言窗口丢失: reduce 已提前完成";

    hook.release.store(true, std::memory_order_release);
    worker.join();
    killer.join();

    EXPECT_TRUE(shutdown_done.load(std::memory_order_acquire));
    EXPECT_FALSE(runtime_initialized());
    // 窗口注入只阻塞前 8 次 map 调用, 不改变被归约的值: 结果仍须等于解析值
    // sum_{i<n} (i % 7) = 完整 7 周期 (每周期和 21) + 余段。
    const long long full_cycles = kRangeN / 7;
    const long long tail = kRangeN % 7;
    long long expect = full_cycles * 21 + (tail * (tail - 1)) / 2;
    EXPECT_EQ(result.load(), expect)
        << "持闸门通过 shutdown 后, 归约结果仍须是解析值";
    // 清理, 避免影响同进程后续用例
    runtime_shutdown();
}

// 2) 降级分支: initialized/arena 为空时归约必须仍能完成, 不得空指针。
TEST(RuntimeShutdownReduceConcurrency, ReduceSurvivesConcurrentInitShutdownStress) {
    runtime_shutdown();
    RuntimeConfig cfg;
    cfg.max_threads = 2;
    cfg.arena_concurrency = 2;
    runtime_init(cfg);

    std::atomic<bool> stop{false};
    std::atomic<long long> completed{0};
    std::vector<std::thread> workers;
    for (int t = 0; t < 4; ++t) {
        workers.emplace_back([&] {
            while (!stop.load(std::memory_order_acquire)) {
                int s = parallel_reduce<int>(KernelId::Custom, Range1D{0, 20000}, 0,
                                            [](std::size_t i) { return static_cast<int>(i); },
                                            [](int a, int b) { return a + b; });
                if (s == 20000 * 19999 / 2) {
                    completed.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    // 主线程高频开关 runtime: 制造 submit 与 shutdown 的交错窗口。
    for (int i = 0; i < 400 && completed.load() == 0; ++i) {
        runtime_shutdown();
        runtime_init(cfg);
    }
    stop.store(true, std::memory_order_release);
    for (auto& w : workers) w.join();
    runtime_shutdown();

    EXPECT_GT(completed.load(), 0)
        << "交错窗口下没有一次归约取到正确结果（降级分支疑似失效/崩溃）";
    // 收尾: 重启后归约仍正确
    runtime_init(cfg);
    int s = parallel_reduce<int>(KernelId::Custom, Range1D{0, 20000}, 0,
                                 [](std::size_t i) { return static_cast<int>(i); },
                                 [](int a, int b) { return a + b; });
    EXPECT_EQ(s, 20000 * 19999 / 2);
    runtime_shutdown();
}

// 3) 等价性: 闸门只改同步, 不改归约数值。
TEST(RuntimeShutdownReduceConcurrency, ReduceNumericEquivalenceUnchangedByGate) {
    runtime_shutdown();
    RuntimeConfig one;
    one.max_threads = 1;
    one.arena_concurrency = 1;
    runtime_init(one);
    const int n = 1000000;
    auto map = [](std::size_t i) { return static_cast<long long>(i); };
    auto op = [](long long a, long long b) { return a + b; };
    const long long serial_ref =
        static_cast<long long>(n) * static_cast<long long>(n - 1) / 2;
    long long r1 = parallel_reduce<long long>(KernelId::Custom, Range1D{0, n}, 0LL, map, op);
    runtime_shutdown();

    RuntimeConfig many;
    many.max_threads = 8;
    many.arena_concurrency = 8;
    runtime_init(many);
    long long rN = parallel_reduce<long long>(KernelId::Custom, Range1D{0, n}, 0LL, map, op);
    runtime_shutdown();

    EXPECT_EQ(r1, serial_ref) << "1 worker 归约结果偏离解析值";
    EXPECT_EQ(rN, serial_ref) << "N worker 归约结果偏离解析值";
    EXPECT_EQ(r1, rN) << "归约结果随并发度变化 —— 违反数值与并发度无关不变量";
    EXPECT_EQ(parallel_reduce<int>(KernelId::Custom, Range1D{0, 0}, 7,
                                 [](std::size_t) { return 1; },
                                 [](int a, int b) { return a + b; }),
              7) << "空 range 必须返回 identity";
}

}  // namespace
