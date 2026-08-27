// lib/phase2/tests/async_io_test.cpp — CON-008 bounded async queue unit tests
#include "astro/phase2/async_io.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

using astro::phase2::BoundedAsyncQueue;
using astro::phase2::bounded_queue_capacity;

namespace {

// 生产型 pipeline 的“tile 条目”：模拟从 HiPS/FITS/XISF 读出的 buffer 所有权对象。
// 入队后所有权移交给消费者；本测试用它验证有界队列在真实 I/O 形状下的行为。
struct SimTile {
    int id = 0;
    std::vector<float> payload;  // 模拟 tile buffer (512*512 量级)
};

}  // namespace

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

// ===================== CON-008 生产型异步 I/O pipeline 测试 =====================
// 这些测试把 BoundedAsyncQueue 放进“reader(producer) -> queue -> writer(consumer)”
// 的真实 pipeline 形状，模拟读取失败、写入失败、队列满、多 worker 并发消费，
// 并验证不 deadlock、不丢失条目、不丢失错误码。

TEST(BoundedAsyncQueuePipeline, ReadFailureCancelsAndPropagatesNoDeadlock) {
    BoundedAsyncQueue<SimTile> q(2);
    std::atomic<bool> consumer_saw_error{false};
    // reader 线程：读取 tile 0..9；在 tile 5 模拟一次读取失败 → cancel 并停止。
    std::thread reader([&] {
        for (int i = 0; i < 10; ++i) {
            if (i == 5) {
                q.cancel("read failed (tile 5)");
                break;
            }
            SimTile t;
            t.id = i;
            t.payload.assign(512u * 512u, (float)i);
            if (!q.push(std::move(t))) break;  // 已取消/关闭
        }
    });
    // writer 线程：消费直到取消；读到已取消 → 记录错误可见。
    std::thread writer([&] {
        while (auto t = q.pop()) {
            (void)t->id;  // 模拟“计算/写入”已出队 buffer
        }
        if (q.has_error()) consumer_saw_error = true;
    });
    reader.join();
    writer.join();
    EXPECT_TRUE(consumer_saw_error.load());
    EXPECT_TRUE(q.has_error());
    EXPECT_EQ(q.error(), "read failed (tile 5)");
    EXPECT_EQ(q.pop(), std::nullopt);  // 取消后消费端返回空 → 错误码不被吞掉
}

TEST(BoundedAsyncQueuePipeline, WriteFailureStopsProducerAndPreservesError) {
    BoundedAsyncQueue<SimTile> q(2);
    std::atomic<bool> producer_saw_cancel{false};
    // writer 线程：在 tile 3 模拟一次写入失败 → cancel 并停止。
    std::thread writer([&] {
        while (auto t = q.pop()) {
            if (t->id == 3) {
                q.cancel("write failed (tile 3)");
                break;
            }
        }
    });
    // reader 线程：尝试连续入队；capacity=2 使其很快阻塞；一旦被取消，push 返回 false。
    std::thread reader([&] {
        for (int i = 0; i < 100; ++i) {
            SimTile t;
            t.id = i;
            if (!q.push(std::move(t))) {
                producer_saw_cancel = true;
                break;
            }
        }
    });
    writer.join();
    reader.join();
    EXPECT_TRUE(producer_saw_cancel.load());  // 写入失败背压传导到生产者
    EXPECT_TRUE(q.has_error());
    EXPECT_EQ(q.error(), "write failed (tile 3)");
}

TEST(BoundedAsyncQueuePipeline, BoundedQueueDeliversAllItemsInOrder) {
    constexpr int N = 256;
    BoundedAsyncQueue<SimTile> q(8);  // 小容量制造背压，验证无丢失且定序
    std::thread reader([&] {
        for (int i = 0; i < N; ++i) {
            SimTile t;
            t.id = i;
            if (!q.push(std::move(t))) break;
        }
        q.close();
    });
    std::vector<int> consumed;
    std::thread writer([&] {
        while (auto t = q.pop()) consumed.push_back(t->id);
    });
    reader.join();
    writer.join();
    ASSERT_EQ(consumed.size(), (std::size_t)N);
    for (int i = 0; i < N; ++i) EXPECT_EQ(consumed[(std::size_t)i], i);
}

TEST(BoundedAsyncQueuePipeline, MultiConsumerProcessesEachItemExactlyOnce) {
    constexpr int N = 1000;
    constexpr int Consumers = 4;
    BoundedAsyncQueue<SimTile> q(16);
    std::thread reader([&] {
        for (int i = 0; i < N; ++i) {
            SimTile t;
            t.id = i;
            if (!q.push(std::move(t))) break;
        }
        q.close();
    });
    std::vector<std::thread> workers;
    std::mutex mu;
    std::vector<int> seen((std::size_t)N, 0);  // 每 id 出现次数，应为 1
    std::atomic<int> total{0};
    for (int c = 0; c < Consumers; ++c) {
        workers.emplace_back([&] {
            while (auto t = q.pop()) {
                std::lock_guard<std::mutex> lk(mu);
                ++seen[(std::size_t)t->id];
                ++total;
            }
        });
    }
    reader.join();
    for (auto& w : workers) w.join();
    EXPECT_EQ(total.load(), N);
    for (int i = 0; i < N; ++i) EXPECT_EQ(seen[(std::size_t)i], 1);
}

TEST(BoundedAsyncQueuePipeline, CancelWakesBlockedConsumerNoDeadlock) {
    BoundedAsyncQueue<int> q(2);
    // 空队列上先启动一个消费者：它会阻塞在 pop。cancel 必须唤醒它并返回 nullopt。
    std::atomic<bool> consumer_returned_null{false};
    std::thread consumer([&] { consumer_returned_null = (q.pop() == std::nullopt); });
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    q.cancel("cancelled while consumer blocked");
    consumer.join();
    EXPECT_TRUE(consumer_returned_null.load());  // 消费者被取消唤醒
    EXPECT_TRUE(q.has_error());
    EXPECT_EQ(q.error(), "cancelled while consumer blocked");
    EXPECT_EQ(q.pop(), std::nullopt);  // 消费端也收到失败
    EXPECT_FALSE(q.push(2));  // 生产者端同样拒绝
}
