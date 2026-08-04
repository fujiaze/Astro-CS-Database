// lib/acr/scheduler/shared_work_pool.cpp — 共享工作池实现（F-fix 5 重构）
//
// F-fix 5: 预分配槽位 + WorkToken 值令牌
//   - 固定模式：init 时预创建所有块
//   - 动态模式：init_dynamic 预分配最大槽位数，claim 时填充 begin/end
//   - 所有状态转换用 atomic CAS
//   - 统计用原子计数器，不遍历 vector
#include "shared_work_pool.hpp"

#include <algorithm>
#include <cstring>
#include <memory>

namespace astro::compute::scheduler {

// ===== 内部：获取槽位指针 =====
WorkSlot* SharedWorkPool::get_slot(std::size_t id) {
    if (id >= slots_.size()) return nullptr;
    return slots_[id].get();
}

const WorkSlot* SharedWorkPool::get_slot(std::size_t id) const {
    if (id >= slots_.size()) return nullptr;
    return slots_[id].get();
}

SharedWorkPool::~SharedWorkPool() = default;

// ===== 固定模式初始化 =====
void SharedWorkPool::init(std::size_t begin, std::size_t end, std::size_t chunk_size) {
    reset();

    if (begin >= end || chunk_size == 0) {
        return;
    }

    // 计算块数
    std::size_t range = end - begin;
    std::size_t n_blocks = (range + chunk_size - 1) / chunk_size;

    // 预分配所有槽位
    slots_.reserve(n_blocks);
    for (std::size_t i = 0; i < n_blocks; ++i) {
        auto slot = std::make_unique<WorkSlot>();
        slot->id = i;
        std::size_t b = begin + i * chunk_size;
        std::size_t e = std::min(b + chunk_size, end);
        slot->begin.store(b, std::memory_order_relaxed);
        slot->end.store(e, std::memory_order_relaxed);
        slot->status.store(WorkBlockStatus::Pending, std::memory_order_relaxed);
        slot->generation.store(0, std::memory_order_relaxed);
        slot->attempt_count.store(0, std::memory_order_relaxed);
        slot->claimed_device.store(nullptr, std::memory_order_relaxed);
        slots_.push_back(std::move(slot));
    }

    capacity_ = n_blocks;
    total_blocks_ = n_blocks;
    dynamic_mode_ = false;
    next_claim_index_.store(0, std::memory_order_relaxed);
}

// ===== 动态模式初始化 =====
void SharedWorkPool::init_dynamic(std::size_t begin, std::size_t end,
                                   std::size_t min_chunk, std::size_t max_chunk) {
    reset();

    if (begin >= end) return;

    // 规范化 min <= max
    dyn_min_chunk_ = (min_chunk == 0) ? 1 : min_chunk;
    dyn_max_chunk_ = (max_chunk == 0) ? dyn_min_chunk_ : max_chunk;
    if (dyn_min_chunk_ > dyn_max_chunk_) {
        dyn_min_chunk_ = dyn_max_chunk_;
    }

    range_begin_ = begin;
    range_end_ = end;

    // 预分配最大可能槽位数
    std::size_t range = end - begin;
    std::size_t max_slots = (range + dyn_min_chunk_ - 1) / dyn_min_chunk_;
    if (max_slots == 0) max_slots = 1;

    slots_.reserve(max_slots);
    for (std::size_t i = 0; i < max_slots; ++i) {
        auto slot = std::make_unique<WorkSlot>();
        slot->id = i;
        // begin/end 在 claim 时填充
        slot->status.store(WorkBlockStatus::Pending, std::memory_order_relaxed);
        slot->generation.store(0, std::memory_order_relaxed);
        slot->attempt_count.store(0, std::memory_order_relaxed);
        slot->claimed_device.store(nullptr, std::memory_order_relaxed);
        slots_.push_back(std::move(slot));
    }

    capacity_ = max_slots;
    total_blocks_ = max_slots;  // 动态模式下 total = capacity
    dynamic_mode_ = true;
    dyn_cursor_.store(begin, std::memory_order_relaxed);
    next_slot_.store(0, std::memory_order_relaxed);
}

// ===== 固定模式 claim =====
WorkToken SharedWorkPool::claim_next(const std::string& device_id) {
    if (dynamic_mode_ || slots_.empty()) {
        return WorkToken{};  // 无效 token
    }

    // 线性扫描找 Pending 块（CAS 保证每个块只被 claim 一次）
    // 使用原子递增的游标优化：从上次扫描位置开始
    std::size_t start = next_claim_index_.load(std::memory_order_relaxed);
    std::size_t n = slots_.size();

    for (std::size_t i = 0; i < n; ++i) {
        std::size_t idx = (start + i) % n;
        WorkSlot* slot = slots_[idx].get();
        if (!slot) continue;

        std::uint64_t gen = slot->generation.load(std::memory_order_acquire);
        if (slot->try_claim(device_id.c_str(), gen)) {
            next_claim_index_.store(idx + 1, std::memory_order_relaxed);
            claimed_count_.fetch_add(1, std::memory_order_relaxed);

            WorkToken token;
            token.id = slot->id;
            token.begin = slot->begin.load(std::memory_order_acquire);
            token.end = slot->end.load(std::memory_order_acquire);
            token.generation = gen;
            token.device_id = device_id;
            return token;
        }
    }

    return WorkToken{};  // 无 Pending 块
}

// ===== 动态模式 claim =====
WorkToken SharedWorkPool::claim_next_dynamic(const std::string& device_id,
                                              std::size_t n_active_devices) {
    if (!dynamic_mode_) {
        return WorkToken{};
    }

    // 原子递增 cursor 获取工作范围
    std::size_t cursor = dyn_cursor_.load(std::memory_order_relaxed);
    while (true) {
        // 检查是否还有剩余工作
        if (cursor >= range_end_) {
            return WorkToken{};  // 无剩余工作
        }

        // 计算块大小（guided scheduling）
        std::size_t remaining = range_end_ - cursor;
        std::size_t n_dev = (n_active_devices == 0) ? 1 : n_active_devices;
        std::size_t chunk = remaining / (2 * n_dev);
        // 尾部收缩：剩余小于 2*min_chunk 时用 min_chunk
        if (remaining < dyn_min_chunk_ * 2) chunk = dyn_min_chunk_;
        if (chunk < dyn_min_chunk_) chunk = dyn_min_chunk_;
        if (chunk > dyn_max_chunk_) chunk = dyn_max_chunk_;

        std::size_t new_cursor = cursor + chunk;
        if (new_cursor > range_end_) new_cursor = range_end_;

        // CAS 递增 cursor
        if (dyn_cursor_.compare_exchange_weak(cursor, new_cursor,
                                               std::memory_order_acq_rel)) {
            // cursor 获取成功，分配槽位
            std::size_t slot_idx = next_slot_.fetch_add(1, std::memory_order_acq_rel);
            if (slot_idx >= slots_.size()) {
                // 不应该发生（预分配了足够的槽位），但安全处理
                return WorkToken{};
            }

            WorkSlot* slot = slots_[slot_idx].get();
            if (!slot) return WorkToken{};

            // 填充 begin/end
            slot->begin.store(cursor, std::memory_order_relaxed);
            slot->end.store(new_cursor, std::memory_order_relaxed);

            // CAS 状态 Pending → Claimed
            std::uint64_t gen = slot->generation.load(std::memory_order_acquire);
            if (slot->try_claim(device_id.c_str(), gen)) {
                claimed_count_.fetch_add(1, std::memory_order_relaxed);

                WorkToken token;
                token.id = slot->id;
                token.begin = cursor;
                token.end = new_cursor;
                token.generation = gen;
                token.device_id = device_id;
                return token;
            } else {
                // 理论上不会失败（预分配槽位，初始 Pending）
                // 如果发生，回退 cursor
                return WorkToken{};
            }
        }
        // CAS 失败，cursor 已更新，重试
    }
}

// ===== 标记完成 =====
bool SharedWorkPool::mark_done(std::size_t id, std::uint64_t generation) {
    WorkSlot* slot = get_slot(id);
    if (!slot) return false;

    if (slot->try_mark_done(generation)) {
        done_count_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    return false;
}

void SharedWorkPool::mark_done(std::size_t id) {
    WorkSlot* slot = get_slot(id);
    if (!slot) return;

    // 旧接口：不验证 generation，直接尝试 Claimed→Done
    WorkBlockStatus expected = WorkBlockStatus::Claimed;
    if (slot->status.compare_exchange_strong(expected, WorkBlockStatus::Done,
                                              std::memory_order_acq_rel)) {
        done_count_.fetch_add(1, std::memory_order_relaxed);
    }
}

// ===== 标记失败 =====
bool SharedWorkPool::mark_failed(std::size_t id, std::uint64_t generation) {
    WorkSlot* slot = get_slot(id);
    if (!slot) return false;

    if (slot->try_mark_failed(generation)) {
        failed_count_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    return false;
}

void SharedWorkPool::mark_failed(std::size_t id) {
    WorkSlot* slot = get_slot(id);
    if (!slot) return;

    WorkBlockStatus expected = WorkBlockStatus::Claimed;
    if (slot->status.compare_exchange_strong(expected, WorkBlockStatus::Failed,
                                              std::memory_order_acq_rel)) {
        failed_count_.fetch_add(1, std::memory_order_relaxed);
    }
}

// ===== 回收失败块 =====
std::size_t SharedWorkPool::reclaim_failed() {
    std::lock_guard<std::mutex> lk(reclaim_mtx_);
    std::size_t reclaimed = 0;

    for (auto& slot_ptr : slots_) {
        if (!slot_ptr) continue;
        if (slot_ptr->try_reclaim()) {
            reclaimed++;
            failed_count_.fetch_sub(1, std::memory_order_relaxed);
            // reclaim 把块从 Failed→Pending，不再被 claim
            // 递减 claimed_count_ 使 pending_count 正确
            claimed_count_.fetch_sub(1, std::memory_order_relaxed);
        }
    }

    return reclaimed;
}

// ===== 统计 =====
std::size_t SharedWorkPool::total_blocks() const noexcept {
    if (dynamic_mode_) {
        return active_slot_count();
    }
    return total_blocks_;
}

std::size_t SharedWorkPool::pending_count() const noexcept {
    std::size_t claimed = claimed_count_.load(std::memory_order_relaxed);
    std::size_t done = done_count_.load(std::memory_order_relaxed);
    std::size_t failed = failed_count_.load(std::memory_order_relaxed);
    std::size_t total = dynamic_mode_ ? active_slot_count() : total_blocks_;
    // pending = total - claimed（已 claim 的包括 done 和 failed）
    // 但 claimed_count 在 claim 时递增，done/failed 时不清零
    // 所以 claimed = done + failed + (claimed 但未完成)
    // pending = total - (done + failed + in_progress)
    // in_progress = claimed - done - failed
    std::size_t in_progress = (claimed > done + failed) ? (claimed - done - failed) : 0;
    std::size_t used = done + failed + in_progress;
    return (total > used) ? (total - used) : 0;
}

std::size_t SharedWorkPool::claimed_count() const noexcept {
    std::size_t done = done_count_.load(std::memory_order_relaxed);
    std::size_t failed = failed_count_.load(std::memory_order_relaxed);
    std::size_t claimed = claimed_count_.load(std::memory_order_relaxed);
    std::size_t in_progress = (claimed > done + failed) ? (claimed - done - failed) : 0;
    return in_progress;
}

std::size_t SharedWorkPool::done_count() const noexcept {
    return done_count_.load(std::memory_order_relaxed);
}

std::size_t SharedWorkPool::failed_count() const noexcept {
    return failed_count_.load(std::memory_order_relaxed);
}

bool SharedWorkPool::all_done() const noexcept {
    std::size_t done = done_count_.load(std::memory_order_relaxed);
    std::size_t failed = failed_count_.load(std::memory_order_relaxed);
    // 无失败块
    if (failed > 0) return false;
    // 动态模式：cursor 到达 range_end 且所有已使用槽位完成
    if (dynamic_mode_) {
        if (dyn_cursor_.load(std::memory_order_acquire) < range_end_) return false;
        std::size_t active = active_slot_count();
        return (done == active) && (active > 0 || range_begin_ == range_end_);
    }
    // 固定模式：done == total_blocks_
    return (done == total_blocks_) && (total_blocks_ > 0 || total_blocks_ == 0);
}

bool SharedWorkPool::no_work_left() const noexcept {
    if (dynamic_mode_) {
        std::size_t cursor = dyn_cursor_.load(std::memory_order_acquire);
        if (cursor < range_end_) return false;
        // cursor 到达 end，检查是否还有 Pending/Failed 块
        std::size_t done = done_count_.load(std::memory_order_relaxed);
        std::size_t failed = failed_count_.load(std::memory_order_relaxed);
        std::size_t claimed = claimed_count_.load(std::memory_order_relaxed);
        std::size_t in_progress = (claimed > done + failed) ? (claimed - done - failed) : 0;
        return (in_progress == 0) && (failed == 0);
    }
    // 固定模式：无 Pending 且无 Failed
    return pending_count() == 0 && failed_count() == 0;
}

// ===== 只读查询（用于测试）=====
WorkBlockStatus SharedWorkPool::slot_status(std::size_t id) const {
    const WorkSlot* slot = get_slot(id);
    if (!slot) return WorkBlockStatus::Failed;  // 不存在的槽位视为 Failed
    return slot->status.load(std::memory_order_acquire);
}

std::size_t SharedWorkPool::slot_begin(std::size_t id) const {
    const WorkSlot* slot = get_slot(id);
    if (!slot) return 0;
    return slot->begin.load(std::memory_order_acquire);
}

std::size_t SharedWorkPool::slot_end(std::size_t id) const {
    const WorkSlot* slot = get_slot(id);
    if (!slot) return 0;
    return slot->end.load(std::memory_order_acquire);
}

std::uint64_t SharedWorkPool::slot_generation(std::size_t id) const {
    const WorkSlot* slot = get_slot(id);
    if (!slot) return 0;
    return slot->generation.load(std::memory_order_acquire);
}

// ===== done 位图 =====
std::vector<bool> SharedWorkPool::done_bitmap() const {
    std::size_t n = dynamic_mode_ ? active_slot_count() : total_blocks_;
    std::vector<bool> bm(n, false);
    for (std::size_t i = 0; i < n && i < slots_.size(); ++i) {
        if (!slots_[i]) continue;
        if (slots_[i]->status.load(std::memory_order_acquire) == WorkBlockStatus::Done) {
            bm[i] = true;
        }
    }
    return bm;
}

// ===== 动态 chunk 建议 =====
std::size_t SharedWorkPool::suggest_next_chunk(std::size_t n_active_devices,
                                                std::size_t min_chunk,
                                                std::size_t max_chunk) const noexcept {
    std::size_t remaining = remaining_work();
    // 固定模式下 remaining_work() 返回 0，用 pending_count 作为剩余工作估算
    if (remaining == 0 && !dynamic_mode_) {
        remaining = pending_count();
    }
    if (remaining == 0) return 0;

    std::size_t n_dev = (n_active_devices == 0) ? 1 : n_active_devices;
    std::size_t chunk = remaining / (2 * n_dev);
    std::size_t eff_min = (min_chunk == 0) ? 1 : min_chunk;
    if (chunk < eff_min) chunk = eff_min;
    if (chunk > max_chunk) chunk = max_chunk;
    return chunk;
}

// ===== 剩余工作量 =====
std::size_t SharedWorkPool::remaining_work() const noexcept {
    if (!dynamic_mode_) return 0;
    std::size_t cursor = dyn_cursor_.load(std::memory_order_acquire);
    if (cursor >= range_end_) return 0;
    return range_end_ - cursor;
}

// ===== 实际使用的槽位数 =====
std::size_t SharedWorkPool::active_slot_count() const noexcept {
    if (!dynamic_mode_) return total_blocks_;
    return next_slot_.load(std::memory_order_acquire);
}

// ===== 重置 =====
void SharedWorkPool::reset() {
    slots_.clear();
    capacity_ = 0;
    total_blocks_ = 0;
    dynamic_mode_ = false;
    range_begin_ = 0;
    range_end_ = 0;
    dyn_min_chunk_ = 1;
    dyn_max_chunk_ = 65536;
    dyn_cursor_.store(0, std::memory_order_relaxed);
    next_slot_.store(0, std::memory_order_relaxed);
    next_claim_index_.store(0, std::memory_order_relaxed);
    done_count_.store(0, std::memory_order_relaxed);
    failed_count_.store(0, std::memory_order_relaxed);
    claimed_count_.store(0, std::memory_order_relaxed);
}

} // namespace astro::compute::scheduler
