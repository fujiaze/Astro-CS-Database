// lib/phase2/tests/async_io_test.cpp — CON-008 bounded async queue unit tests
#include "astro/phase2/async_io.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using astro::phase2::BoundedAsyncQueue;
using astro::phase2::bounded_queue_capacity;

TEST(BoundedAsyncQueue, CapacityDerivedFromMemoryBudget) {
    EXPECT_EQ(bounded_queue_capacity(1024, 256), 4u);
    EXPECT_EQ(bounded_queue_capacity(1000, 256), 3u);  // 向下取整
    EXPECT_EQ(bounded_queue_capacity(0, 256), 1u);
    EXPECT_EQ(bounded_queue_capacity(100, 0), 1u);
}

TEST(BoundedAsyncQueue, PushPopRoundTrip) {
    BoundedAsyncQueue<int> q(4);
    ASSERT_TRUE(q.push(1));
    ASSERT_TRUE(q.push(2));
    ASSERT_EQ(q.size(), 2u);
    EXPECT_EQ(q.pop(), std::optional<int>(1));
    EXPECT_EQ(q.pop(), std::optional<int>(2));
    q.close();
    EXPECT_EQ(q.pop(), std::nullopt);
}

TEST(BoundedAsyncQueue, CloseDrainsAndRejectsNew) {
    BoundedAsyncQueue<int> q(4);
    ASSERT_TRUE(q.push(1));
    ASSERT_TRUE(q.push(2));
    q.close();
    EXPECT_FALSE(q.push(3));
    EXPECT_EQ(q.pop(), std::optional<int>(1));
    EXPECT_EQ(q.pop(), std::optional<int>(2));
    EXPECT_EQ(q.pop(), std::nullopt);
}

TEST(BoundedAsyncQueue, BackpressureBlocksProducerUntilConsumer) {
    BoundedAsyncQueue<int> q(2);
    std::atomic<int> pushed{0};
    std::atomic<bool> producer_done{false};
    std::thread producer([&] {
        for (int i = 0; i < 4; ++i) {
            if (!q.push(i)) break;
            pushed = i + 1;
        }
        producer_done = true;
    });
    // 队列容量 2，生产 4 个：前两个立即完成，后两个应等待消费。
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_GE(pushed.load(), 2);
    EXPECT_LE(pushed.load(), 2);
    EXPECT_EQ(q.pop(), std::optional<int>(0));
    // 等待生产者把第三个元素推入，再消费第 1 个；最后队列应剩 2 个元素。
    for (int i = 0; i < 100 && pushed.load() < 3; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_GE(pushed.load(), 3);
    EXPECT_EQ(q.pop(), std::optional<int>(1));
    // 让第 4 个元素也完成入队，再关闭。
    for (int i = 0; i < 200 && pushed.load() < 4; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_GE(pushed.load(), 4);
    q.close();
    producer.join();
    EXPECT_TRUE(producer_done.load());
    EXPECT_EQ(q.size(), 2u);
}

TEST(BoundedAsyncQueue, CancelWakesBlockedAndPropagatesError) {
    BoundedAsyncQueue<int> q(1);
    ASSERT_TRUE(q.push(0));   // 填满容量，使后续 push 阻塞。
    std::atomic<bool> pushed{false};
    std::thread producer([&] { pushed = q.push(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    q.cancel("read failed");
    producer.join();
    EXPECT_FALSE(pushed.load());
    EXPECT_TRUE(q.has_error());
    EXPECT_EQ(q.error(), "read failed");
    EXPECT_EQ(q.pop(), std::nullopt);
    EXPECT_FALSE(q.push(2));
}
