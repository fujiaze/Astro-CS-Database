// lib/acr/scheduler/shared_work_pool.cpp — 共享工作池实现（23 号计划 §2 重写）
//
// 与旧实现的差异：
//   - WorkToken 按值返回 {id, begin, end, claimant(DeviceId), attempt}；
//   - 槽位预分配（slot[i].id == i），claim 时填充范围，不再返回容器内部指针；
//   - 动态模式：CAS cursor 推进 + requested_items（调用方 CostEstimate 给出），
//     尾部按 remaining 收缩；GPU 数量不再由池内部折算块大小；
//   - 完成判据：cursor>=end && inflight==0 && retry_queue.empty()
//     && failed_terminal==0 && completed_items==end-begin。
#include "shared_work_pool.hpp"

#include <algorithm>
#include <cassert>
#include <memory>

namespace astro::compute::scheduler {

// ===== 槽位访问（预分配，越界返回 nullptr）=====
WorkSlot* SharedWorkPool::get_slot(std::size_t id) noexcept {
    if (id >= slots_.size()) return nullptr;
    return slots_[id].get();
}

const WorkSlot* SharedWorkPool::get_slot(std::size_t id) const noexcept {
    if (id >= slots_.size()) return nullptr;
    return slots_[id].get();
}

SharedWorkPool::~SharedWorkPool() = default;

// ===== 固定模式初始化 =====
void SharedWorkPool::init(std::size_t begin, std::size_t end,
                          std::size_t chunk_size) {
    reset();
    if (begin >= end || chunk_size == 0) return;

    const std::size_t range = end - begin;
    const std::size_t n_blocks = (range + chunk_size - 1) / chunk_size;
    slots_.reserve(n_blocks);
    for (std::size_t i = 0; i < n_blocks; ++i) {
        auto slot = std::make_unique<WorkSlot>();
        slot->id = i;
        const std::size_t b = begin + i * chunk_size;
        const std::size_t e = std::min(b + chunk_size, end);
        slot->begin.store(b, std::memory_order_relaxed);
        slot->end.store(e, std::memory_order_relaxed);
        slot->state.store(0, std::memory_order_relaxed);  // Pending + attempt 0
        slot->claimant.store(kHwInvalidDeviceId, std::memory_order_relaxed);
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

    dyn_min_chunk_ = (min_chunk == 0) ? 1 : min_chunk;
    std::size_t eff_max = (max_chunk == 0) ? dyn_min_chunk_ : max_chunk;
    if (dyn_min_chunk_ > eff_max) dyn_min_chunk_ = eff_max;
    dyn_max_chunk_.store(eff_max, std::memory_order_relaxed);

    range_begin_ = begin;
    range_end_ = end;

    // 预分配最大可能槽位数（按 min_chunk 拆分）
    const std::size_t range = end - begin;
    std::size_t max_slots = (range + dyn_min_chunk_ - 1) / dyn_min_chunk_;
    if (max_slots == 0) max_slots = 1;
    slots_.reserve(max_slots);
    for (std::size_t i = 0; i < max_slots; ++i) {
        auto slot = std::make_unique<WorkSlot>();
        slot->id = i;
        slot->state.store(0, std::memory_order_relaxed);  // Pending + attempt 0
        slot->claimant.store(kHwInvalidDeviceId, std::memory_order_relaxed);
        slots_.push_back(std::move(slot));
    }
    capacity_ = max_slots;
    total_blocks_ = max_slots;  // 动态模式下 total 语义为 capacity
    dynamic_mode_ = true;
    dyn_cursor_.store(begin, std::memory_order_relaxed);
    next_slot_.store(0, std::memory_order_relaxed);
}

// ===== 固定模式领取 =====
WorkToken SharedWorkPool::claim_next(DeviceId device) {
    if (dynamic_mode_ || slots_.empty()) return WorkToken{};

    const std::size_t n = slots_.size();
    std::size_t start = next_claim_index_.load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t idx = (start + i) % n;
        WorkSlot* slot = slots_[idx].get();
        if (!slot) continue;
        const std::size_t b = slot->begin.load(std::memory_order_relaxed);
        const std::size_t e = slot->end.load(std::memory_order_relaxed);
        const std::uint32_t attempt = slot->try_claim(device, b, e);
        if (attempt != 0) {
            next_claim_index_.store(idx + 1, std::memory_order_relaxed);
            claimed_count_.fetch_add(1, std::memory_order_relaxed);
            inflight_count_.fetch_add(1, std::memory_order_relaxed);

            WorkToken token;
            token.id = slot->id;
            token.begin = b;
            token.end = e;
            token.claimant = device;
            token.attempt = attempt;
            return token;
        }
    }
    return WorkToken{};  // 无 Pending 块
}

// ===== 从 retry queue 弹出一个失败块（锁内 pop）=====
bool SharedWorkPool::pop_retry(RetryEntry& out) {
    std::lock_guard<std::mutex> lk(retry_mtx_);
    if (retry_queue_.empty()) return false;
    out = retry_queue_.back();
    retry_queue_.pop_back();
    return true;
}

// ===== 动态模式领取 =====
WorkToken SharedWorkPool::claim_next_dynamic(DeviceId device,
                                              std::size_t requested_items) {
    if (!dynamic_mode_) return WorkToken{};

    // 1. 优先处理 retry queue（失败回收后由任意设备重新领取）
    RetryEntry entry;
    while (pop_retry(entry)) {
        WorkSlot* slot = get_slot(entry.slot_id);
        if (!slot) continue;
        const std::uint32_t attempt = slot->try_reclaim_claim(device);
        if (attempt != 0) {
            claimed_count_.fetch_add(1, std::memory_order_relaxed);
            retry_pending_.fetch_sub(1, std::memory_order_relaxed);
            inflight_count_.fetch_add(1, std::memory_order_relaxed);

            WorkToken token;
            token.id = slot->id;
            token.begin = entry.begin;
            token.end = entry.end;
            token.claimant = device;
            token.attempt = attempt;
            return token;
        }
        // 竞争失败（其他线程已领取）：继续处理下一个条目
    }

    // 2. CAS cursor 领取新范围
    std::size_t cursor = dyn_cursor_.load(std::memory_order_relaxed);
    while (true) {
        if (cursor >= range_end_) return WorkToken{};  // 无剩余工作

        const std::size_t remaining = range_end_ - cursor;
        std::size_t chunk = requested_items;
        if (chunk == 0) chunk = dyn_min_chunk_;
        if (chunk > remaining) chunk = remaining;
        // 尾部收缩：剩余 < 2*min_chunk 时直接取剩余（避免碎块）
        if (remaining < dyn_min_chunk_ * 2) chunk = remaining;
        const std::size_t max_c = dyn_max_chunk_.load(std::memory_order_relaxed);
        if (chunk > max_c) chunk = max_c;
        if (chunk < dyn_min_chunk_) chunk = dyn_min_chunk_;
        if (chunk > remaining) chunk = remaining;
        if (chunk == 0) return WorkToken{};  // 不应发生（remaining>0 且 min>=1）

        const std::size_t new_cursor = cursor + chunk;
        if (dyn_cursor_.compare_exchange_weak(cursor, new_cursor,
                                               std::memory_order_acq_rel)) {
            const std::size_t slot_idx = next_slot_.fetch_add(1, std::memory_order_acq_rel);
            if (slot_idx >= slots_.size()) {
                // 数学上不可达（capacity = ceil(range/min_chunk) 且 chunk>=min_chunk）。
                // 防御性处理：不再领取；all_done 会因 completed_items != total
                // 而保持 false，调用方可检测漏算（不静默成功）。
                assert(false && "work pool slot capacity exceeded");
                return WorkToken{};
            }
            WorkSlot* slot = slots_[slot_idx].get();
            const std::uint32_t attempt =
                slot ? slot->try_claim(device, cursor, new_cursor) : 0;
            if (attempt == 0) {
                // 新槽位初始 Pending，理论不会失败；防御性返回
                assert(false && "work pool fresh slot claim failed");
                return WorkToken{};
            }
            claimed_count_.fetch_add(1, std::memory_order_relaxed);
            inflight_count_.fetch_add(1, std::memory_order_relaxed);

            WorkToken token;
            token.id = slot->id;
            token.begin = cursor;
            token.end = new_cursor;
            token.claimant = device;
            token.attempt = attempt;
            return token;
        }
        // CAS 失败：cursor 已更新，重试
    }
}

// ===== 运行时调整动态模式最大块大小 =====
void SharedWorkPool::set_dynamic_max_chunk(std::size_t max_chunk) noexcept {
    if (!dynamic_mode_) return;
    const std::size_t min_c = dyn_min_chunk_;
    if (max_chunk < min_c) max_chunk = min_c;
    dyn_max_chunk_.store(max_chunk, std::memory_order_relaxed);
}

// ===== 标记完成 =====
bool SharedWorkPool::mark_done(const WorkToken& token) {
    WorkSlot* slot = get_slot(token.id);
    if (!slot) return false;
    if (slot->try_mark_done(token.attempt)) {
        done_count_.fetch_add(1, std::memory_order_relaxed);
        completed_items_.fetch_add(token.size(), std::memory_order_relaxed);
        inflight_count_.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }
    return false;
}

bool SharedWorkPool::mark_done(std::size_t id, std::uint32_t attempt) {
    WorkSlot* slot = get_slot(id);
    if (!slot) return false;
    if (slot->try_mark_done(attempt)) {
        const std::size_t b = slot->begin.load(std::memory_order_relaxed);
        const std::size_t e = slot->end.load(std::memory_order_relaxed);
        done_count_.fetch_add(1, std::memory_order_relaxed);
        completed_items_.fetch_add(e - b, std::memory_order_relaxed);
        inflight_count_.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }
    return false;
}

// ===== 标记失败 =====
bool SharedWorkPool::mark_failed(const WorkToken& token, bool retryable) {
    WorkSlot* slot = get_slot(token.id);
    if (!slot) return false;
    if (!slot->try_mark_failed(token.attempt)) return false;
    inflight_count_.fetch_sub(1, std::memory_order_relaxed);
    if (retryable) {
        retry_pending_.fetch_add(1, std::memory_order_relaxed);
        RetryEntry entry;
        entry.slot_id = token.id;
        entry.begin = token.begin;
        entry.end = token.end;
        {
            std::lock_guard<std::mutex> lk(retry_mtx_);
            retry_queue_.push_back(entry);
        }
    } else {
        failed_terminal_.fetch_add(1, std::memory_order_relaxed);
    }
    return true;
}

bool SharedWorkPool::mark_failed(std::size_t id, std::uint32_t attempt,
                                 bool retryable) {
    WorkSlot* slot = get_slot(id);
    if (!slot) return false;
    if (!slot->try_mark_failed(attempt)) return false;
    inflight_count_.fetch_sub(1, std::memory_order_relaxed);
    if (retryable) {
        retry_pending_.fetch_add(1, std::memory_order_relaxed);
        RetryEntry entry;
        entry.slot_id = id;
        entry.begin = slot->begin.load(std::memory_order_relaxed);
        entry.end = slot->end.load(std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lk(retry_mtx_);
            retry_queue_.push_back(entry);
        }
    } else {
        failed_terminal_.fetch_add(1, std::memory_order_relaxed);
    }
    return true;
}

// ===== 批量回收失败块 =====
std::size_t SharedWorkPool::reclaim_failed() {
    std::size_t reclaimed = 0;
    while (true) {
        RetryEntry entry;
        if (!pop_retry(entry)) break;
        WorkSlot* slot = get_slot(entry.slot_id);
        if (!slot) continue;
        if (slot->try_reclaim()) {
            ++reclaimed;
            retry_pending_.fetch_sub(1, std::memory_order_relaxed);
        }
        // 已被其他路径领取（retry claim）——不计入回收
    }
    return reclaimed;
}

// ===== 统计 =====
std::size_t SharedWorkPool::total_blocks() const noexcept {
    if (dynamic_mode_) return active_slot_count();
    return total_blocks_;
}

std::size_t SharedWorkPool::pending_count() const noexcept {
    const std::size_t done = done_count_.load(std::memory_order_relaxed);
    const std::size_t retry = retry_pending_.load(std::memory_order_relaxed);
    const std::size_t terminal = failed_terminal_.load(std::memory_order_relaxed);
    const std::size_t in_flight = inflight_count_.load(std::memory_order_relaxed);
    const std::size_t total = total_blocks();
    const std::size_t used = done + retry + terminal + in_flight;
    return total > used ? (total - used) : 0;
}

std::size_t SharedWorkPool::claimed_count() const noexcept {
    return inflight_count_.load(std::memory_order_relaxed);
}

std::size_t SharedWorkPool::done_count() const noexcept {
    return done_count_.load(std::memory_order_relaxed);
}

std::size_t SharedWorkPool::failed_count() const noexcept {
    return retry_pending_.load(std::memory_order_relaxed) +
           failed_terminal_.load(std::memory_order_relaxed);
}

std::size_t SharedWorkPool::retry_pending_count() const noexcept {
    return retry_pending_.load(std::memory_order_relaxed);
}

std::size_t SharedWorkPool::failed_terminal_count() const noexcept {
    return failed_terminal_.load(std::memory_order_relaxed);
}

std::size_t SharedWorkPool::completed_items() const noexcept {
    return completed_items_.load(std::memory_order_relaxed);
}

// ===== 完成判据（23 号计划 §2.3）=====
bool SharedWorkPool::all_done() const noexcept {
    const std::size_t done = done_count_.load(std::memory_order_relaxed);
    const std::size_t retry = retry_pending_.load(std::memory_order_relaxed);
    const std::size_t terminal = failed_terminal_.load(std::memory_order_relaxed);
    const std::size_t in_flight = inflight_count_.load(std::memory_order_relaxed);

    if (retry != 0) return false;
    if (terminal != 0) return false;
    if (in_flight != 0) return false;

    if (dynamic_mode_) {
        if (dyn_cursor_.load(std::memory_order_acquire) < range_end_) return false;
        const std::size_t total_items = range_end_ - range_begin_;
        return completed_items_.load(std::memory_order_relaxed) == total_items;
    }
    // 固定模式
    const std::size_t total_items =
        total_blocks_ == 0 ? 0 :
        (slots_.back()->end.load(std::memory_order_relaxed) -
         slots_.front()->begin.load(std::memory_order_relaxed));
    if (completed_items_.load(std::memory_order_relaxed) != total_items) return false;
    return done == total_blocks_;
}

bool SharedWorkPool::no_work_left() const noexcept {
    if (dynamic_mode_) {
        if (dyn_cursor_.load(std::memory_order_acquire) < range_end_) return false;
    }
    return pending_count() == 0 &&
           inflight_count_.load(std::memory_order_relaxed) == 0 &&
           retry_pending_.load(std::memory_order_relaxed) == 0 &&
           failed_terminal_.load(std::memory_order_relaxed) == 0;
}

// ===== 只读查询 =====
WorkBlockStatus SharedWorkPool::slot_status(std::size_t id) const {
    const WorkSlot* slot = get_slot(id);
    if (!slot) return WorkBlockStatus::Failed;
    return slot->status();
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

std::uint32_t SharedWorkPool::slot_attempt(std::size_t id) const {
    const WorkSlot* slot = get_slot(id);
    if (!slot) return 0;
    return slot->attempt();
}

DeviceId SharedWorkPool::slot_claimant(std::size_t id) const {
    const WorkSlot* slot = get_slot(id);
    if (!slot) return kHwInvalidDeviceId;
    return slot->claimant.load(std::memory_order_acquire);
}

std::vector<bool> SharedWorkPool::done_bitmap() const {
    const std::size_t n = total_blocks();
    std::vector<bool> bm(n, false);
    for (std::size_t i = 0; i < n && i < slots_.size(); ++i) {
        if (!slots_[i]) continue;
        if (slots_[i]->status() == WorkBlockStatus::Done) {
            bm[i] = true;
        }
    }
    return bm;
}

std::size_t SharedWorkPool::remaining_work() const noexcept {
    if (!dynamic_mode_) return 0;
    const std::size_t cursor = dyn_cursor_.load(std::memory_order_acquire);
    if (cursor >= range_end_) return 0;
    return range_end_ - cursor;
}

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
    dyn_max_chunk_.store(65536, std::memory_order_relaxed);
    dyn_cursor_.store(0, std::memory_order_relaxed);
    next_slot_.store(0, std::memory_order_relaxed);
    next_claim_index_.store(0, std::memory_order_relaxed);
    claimed_count_.store(0, std::memory_order_relaxed);
    inflight_count_.store(0, std::memory_order_relaxed);
    done_count_.store(0, std::memory_order_relaxed);
    retry_pending_.store(0, std::memory_order_relaxed);
    failed_terminal_.store(0, std::memory_order_relaxed);
    completed_items_.store(0, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lk(retry_mtx_);
        retry_queue_.clear();
    }
}

} // namespace astro::compute::scheduler
