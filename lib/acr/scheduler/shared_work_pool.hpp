// lib/acr/scheduler/shared_work_pool.hpp — 共享工作池（F-fix 2）
//
// CostEstimator 驱动的 Shared Pending Pool（控制包 07_COST_MODEL §3）：
//   1. 工作块状态机：PENDING → CLAIMED(device, attempt) → DONE / FAILED
//   2. FAILED 可重新进入 PENDING；已 DONE 不可再次执行
//   3. claim 使用原子状态转换
//   4. 已开始块不跨设备迁移
//   5. 设备失败只回收未开始或明确失败块
//   6. coverage 由实际完成事件驱动
//   7. 禁止执行结束后无条件批量 mark_done
//   8. 最终验证每块恰好一次成功完成
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace astro::compute::scheduler {

// ===== 工作块状态 =====
enum class WorkBlockStatus : std::uint8_t {
    Pending = 0,   // 未领取
    Claimed = 1,   // 已被某设备领取
    Done     = 2,   // 成功完成
    Failed   = 3,   // 执行失败（可重新入池）
};

// ===== 单个工作块 =====
struct WorkBlock {
    std::size_t id{0};              // 唯一 ID
    std::size_t begin{0};           // 范围起始
    std::size_t end{0};             // 范围结束
    std::atomic<WorkBlockStatus> status{WorkBlockStatus::Pending};
    std::string claimed_device;     // 领取此块的设备
    int attempt_count{0};           // 尝试次数

    WorkBlock() = default;
    WorkBlock(std::size_t id_, std::size_t b, std::size_t e)
        : id(id_), begin(b), end(e) {}

    // 显式移动构造（atomic 不可拷贝）
    WorkBlock(WorkBlock&& other) noexcept
        : id(other.id), begin(other.begin), end(other.end),
          status(other.status.load(std::memory_order_relaxed)),
          claimed_device(std::move(other.claimed_device)),
          attempt_count(other.attempt_count) {}
    WorkBlock& operator=(WorkBlock&& other) noexcept {
        id = other.id;
        begin = other.begin;
        end = other.end;
        status.store(other.status.load(std::memory_order_relaxed), std::memory_order_relaxed);
        claimed_device = std::move(other.claimed_device);
        attempt_count = other.attempt_count;
        return *this;
    }
    WorkBlock(const WorkBlock&) = delete;
    WorkBlock& operator=(const WorkBlock&) = delete;

    // 原子状态转换：Pending → Claimed
    bool try_claim(const std::string& device_id) {
        WorkBlockStatus expected = WorkBlockStatus::Pending;
        if (status.compare_exchange_strong(expected, WorkBlockStatus::Claimed,
                                            std::memory_order_acq_rel)) {
            claimed_device = device_id;
            ++attempt_count;
            return true;
        }
        return false;
    }

    // 标记完成：Claimed → Done
    bool try_mark_done() {
        WorkBlockStatus expected = WorkBlockStatus::Claimed;
        return status.compare_exchange_strong(expected, WorkBlockStatus::Done,
                                               std::memory_order_acq_rel);
    }

    // 标记失败：Claimed → Failed（可重新入池）
    bool try_mark_failed() {
        WorkBlockStatus expected = WorkBlockStatus::Claimed;
        return status.compare_exchange_strong(expected, WorkBlockStatus::Failed,
                                               std::memory_order_acq_rel);
    }

    // 重新入池：Failed → Pending
    bool try_reclaim() {
        WorkBlockStatus expected = WorkBlockStatus::Failed;
        return status.compare_exchange_strong(expected, WorkBlockStatus::Pending,
                                               std::memory_order_acq_rel);
    }
};

// ===== SharedWorkPool =====
// 共享工作池：所有设备从此池领取未开始的工作块。
// CostEstimator 驱动：每次 claim 时可选择最优设备和块大小。
class SharedWorkPool {
public:
    SharedWorkPool() = default;

    // 初始化：按 chunk_size 拆分 [begin, end)
    void init(std::size_t begin, std::size_t end, std::size_t chunk_size);

    // ===== 领取下一块 =====
    // 原子状态转换 Pending → Claimed
    // 返回指向 WorkBlock 的指针；nullptr 表示无 pending 块
    // device_id: 领取此块的设备标识
    WorkBlock* claim_next(const std::string& device_id);

    // ===== 标记完成 =====
    void mark_done(std::size_t id);

    // ===== 标记失败 =====
    // 失败块可重新入池（try_reclaim）
    void mark_failed(std::size_t id);

    // ===== 回收失败块 =====
    // 将所有 Failed 块重新设为 Pending
    std::size_t reclaim_failed();

    // ===== 统计 =====
    std::size_t total_blocks() const noexcept;
    std::size_t pending_count() const noexcept;
    std::size_t claimed_count() const noexcept;
    std::size_t done_count() const noexcept;
    std::size_t failed_count() const noexcept;

    // 检查是否全部完成（所有块为 Done）
    bool all_done() const noexcept;

    // 检查是否没有可领取的块（无 Pending 且无 Failed）
    bool no_work_left() const noexcept;

    // 获取块引用（用于执行）
    const std::vector<WorkBlock>& blocks() const noexcept { return blocks_; }
    std::vector<WorkBlock>& blocks() noexcept { return blocks_; }

    // 获取 done 位图（与 CoverageBitmap 兼容）
    // done[i] = (blocks_[i].status == Done)
    std::vector<bool> done_bitmap() const;

    // ===== 动态 chunk 大小 =====
    // F-fix 3：根据剩余工作和活跃设备数建议下一块大小
    // chunk = max(min_chunk, remaining / (2 * n_active_devices))
    std::size_t suggest_next_chunk(std::size_t n_active_devices,
                                    std::size_t min_chunk,
                                    std::size_t max_chunk) const noexcept;

    // ===== 重置 =====
    void reset();

private:
    std::vector<WorkBlock> blocks_;
    std::atomic<std::size_t> next_claim_index_{0};
    mutable std::mutex mtx_;  // 保护 reclaim_failed 等需要遍历的操作
};

} // namespace astro::compute::scheduler
