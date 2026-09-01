// lib/phase2/include/astro/phase2/async_io.h — CON-008 bounded async I/O queue
//
// 有界 producer/consumer 队列，用于 HiPS/FITS/XISF 读取任务与计算任务解耦。
// 队列容量由调用方根据 memory_budget_bytes 和单条目字节数计算，
// 生产者满时阻塞（背压），消费者空时阻塞；支持 close/cancel/错误传播。
//
// Reader 线程安全约束（与 ASYNC_IO_CONTRACT.md 一致）：
// - cfitsio 同一数据集句柄不保证跨线程并发读安全；
// - 因此每个读取线程必须持有独立 reader/AioHipsDataset 句柄（CON-004 的
// per-worker reader 形态），或把全部 cfitsio 调用串行化到单一 IO 线程
// （“有界预取队列 + 单一 IO 线程”形态），二者都禁止跨线程共享句柄并发读；
// - 共享句柄只允许串行读取；若在异步队列中共享，需由 reader 层串行锁并计入
// 串行时间预算。本队列不为你自动建立跨线程句柄共享，调用方必须遵守上述约束。
#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace astro::phase2 {

// 根据内存预算和单条目估算字节数计算有界队列容量（至少 1）。
inline std::size_t bounded_queue_capacity(std::size_t memory_budget_bytes,
                                          std::size_t item_bytes) {
    if (item_bytes == 0) return 1;
    const std::size_t n = memory_budget_bytes / item_bytes;
    return n > 0 ? n : 1;
}

template<class T>
class BoundedAsyncQueue {
public:
    explicit BoundedAsyncQueue(std::size_t capacity)
        : capacity_(capacity > 0 ? capacity : 1) {}

    BoundedAsyncQueue(const BoundedAsyncQueue&) = delete;
    BoundedAsyncQueue& operator=(const BoundedAsyncQueue&) = delete;

    // 生产者入队。队列满时阻塞；close/cancel 后返回 false。
    bool push(T value) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [this] {
            return !error_.empty() || closed_ || queue_.size() < capacity_;
        });
        if (!error_.empty() || closed_) return false;
        queue_.push_back(std::move(value));
        not_empty_.notify_one();
        return true;
    }

    // 消费者出队。队列空时阻塞；close 且队列空或 cancel 后返回 nullopt。
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] {
            return !error_.empty() || !queue_.empty() || closed_;
        });
        if (!error_.empty()) return std::nullopt;
        if (queue_.empty()) return std::nullopt;  // closed & drained
        T value = std::move(queue_.front());
        queue_.pop_front();
        not_full_.notify_one();
        return value;
    }

    // 正常关闭：不再接受新条目，已入队条目仍可被消费。
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    // 取消/错误：唤醒所有阻塞者，后续 push/pop 失败并保留错误文本。
    void cancel(const std::string& reason) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (error_.empty()) error_ = reason;
        closed_ = true;
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    bool has_error() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return !error_.empty();
    }

    std::string error() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return error_;
    }

    std::size_t capacity() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return capacity_;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
    std::deque<T> queue_;
    std::size_t capacity_;
    bool closed_ = false;
    std::string error_;
};

} // namespace astro::phase2
