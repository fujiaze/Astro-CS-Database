// lib/acr/tests/unit/test_work_pool.cpp — 共享工作池并发专项测试
//
// 覆盖强制案例：
// 1. 强制 A/B 线程逆序交错：A 先取得 token 后暂停，B 取得下一 token 并先完成；
// 验证 ID、槽位、状态和完成块完全对应；
// 2. gate 关闭时仍有未领取范围：已领取块全部完成但 cursor < end 时 all_done
// 必须为 false（禁止静默漏算）；
// 3. 1000 轮高并发压力：无重叠、无遗漏、每块恰好完成一次；
// 4. 失败回收与重复领取：retryable 失败进入 retry queue，重试 attempt 递增；
// 5. 每个 item 恰好一次（completed_items == end - begin）。
#include <gtest/gtest.h>

#include "shared_work_pool.hpp"

#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

using namespace astro::compute::scheduler;
using astro::compute::DeviceId;
using astro::compute::kHwCpuDeviceId;

namespace {

constexpr DeviceId kGpu0 = static_cast<DeviceId>(1);  // GPU 0

// 执行 [begin,end) 并标记 coverage（每 item 恰好一次）
void execute_range(std::vector<std::atomic<unsigned>>& coverage,
                   std::size_t begin, std::size_t end) {
    for (std::size_t i = begin; i < end; ++i) {
        coverage[i].fetch_add(1, std::memory_order_relaxed);
    }
}

} // anonymous namespace

