// lib/acr/scheduler/shared_work_pool.hpp — 共享工作池（F-fix 5 重构）
//
// F-fix 5 重构目标：
//   1. 禁止动态增长 std::vector<WorkBlock> 后返回裸指针；
//   2. block_id 与预分配槽位一一对应，锁外不分配 ID、锁内不乱序插入；
//   3. claim 返回值令牌 WorkToken{id, begin, end, generation, device_id}；
//   4. 状态存储采用预分配槽位（init 时 reserve，不再 push_back）；
//   5. 统计用原子计数器，不遍历 vector；
//   6. 并发安全：所有状态转换用 atomic CAS，无锁读取安全。
//
// 设计（控制包 07_COST_MODEL §3 + 22_FIX_REVIEW_CORRECTION_PLAN §F-fix 5）：
//   - 固定模式：init 时预创建所有块，reserve 后不再增长
//   - 动态模式：init_dynamic 预分配最大可能槽位数，claim 时填充 begin/end
//   - WorkToken 是值类型，拷贝返回，不依赖池内地址
//   - generation 防止 ABA：claim 时递增，mark_done 时验证
#pragma once

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
    Pending = 0,   // 未领取
    Claimed = 1,   // 已被某设备领取
    Done     = 2,   // 成功完成
    Failed   = 3,   // 执行失败（可重新入池）
};

// ===== 值令牌（claim 成功后返回，不依赖池内地址）=====
struct WorkToken {
    std::size_t id{0};              // 槽位 ID（与 slots_[id] 一一对应）
    std::size_t begin{0};           // 范围起始
    std::size_t end{0};             // 范围结束
    std::uint64_t generation{0};    // 代际标记（防止 ABA）
    std::string device_id;          // 领取此块的设备

    bool valid() const noexcept { return end > begin; }
    std::size_t size() const noexcept { return end - begin; }
};

// ===== 预分配槽位（地址稳定，atomic 状态）=====
struct WorkSlot {
    std::size_t id{0};
    std::atomic<std::size_t> begin{0};       // 动态模式 claim 时写入
    std::atomic<std::size_t> end{0};         // 动态模式 claim 时写入
    std::atomic<WorkBlockStatus> status{WorkBlockStatus::Pending};
    std::atomic<std::uint64_t> generation{0}; // 每次 claim 递增
    std::atomic<int> attempt_count{0};
    // claimed_device 只在 claim 时写，后续只读（通过 generation 保证可见性）
    // 用 atomic 避免数据竞争
    std::atomic<const char*> claimed_device{nullptr};  // 指向外部静态字符串

    WorkSlot() = default;

    // 禁止拷贝/移动（atomic 不可拷贝）
    WorkSlot(const WorkSlot&) = delete;
    WorkSlot& operator=(const WorkSlot&) = delete;
    WorkSlot(WorkSlot&&) = delete;
    WorkSlot& operator=(WorkSlot&&) = delete;

    // 原子状态转换：Pending → Claimed
    bool try_claim(const char* device_id_cstr, std::uint64_t expected_gen) {
        WorkBlockStatus expected = WorkBlockStatus::Pending;
        if (status.compare_exchange_strong(expected, WorkBlockStatus::Claimed,
                                            std::memory_order_acq_rel)) {
            // 验证代际（防止 ABA：Failed→Pending 后旧 token 误标记）
            // generation 在 reclaim 时递增，claim 时验证
            std::uint64_t cur_gen = generation.load(std::memory_order_acquire);
            if (cur_gen != expected_gen) {
                // 代际不匹配，回滚状态
                status.store(WorkBlockStatus::Pending, std::memory_order_release);
                return false;
            }
            claimed_device.store(device_id_cstr, std::memory_order_release);
            attempt_count.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        return false;
    }

    // 标记完成：Claimed → Done（验证 generation）
    bool try_mark_done(std::uint64_t expected_gen) {
        WorkBlockStatus expected = WorkBlockStatus::Claimed;
        if (status.compare_exchange_strong(expected, WorkBlockStatus::Done,
                                            std::memory_order_acq_rel)) {
            std::uint64_t cur_gen = generation.load(std::memory_order_acquire);
            if (cur_gen != expected_gen) {
                // 代际不匹配，回滚
                status.store(WorkBlockStatus::Claimed, std::memory_order_release);
                return false;
            }
            return true;
        }
        return false;
    }

    // 标记失败：Claimed → Failed（验证 generation）
    bool try_mark_failed(std::uint64_t expected_gen) {
        WorkBlockStatus expected = WorkBlockStatus::Claimed;
        if (status.compare_exchange_strong(expected, WorkBlockStatus::Failed,
                                            std::memory_order_acq_rel)) {
            std::uint64_t cur_gen = generation.load(std::memory_order_acquire);
            if (cur_gen != expected_gen) {
                status.store(WorkBlockStatus::Claimed, std::memory_order_release);
                return false;
            }
            return true;
        }
        return false;
    }

    // 重新入池：Failed → Pending（递增 generation，使旧 token 失效）
    bool try_reclaim() {
        WorkBlockStatus expected = WorkBlockStatus::Failed;
        if (status.compare_exchange_strong(expected, WorkBlockStatus::Pending,
                                            std::memory_order_acq_rel)) {
            generation.fetch_add(1, std::memory_order_release);
            return true;
        }
        return false;
    }
};

// ===== SharedWorkPool =====
// 共享工作池：所有设备从此池领取未开始的工作块。
// F-fix 5：预分配槽位 + WorkToken 值令牌，无并发 vector 增长。
class SharedWorkPool {
public:
    SharedWorkPool() = default;
    ~SharedWorkPool();

