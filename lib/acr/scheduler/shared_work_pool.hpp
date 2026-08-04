// lib/acr/scheduler/shared_work_pool.hpp — 共享工作池（23 号计划 §2 重写）
//
// 重写目标（audits/SECOND_FIX_REVIEW_AUDIT.md §一.4 + 23 号计划 §2）：
//   1. WorkToken 按值返回：{id, begin, end, claimant(DeviceId), attempt}，
//      不依赖池内地址，禁止返回并发增长容器内部指针；
//   2. 原子范围领取：CAS loop 推进 cursor，token 范围一经生成不可变；
//   3. completion ledger / retry queue 使用稳定槽位（预分配）+ 受锁容器；
//   4. block ID 与槽位一一对应（预分配 slot[i].id == i），
//      不依赖插入顺序（不再 push_back 竞争插入）；
//   5. 正确完成判据：
//        cursor >= end
//        && inflight == 0
//        && retry_queue.empty()
//        && failed_terminal == 0
//        && completed_items == end - begin
//   6. 失败回收：retryable 失败进入 retry queue，可由任意 executor 重新领取；
//      已成功 DONE 的块不可再次执行（attempt 防 ABA：重试领取会递增 attempt，
//      旧 token 的 attempt 不再匹配）。
#pragma once

#include "astro/compute/hardware_profile.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace astro::compute::scheduler {

// ===== 工作块状态 =====
enum class WorkBlockStatus : std::uint8_t {
    Pending = 0,   // 未领取 / 可领取
    Claimed = 1,   // 已被某设备领取（执行中）
    Done    = 2,   // 成功完成（终态）
    Failed  = 3,   // 执行失败（可回收重新入池）
};

// ===== 值令牌（claim 成功后按值返回）=====
// 23 号计划 §2.1：{id, begin, end, claimant, attempt}
struct WorkToken {
    std::uint64_t id{0};            // 槽位 ID（与 slots_[id] 一一对应，预分配）
    std::size_t begin{0};           // 范围起始（不可变）
    std::size_t end{0};             // 范围结束（不可变）
    DeviceId claimant{kHwInvalidDeviceId};  // 领取设备
    std::uint32_t attempt{0};       // 尝试次数（第几次领取此范围，防 ABA）

    bool valid() const noexcept { return end > begin; }
    std::size_t size() const noexcept { return end - begin; }
};

// ===== 预分配槽位（地址稳定，原子状态）=====
struct WorkSlot {
    std::size_t id{0};
    std::atomic<std::size_t> begin{0};
    std::atomic<std::size_t> end{0};
    std::atomic<WorkBlockStatus> status{WorkBlockStatus::Pending};
    std::atomic<int> attempt_count{0};          // 每次领取递增（防 ABA）
    std::atomic<DeviceId> claimant{kHwInvalidDeviceId};

    WorkSlot() = default;
    WorkSlot(const WorkSlot&) = delete;
    WorkSlot& operator=(const WorkSlot&) = delete;
    WorkSlot(WorkSlot&&) = delete;
    WorkSlot& operator=(WorkSlot&&) = delete;

    // Pending → Claimed（写入 begin/end 后 release 发布）
    // 返回新 attempt（调用方写入 token）
    std::uint32_t try_claim(DeviceId device, std::size_t b, std::size_t e) {
        WorkBlockStatus expected = WorkBlockStatus::Pending;
        if (!status.compare_exchange_strong(expected, WorkBlockStatus::Claimed,
                                             std::memory_order_acq_rel)) {
            return 0;
        }
        begin.store(b, std::memory_order_relaxed);
        end.store(e, std::memory_order_relaxed);
        claimant.store(device, std::memory_order_relaxed);
        return static_cast<std::uint32_t>(
            attempt_count.fetch_add(1, std::memory_order_relaxed) + 1);
    }

    // Failed → Claimed（retry 领取；返回新 attempt）
    std::uint32_t try_reclaim_claim(DeviceId device) {
        WorkBlockStatus expected = WorkBlockStatus::Failed;
        if (!status.compare_exchange_strong(expected, WorkBlockStatus::Claimed,
                                             std::memory_order_acq_rel)) {
            return 0;
        }
        claimant.store(device, std::memory_order_relaxed);
        return static_cast<std::uint32_t>(
            attempt_count.fetch_add(1, std::memory_order_relaxed) + 1);
    }

    // Claimed → Done（验证 attempt，防 ABA）
    bool try_mark_done(std::uint32_t expected_attempt) {
        WorkBlockStatus expected = WorkBlockStatus::Claimed;
        if (!status.compare_exchange_strong(expected, WorkBlockStatus::Done,
                                             std::memory_order_acq_rel)) {
            return false;
        }
        if (attempt_count.load(std::memory_order_acquire) !=
            static_cast<int>(expected_attempt)) {
            status.store(WorkBlockStatus::Claimed, std::memory_order_release);
            return false;
        }
        return true;
    }

    // Claimed → Failed（验证 attempt，防 ABA）
    bool try_mark_failed(std::uint32_t expected_attempt) {
        WorkBlockStatus expected = WorkBlockStatus::Claimed;
        if (!status.compare_exchange_strong(expected, WorkBlockStatus::Failed,
                                             std::memory_order_acq_rel)) {
            return false;
        }
        if (attempt_count.load(std::memory_order_acquire) !=
            static_cast<int>(expected_attempt)) {
            status.store(WorkBlockStatus::Claimed, std::memory_order_release);
            return false;
        }
        return true;
    }

