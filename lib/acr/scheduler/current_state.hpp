// lib/acr/scheduler/current_state.hpp — 运行时设备状态跟踪
// Phase F4：工作保持调度器的当前状态结构。
//
// 设计（控制包 07_STATIC_ROUTING_AND_MIXED_EXECUTION.md §5-§6）：
//   1. CurrentState 跟踪每个设备的队列负载、上次完成时间、可用性
//   2. 调度器据此做工作保持决策（选 finish 最短的空闲合格设备）
//   3. 不是在线学习：状态由调度器运行时维护，不写回 hardware-profile
//   4. 剩余工作计数用于 guided 尾部收缩
//   5. 线程安全：所有方法用 mutex 保护（dispatcher 工作线程并发访问）
//   6. 公共头不暴露第三方类型
#pragma once

#include "partitioner.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace astro::compute::scheduler {

// ===== 单设备运行时状态 =====
struct DeviceRuntimeState {
    std::string backend;             // "cpu" / "cuda:0" / ...
    std::atomic<std::uint64_t> queue_load_ns{0};   // 当前队列累计等待时间
    std::atomic<std::uint64_t> last_finish_ns{0};  // 上次任务完成时间戳（steady_clock）
    std::atomic<bool> available{true};
    std::atomic<std::size_t> chunks_assigned{0};   // 已分配的 chunk 数
    std::atomic<std::size_t> chunks_completed{0};  // 已完成的 chunk 数
    std::atomic<std::size_t> chunks_failed{0};     // 失败的 chunk 数

    DeviceRuntimeState() = default;
    explicit DeviceRuntimeState(const std::string& b) : backend(b) {}
    DeviceRuntimeState(DeviceRuntimeState&& other) noexcept
        : backend(std::move(other.backend)),
          queue_load_ns(other.queue_load_ns.load()),
          last_finish_ns(other.last_finish_ns.load()),
          available(other.available.load()),
          chunks_assigned(other.chunks_assigned.load()),
          chunks_completed(other.chunks_completed.load()),
          chunks_failed(other.chunks_failed.load()) {}

    // 累加队列负载
    void add_queue_load(std::uint64_t ns) noexcept {
        queue_load_ns.fetch_add(ns, std::memory_order_relaxed);
    }
    // 减少队列负载（任务完成）
    void reduce_queue_load(std::uint64_t ns) noexcept {
        std::uint64_t cur = queue_load_ns.load(std::memory_order_relaxed);
        while (cur > 0 && !queue_load_ns.compare_exchange_weak(cur, cur >= ns ? cur - ns : 0,
                                                                std::memory_order_relaxed)) {}
    }
    // 标记完成
    void mark_chunk_done() noexcept {
        chunks_completed.fetch_add(1, std::memory_order_relaxed);
    }
    void mark_chunk_failed() noexcept {
        chunks_failed.fetch_add(1, std::memory_order_relaxed);
    }
};

// ===== CurrentState：调度器全局状态 =====
// 跟踪所有设备 + 剩余工作 + coverage bitmap。
// 不是在线学习：状态由调度器运行时维护，不持久化，不写回 profile。
class CurrentState {
public:
    CurrentState() = default;

    // 初始化设备列表
    void init_devices(const std::vector<std::string>& backends);

    // ===== 设备查询 =====
    std::size_t device_count() const noexcept;
    std::vector<std::string> backends() const;
    bool is_available(const std::string& backend) const;
    DeviceRuntimeState* find_device(const std::string& backend);
    const DeviceRuntimeState* find_device(const std::string& backend) const;

    // ===== 工作保持：选 finish 最短的空闲合格设备 =====
    // finish = queue_load_ns + now - last_finish_ns（粗略估算）
    // 返回 backend 字符串；空字符串表示无可用设备
    std::string pick_finish_shortest(const std::vector<std::string>& candidate_backends) const;

    // ===== 剩余工作跟踪 =====
    void set_total_chunks(std::size_t n);
    std::size_t total_chunks() const noexcept;
    std::size_t completed_chunks() const noexcept;
    std::size_t pending_chunks() const noexcept;
    bool all_done() const noexcept;
    double completion_ratio() const noexcept;

    // ===== Coverage Bitmap 管理 =====
    void init_coverage(std::size_t chunk_count);
    CoverageBitmap& coverage() noexcept;
    const CoverageBitmap& coverage() const noexcept;

    // ===== Guided 尾部收缩 =====
    // 剩余工作减少时，建议更小的 chunk_size（避免最后一个 chunk 过大）
    // guided 分割：next_chunk = max(1, remaining / (2 × n_workers))
    std::size_t guided_next_chunk_size(std::size_t remaining, std::size_t n_workers) const noexcept;

    // ===== 状态重置（新任务前调用）=====
    void reset();

    // ===== 诊断 JSON =====
    std::string status_json() const;

private:
    mutable std::mutex mtx_;
    std::vector<DeviceRuntimeState> devices_;
    std::size_t total_chunks_{0};
    CoverageBitmap coverage_;
};

// ===== 时间工具（steady_clock 纳秒）=====
inline std::uint64_t steady_now_ns() noexcept {
    auto tp = std::chrono::steady_clock::now();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()).count());
}

} // namespace astro::compute::scheduler