// ============================================================================
// 1. 强制 A/B 线程逆序交错（ID/槽位/状态一一对应）
// ============================================================================
TEST(WorkPoolConcurrency, ForcedInterleaveReverseOrder) {
    SharedWorkPool pool;
    pool.init(0, 200, 50);  // 固定模式 4 块

    WorkToken ta;
    WorkToken tb;
    std::atomic<bool> a_claimed{false};
    std::atomic<bool> b_done{false};

    std::thread ta_thread([&] {
        ta = pool.claim_next(kHwCpuDeviceId);
        a_claimed.store(true, std::memory_order_release);
        // 暂停直到 B 领取并完成（逆序完成）
        while (!b_done.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        pool.mark_done(ta);
    });

    while (!a_claimed.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    tb = pool.claim_next(kHwCpuDeviceId);

    ASSERT_TRUE(ta.valid());
    ASSERT_TRUE(tb.valid());
    EXPECT_NE(ta.id, tb.id);
    EXPECT_EQ(ta.claimant, kHwCpuDeviceId);
    EXPECT_EQ(tb.claimant, kHwCpuDeviceId);

    // B 先完成，A 后完成（逆序）
    EXPECT_TRUE(pool.mark_done(tb));
    b_done.store(true, std::memory_order_release);
    ta_thread.join();

    // 槽位状态与 token 对应：两个块都 Done
    EXPECT_EQ(pool.slot_status(ta.id), WorkBlockStatus::Done);
    EXPECT_EQ(pool.slot_status(tb.id), WorkBlockStatus::Done);
    EXPECT_EQ(pool.slot_begin(ta.id), ta.begin);
    EXPECT_EQ(pool.slot_end(ta.id), ta.end);
    EXPECT_EQ(pool.slot_begin(tb.id), tb.begin);
    EXPECT_EQ(pool.slot_end(tb.id), tb.end);

    // 剩余两块继续完成
    for (int i = 0; i < 2; ++i) {
        auto t = pool.claim_next(kHwCpuDeviceId);
        ASSERT_TRUE(t.valid());
        EXPECT_TRUE(pool.mark_done(t));
    }
    EXPECT_TRUE(pool.all_done());
    EXPECT_EQ(pool.done_count(), 4u);
    EXPECT_EQ(pool.completed_items(), 200u);
}

// ============================================================================
// 2. gate 关闭时仍有未领取范围：all_done 必须为 false
// ============================================================================
TEST(WorkPoolConcurrency, GateCloseKeepsUnclaimedRanges) {
    SharedWorkPool pool;
    pool.init_dynamic(0, 1000, 100, 400);

    // 只领取并完成前两块；剩余范围不领取（模拟 submit gate 关闭）
    auto t1 = pool.claim_next_dynamic(kHwCpuDeviceId, 400);
    auto t2 = pool.claim_next_dynamic(kHwCpuDeviceId, 400);
    ASSERT_TRUE(t1.valid());
    ASSERT_TRUE(t2.valid());
    EXPECT_TRUE(pool.mark_done(t1));
    EXPECT_TRUE(pool.mark_done(t2));

    // 已领取块全部完成，但 cursor 未到 end → 禁止 all_done=true
    EXPECT_GT(pool.remaining_work(), 0u);
    EXPECT_FALSE(pool.all_done());

    // gate 恢复：继续领取并完成，最终不漏项
    while (true) {
        auto t = pool.claim_next_dynamic(kHwCpuDeviceId, 400);
        if (!t.valid()) break;
        EXPECT_TRUE(pool.mark_done(t));
    }
    EXPECT_TRUE(pool.all_done());
    EXPECT_EQ(pool.completed_items(), 1000u);
}

// ============================================================================
// 3. 1000 轮高并发压力：无重叠、无遗漏、每块恰好完成一次
// ============================================================================
TEST(WorkPoolConcurrency, Stress1000Rounds) {
    constexpr std::size_t kRounds = 1000;
    constexpr std::size_t kItems = 1024;
    constexpr std::size_t kThreads = 4;

    for (std::size_t round = 0; round < kRounds; ++round) {
        SharedWorkPool pool;
        pool.init_dynamic(0, kItems, 64, 256);

        std::vector<std::atomic<unsigned>> coverage(kItems);
        for (auto& c : coverage) c.store(0, std::memory_order_relaxed);

        std::atomic<std::size_t> completed{0};
        std::atomic<bool> any_fail{false};

        std::vector<std::thread> workers;
        workers.reserve(kThreads);
        for (std::size_t w = 0; w < kThreads; ++w) {
            workers.emplace_back([&] {
                while (true) {
                    auto t = pool.claim_next_dynamic(
                        kHwCpuDeviceId, 256);
                    if (!t.valid()) break;
                    execute_range(coverage, t.begin, t.end);
                    if (!pool.mark_done(t)) {
                        any_fail.store(true, std::memory_order_relaxed);
                    }
                    completed.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        for (auto& th : workers) th.join();

        EXPECT_FALSE(any_fail.load()) << "round " << round;
        EXPECT_TRUE(pool.all_done()) << "round " << round;
        EXPECT_EQ(pool.completed_items(), kItems) << "round " << round;
        EXPECT_EQ(completed.load(), pool.total_blocks()) << "round " << round;
        for (std::size_t i = 0; i < kItems; ++i) {
            EXPECT_EQ(coverage[i].load(), 1u)
                << "item " << i << " processed " << coverage[i].load()
                << " times (round " << round << ")";
        }
    }
}

// ============================================================================
// 4. 失败回收与重复领取：attempt 递增，最终不漏项
// ============================================================================
TEST(WorkPoolConcurrency, FailureReclaimAndReclaim) {
    SharedWorkPool pool;
    pool.init_dynamic(0, 500, 100, 250);

    auto t1 = pool.claim_next_dynamic(kHwCpuDeviceId, 250);
    ASSERT_TRUE(t1.valid());
    EXPECT_EQ(t1.attempt, 1u);

    // retryable 失败 → 进入 retry queue
    EXPECT_TRUE(pool.mark_failed(t1));
    EXPECT_EQ(pool.retry_pending_count(), 1u);
    EXPECT_FALSE(pool.all_done());

    // 下一次领取拿到重试块（attempt=2，claimant 可为其他设备）
    auto t2 = pool.claim_next_dynamic(kGpu0, 250);
    ASSERT_TRUE(t2.valid());
    EXPECT_EQ(t2.begin, t1.begin);
    EXPECT_EQ(t2.end, t1.end);
    EXPECT_EQ(t2.attempt, 2u);
    EXPECT_EQ(t2.claimant, kGpu0);

    // 旧 token 已完成失效防护：用 t1 再 mark_done 必须失败
    EXPECT_FALSE(pool.mark_done(t1));
    EXPECT_TRUE(pool.mark_done(t2));

    // 完成剩余范围
    while (true) {
        auto t = pool.claim_next_dynamic(kHwCpuDeviceId, 250);
        if (!t.valid()) break;
        EXPECT_TRUE(pool.mark_done(t));
    }
    EXPECT_TRUE(pool.all_done());
    EXPECT_EQ(pool.completed_items(), 500u);
    EXPECT_EQ(pool.retry_pending_count(), 0u);
    EXPECT_EQ(pool.failed_terminal_count(), 0u);
}

// ============================================================================
// 4b. 确定性 ABA 交错：旧 attempt 延迟完成 vs 新 attempt 已领取同一范围
// ============================================================================
TEST(WorkPoolConcurrency, StaleAttemptCannotChangeNewState) {
    SharedWorkPool pool;
    pool.init_dynamic(0, 1000, 100, 1000);

    // A 领取 [0,1000) attempt=1，然后失败进入 retry queue
    auto t1 = pool.claim_next_dynamic(kHwCpuDeviceId, 1000);
    ASSERT_TRUE(t1.valid());
    EXPECT_EQ(t1.attempt, 1u);
    EXPECT_TRUE(pool.mark_failed(t1));

    // B 线程领取同一范围（attempt=2）并完成
    std::atomic<bool> b_done{false};
    std::thread b([&] {
        auto t2 = pool.claim_next_dynamic(kGpu0, 1000);
        ASSERT_TRUE(t2.valid());
        EXPECT_EQ(t2.begin, t1.begin);
        EXPECT_EQ(t2.end, t1.end);
        EXPECT_EQ(t2.attempt, 2u);
        EXPECT_TRUE(pool.mark_done(t2));
        b_done.store(true, std::memory_order_release);
    });

    // A 等待 B 完成后再尝试用旧 token 完成（延迟完成）
    while (!b_done.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    EXPECT_FALSE(pool.mark_done(t1));  // 旧 attempt 不能改变新状态
    EXPECT_EQ(pool.slot_status(t1.id), WorkBlockStatus::Done);  // 仍为新 attempt 的 Done
    EXPECT_EQ(pool.slot_attempt(t1.id), 2u);
    b.join();

    EXPECT_TRUE(pool.all_done());
    EXPECT_EQ(pool.completed_items(), 1000u);
    EXPECT_EQ(pool.retry_pending_count(), 0u);
}

// ============================================================================
// 5. 每个 item 恰好一次（并发多设备 claim）
// ============================================================================
TEST(WorkPoolConcurrency, ExactOncePerItemConcurrent) {
    constexpr std::size_t kItems = 8192;
    SharedWorkPool pool;
    pool.init_dynamic(0, kItems, 128, 1024);

    std::vector<std::atomic<unsigned>> coverage(kItems);
    for (auto& c : coverage) c.store(0, std::memory_order_relaxed);

    constexpr std::size_t kThreads = 8;
    std::vector<std::thread> workers;
    for (std::size_t w = 0; w < kThreads; ++w) {
        const DeviceId dev = (w % 2 == 0) ? kHwCpuDeviceId : kGpu0;
        workers.emplace_back([&, dev] {
            while (true) {
                auto t = pool.claim_next_dynamic(dev, 512);
                if (!t.valid()) break;
                EXPECT_EQ(t.claimant, dev);
                execute_range(coverage, t.begin, t.end);
                EXPECT_TRUE(pool.mark_done(t));
            }
        });
    }
    for (auto& th : workers) th.join();

    EXPECT_TRUE(pool.all_done());
    EXPECT_EQ(pool.completed_items(), kItems);
    std::size_t ones = 0;
    for (std::size_t i = 0; i < kItems; ++i) {
        EXPECT_EQ(coverage[i].load(), 1u);
        ones += (coverage[i].load() == 1u);
    }
    EXPECT_EQ(ones, kItems);
}