    // Failed → Pending（重新入池）
    bool try_reclaim() {
        WorkBlockStatus expected = WorkBlockStatus::Failed;
        return status.compare_exchange_strong(expected, WorkBlockStatus::Pending,
                                               std::memory_order_acq_rel);
    }
};

// ===== RetryQueue 条目（失败回收后等待重新领取）=====
struct RetryEntry {
    std::size_t slot_id{0};
    std::size_t begin{0};
    std::size_t end{0};
};

// ===== SharedWorkPool =====
// 所有设备从此池领取未开始工作块。固定模式预创建全部块；
// 动态模式 CAS cursor 领取 + 稳定槽位，块大小由调用方（CostEstimator）给出。
class SharedWorkPool {
public:
    SharedWorkPool() = default;
    ~SharedWorkPool();

    SharedWorkPool(const SharedWorkPool&) = delete;
    SharedWorkPool& operator=(const SharedWorkPool&) = delete;

    // 固定模式：按 chunk_size 拆分 [begin, end)，预创建所有槽位
    void init(std::size_t begin, std::size_t end, std::size_t chunk_size);

    // 动态模式：预分配最大可能槽位数（按 min_chunk），claim 时填充范围
    void init_dynamic(std::size_t begin, std::size_t end,
                      std::size_t min_chunk, std::size_t max_chunk);

    // ===== 领取（固定模式）=====
    // 返回 WorkToken 值拷贝；invalid token 表示无 Pending 块
    WorkToken claim_next(DeviceId device);

    // ===== 领取（动态模式）=====
    // requested_items：调用方（CostEstimator/executor）建议的块大小；
    // 池内部按 remaining 收缩尾部并 clamp 到 [min_chunk, max_chunk]。
    // 返回 WorkToken 值拷贝；invalid token 表示无剩余工作。
    WorkToken claim_next_dynamic(DeviceId device, std::size_t requested_items);

    // 运行时调整动态模式最大块大小（资源闭环 ShrinkBlock 用）
    void set_dynamic_max_chunk(std::size_t max_chunk) noexcept;

    // ===== 完成 / 失败 =====
    bool mark_done(const WorkToken& token);
    // retryable=true：进入 retry queue（可被其他 executor 重新领取）
    // retryable=false：终态失败（failed_terminal++，不可重试）
    bool mark_failed(const WorkToken& token, bool retryable = true);

    // 兼容旧接口（id+attempt）
    bool mark_done(std::size_t id, std::uint32_t attempt);
    bool mark_failed(std::size_t id, std::uint32_t attempt, bool retryable = true);

    // 把 retry queue 中的失败块全部重新入池（Failed → Pending）
    std::size_t reclaim_failed();

    // ===== 统计 =====
    std::size_t total_blocks() const noexcept;
    std::size_t pending_count() const noexcept;
    std::size_t claimed_count() const noexcept;
    std::size_t done_count() const noexcept;
    std::size_t failed_count() const noexcept;         // retry_pending + terminal
    std::size_t retry_pending_count() const noexcept;
    std::size_t failed_terminal_count() const noexcept;
    std::size_t completed_items() const noexcept;

    // ===== 完成判据（23 号计划 §2.3）=====
    bool all_done() const noexcept;
    bool no_work_left() const noexcept;

    // ===== 只读查询（测试/诊断）=====
    WorkBlockStatus slot_status(std::size_t id) const;
    std::size_t slot_begin(std::size_t id) const;
    std::size_t slot_end(std::size_t id) const;
    std::uint32_t slot_attempt(std::size_t id) const;
    DeviceId slot_claimant(std::size_t id) const;
    std::vector<bool> done_bitmap() const;

    std::size_t remaining_work() const noexcept;
    std::size_t active_slot_count() const noexcept;
    bool is_dynamic() const noexcept { return dynamic_mode_; }
    std::size_t min_chunk() const noexcept { return dyn_min_chunk_; }
    std::size_t max_chunk() const noexcept { return dyn_max_chunk_.load(std::memory_order_relaxed); }

    // ===== 重置 =====
    void reset();

private:
    WorkSlot* get_slot(std::size_t id) noexcept;
    const WorkSlot* get_slot(std::size_t id) const noexcept;

    // 从 retry queue 领取一个条目（锁内 pop；队列空返回 false）
    bool pop_retry(RetryEntry& out);

    std::vector<std::unique_ptr<WorkSlot>> slots_;
    std::size_t capacity_{0};

    // 固定模式
    std::size_t total_blocks_{0};
    std::atomic<std::size_t> next_claim_index_{0};

    // 动态模式
    bool dynamic_mode_{false};
    std::size_t range_begin_{0};
    std::size_t range_end_{0};
    std::size_t dyn_min_chunk_{1};
    std::atomic<std::size_t> dyn_max_chunk_{65536};
    std::atomic<std::size_t> dyn_cursor_{0};
    std::atomic<std::size_t> next_slot_{0};

    // 完成账本（原子计数）
    std::atomic<std::size_t> claimed_count_{0};   // 累计领取次数（含重试，诊断用）
    std::atomic<std::size_t> inflight_count_{0};  // 当前执行中（Claimed 未终态）块数
    std::atomic<std::size_t> done_count_{0};      // 成功完成块数
    std::atomic<std::size_t> retry_pending_{0};   // 失败可重试（在 retry queue）
    std::atomic<std::size_t> failed_terminal_{0}; // 终态失败
    std::atomic<std::size_t> completed_items_{0}; // 成功完成元素数

    // retry queue（受锁保护；条目是值拷贝，无内部指针泄漏）
    mutable std::mutex retry_mtx_;
    std::vector<RetryEntry> retry_queue_;
};

} // namespace astro::compute::scheduler