    // 初始化：按 chunk_size 拆分 [begin, end)（固定块大小模式）
    // 预分配所有槽位，reserve 后不再增长
    void init(std::size_t begin, std::size_t end, std::size_t chunk_size);

    // 动态初始化：预分配最大可能槽位数
    // max_slots = (end - begin + min_chunk - 1) / min_chunk
    // claim 时填充 begin/end（guided scheduling）
    void init_dynamic(std::size_t begin, std::size_t end,
                      std::size_t min_chunk, std::size_t max_chunk);

    // ===== 领取下一块（固定块大小模式）=====
    // 返回 WorkToken（值拷贝）；invalid token 表示无 pending 块
    WorkToken claim_next(const std::string& device_id);

    // ===== 动态领取下一块 =====
    // 根据剩余工作和活跃设备数计算块大小（guided scheduling）
    // 返回 WorkToken（值拷贝）；invalid token 表示无剩余工作
    WorkToken claim_next_dynamic(const std::string& device_id,
                                 std::size_t n_active_devices);

    // F-fix 9: 运行时调整动态模式的最大块大小（线程安全）。
    // 用于资源闭环控制：CpuController/MemoryBudget 建议缩小时，
    // 通过此接口让后续 claim_next_dynamic 使用更小的 max_chunk。
    // 仅动态模式生效；max_chunk 会被 clamp 到 [dyn_min_chunk_, +inf)。
    void set_dynamic_max_chunk(std::size_t max_chunk) noexcept;

    // ===== 标记完成（验证 generation）=====
    bool mark_done(std::size_t id, std::uint64_t generation);
    // 兼容旧接口（不验证 generation，用于旧测试过渡）
    void mark_done(std::size_t id);

    // ===== 标记失败（验证 generation）=====
    bool mark_failed(std::size_t id, std::uint64_t generation);
    // 兼容旧接口
    void mark_failed(std::size_t id);

    // ===== 回收失败块 =====
    // 将所有 Failed 块重新设为 Pending（递增 generation）
    std::size_t reclaim_failed();

    // ===== 统计（原子计数器，无遍历）=====
    std::size_t total_blocks() const noexcept;
    std::size_t pending_count() const noexcept;
    std::size_t claimed_count() const noexcept;
    std::size_t done_count() const noexcept;
    std::size_t failed_count() const noexcept;

    // 检查是否全部完成
    bool all_done() const noexcept;

    // 检查是否没有可领取的块（无 Pending 且无 Failed）
    bool no_work_left() const noexcept;

    // 获取块状态（只读，用于测试验证）
    WorkBlockStatus slot_status(std::size_t id) const;
    std::size_t slot_begin(std::size_t id) const;
    std::size_t slot_end(std::size_t id) const;
    std::uint64_t slot_generation(std::size_t id) const;

    // 获取 done 位图（与 CoverageBitmap 兼容）
    std::vector<bool> done_bitmap() const;

    // ===== 动态 chunk 大小 =====
    std::size_t suggest_next_chunk(std::size_t n_active_devices,
                                    std::size_t min_chunk,
                                    std::size_t max_chunk) const noexcept;

    // 是否为动态模式
    bool is_dynamic() const noexcept { return dynamic_mode_; }

    // 剩余工作量（动态模式）
    std::size_t remaining_work() const noexcept;

    // 实际使用的槽位数（动态模式）
    std::size_t active_slot_count() const noexcept;

    // ===== 重置 =====
    void reset();

private:
    // 预分配槽位（reserve 后不再增长，地址稳定）
    // 使用 unique_ptr 包裹，避免 vector<WorkSlot> 的 WorkSlot 不可移动问题
    std::vector<std::unique_ptr<WorkSlot>> slots_;
    std::size_t capacity_{0};               // 预分配槽位数

    // 固定模式
    std::size_t total_blocks_{0};            // 实际块数

    // 固定模式 claim 游标
    std::atomic<std::size_t> next_claim_index_{0};

    // 动态模式状态
    bool dynamic_mode_{false};
    std::size_t range_begin_{0};
    std::size_t range_end_{0};
    std::size_t dyn_min_chunk_{1};
    // F-fix 9: 改为 atomic 以支持运行时通过 set_dynamic_max_chunk 调整
    std::atomic<std::size_t> dyn_max_chunk_{65536};
    std::atomic<std::size_t> dyn_cursor_{0};     // 工作分配游标（range 内位置）
    std::atomic<std::size_t> next_slot_{0};      // 下一个可用槽位索引

    // 原子统计（无遍历）
    std::atomic<std::size_t> done_count_{0};
    std::atomic<std::size_t> failed_count_{0};
    std::atomic<std::size_t> claimed_count_{0};

    // reclaim_failed 需要遍历（加锁保护）
    mutable std::mutex reclaim_mtx_;

    // 获取槽位指针（边界检查）
    WorkSlot* get_slot(std::size_t id);
    const WorkSlot* get_slot(std::size_t id) const;
};

} // namespace astro::compute::scheduler
