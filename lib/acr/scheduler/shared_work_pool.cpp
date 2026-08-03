// lib/acr/scheduler/shared_work_pool.cpp — SharedWorkPool 实现（F-fix 2）
#include "shared_work_pool.hpp"

#include <algorithm>

namespace astro::compute::scheduler {

void SharedWorkPool::init(std::size_t begin, std::size_t end, std::size_t chunk_size) {
    blocks_.clear();
    next_claim_index_.store(0, std::memory_order_relaxed);
    dynamic_mode_ = false;

    if (begin >= end || chunk_size == 0) return;

    std::size_t id = 0;
    for (std::size_t pos = begin; pos < end; pos += chunk_size) {
        std::size_t chunk_end = std::min(pos + chunk_size, end);
        blocks_.emplace_back(id++, pos, chunk_end);
    }
}

// F-fix 3：动态初始化
void SharedWorkPool::init_dynamic(std::size_t begin, std::size_t end,
                                   std::size_t min_chunk, std::size_t max_chunk) {
    blocks_.clear();
    next_claim_index_.store(0, std::memory_order_relaxed);
    dynamic_mode_ = true;
    range_begin_ = begin;
    range_end_ = end;
    dyn_min_chunk_ = (min_chunk == 0) ? 1 : min_chunk;
    dyn_max_chunk_ = (max_chunk == 0) ? dyn_min_chunk_ : max_chunk;
    // 规范化：确保 min <= max（避免 min=256/max=25 矛盾配置导致块覆盖整个 range）
    if (dyn_min_chunk_ > dyn_max_chunk_) {
        dyn_min_chunk_ = dyn_max_chunk_;
    }
    dyn_cursor_.store(begin, std::memory_order_relaxed);
    dyn_next_id_.store(0, std::memory_order_relaxed);

    // 预分配空间（避免动态 push_back 时重分配）
    if (end > begin && dyn_min_chunk_ > 0) {
        std::size_t max_blocks = (end - begin + dyn_min_chunk_ - 1) / dyn_min_chunk_;
        blocks_.reserve(max_blocks);
    }
}

WorkBlock* SharedWorkPool::claim_next(const std::string& device_id) {
    // 原子扫描：从 next_claim_index 开始找第一个 Pending 块
    std::size_t idx = next_claim_index_.load(std::memory_order_acquire);
    std::size_t n = blocks_.size();

    for (std::size_t i = 0; i < n; ++i) {
        std::size_t check_idx = (idx + i) % n;
        if (blocks_[check_idx].try_claim(device_id)) {
            // 更新 next_claim_index（允许回绕，但避免每次从头扫描）
            next_claim_index_.store((check_idx + 1) % n, std::memory_order_release);
            return &blocks_[check_idx];
        }
    }
    return nullptr;  // 无 Pending 块
}

// F-fix 3：动态领取下一块
WorkBlock* SharedWorkPool::claim_next_dynamic(const std::string& device_id,
                                               std::size_t n_active_devices) {
    if (!dynamic_mode_) return nullptr;

    // 计算剩余工作
    std::size_t cursor = dyn_cursor_.load(std::memory_order_acquire);
    if (cursor >= range_end_) return nullptr;

    // guided: chunk = clamp(remaining / (2 * n_devices), min, max)
    std::size_t remaining = range_end_ - cursor;
    std::size_t n_dev = (n_active_devices == 0) ? 1 : n_active_devices;
    std::size_t chunk = remaining / (2 * n_dev);
    // 尾部收缩：剩余小于 2*min_chunk 时用 min_chunk（必须在 max clamp 之前）
    if (remaining < dyn_min_chunk_ * 2) chunk = dyn_min_chunk_;
    if (chunk < dyn_min_chunk_) chunk = dyn_min_chunk_;
    if (chunk > dyn_max_chunk_) chunk = dyn_max_chunk_;

    // 原子推进 cursor
    std::size_t old_cursor = dyn_cursor_.fetch_add(chunk, std::memory_order_acq_rel);
    if (old_cursor >= range_end_) return nullptr;  // 已被其他 worker 领完

    std::size_t block_end = std::min(old_cursor + chunk, range_end_);
    if (block_end <= old_cursor) return nullptr;

    // 创建新块（mutex 保护 push_back）
    std::size_t block_id = dyn_next_id_.fetch_add(1, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lk(dyn_mtx_);
        blocks_.emplace_back(block_id, old_cursor, block_end);
        auto& block = blocks_.back();
        block.try_claim(device_id);
        return &blocks_.back();
    }
}

void SharedWorkPool::mark_done(std::size_t id) {
    if (id < blocks_.size()) {
        blocks_[id].try_mark_done();
    }
}

void SharedWorkPool::mark_failed(std::size_t id) {
    if (id < blocks_.size()) {
        blocks_[id].try_mark_failed();
    }
}

std::size_t SharedWorkPool::reclaim_failed() {
    std::lock_guard<std::mutex> lk(mtx_);
    std::size_t count = 0;
    for (auto& b : blocks_) {
        if (b.try_reclaim()) {
            ++count;
        }
    }
    if (count > 0) {
        next_claim_index_.store(0, std::memory_order_relaxed);
    }
    return count;
}

std::size_t SharedWorkPool::total_blocks() const noexcept {
    return blocks_.size();
}

std::size_t SharedWorkPool::pending_count() const noexcept {
    std::size_t c = 0;
    for (const auto& b : blocks_) {
        if (b.status.load(std::memory_order_relaxed) == WorkBlockStatus::Pending) ++c;
    }
    return c;
}

std::size_t SharedWorkPool::claimed_count() const noexcept {
    std::size_t c = 0;
    for (const auto& b : blocks_) {
        if (b.status.load(std::memory_order_relaxed) == WorkBlockStatus::Claimed) ++c;
    }
    return c;
}

std::size_t SharedWorkPool::done_count() const noexcept {
    std::size_t c = 0;
    for (const auto& b : blocks_) {
        if (b.status.load(std::memory_order_relaxed) == WorkBlockStatus::Done) ++c;
    }
    return c;
}

std::size_t SharedWorkPool::failed_count() const noexcept {
    std::size_t c = 0;
    for (const auto& b : blocks_) {
        if (b.status.load(std::memory_order_relaxed) == WorkBlockStatus::Failed) ++c;
    }
    return c;
}

bool SharedWorkPool::all_done() const noexcept {
    if (blocks_.empty()) return true;  // 空范围视为全部完成
    for (const auto& b : blocks_) {
        if (b.status.load(std::memory_order_relaxed) != WorkBlockStatus::Done) return false;
    }
    return true;
}

bool SharedWorkPool::no_work_left() const noexcept {
    for (const auto& b : blocks_) {
        auto s = b.status.load(std::memory_order_relaxed);
        if (s == WorkBlockStatus::Pending || s == WorkBlockStatus::Failed) return false;
    }
    return true;
}

std::vector<bool> SharedWorkPool::done_bitmap() const {
    std::vector<bool> bm(blocks_.size(), false);
    for (std::size_t i = 0; i < blocks_.size(); ++i) {
        if (blocks_[i].status.load(std::memory_order_relaxed) == WorkBlockStatus::Done) {
            bm[i] = true;
        }
    }
    return bm;
}

std::size_t SharedWorkPool::suggest_next_chunk(std::size_t n_active_devices,
                                                std::size_t min_chunk,
                                                std::size_t max_chunk) const noexcept {
    if (blocks_.empty() || n_active_devices == 0) return min_chunk;
    std::size_t remaining = pending_count();
    if (remaining == 0) return min_chunk;
    // guided: next = remaining / (2 * n_devices)
    std::size_t suggested = remaining / (2 * n_active_devices);
    if (suggested < min_chunk) suggested = min_chunk;
    if (suggested > max_chunk) suggested = max_chunk;
    return suggested;
}

// F-fix 3：剩余工作量（动态模式）
std::size_t SharedWorkPool::remaining_work() const noexcept {
    if (!dynamic_mode_) return 0;
    std::size_t cursor = dyn_cursor_.load(std::memory_order_relaxed);
    if (cursor >= range_end_) return 0;
    return range_end_ - cursor;
}

void SharedWorkPool::reset() {
    blocks_.clear();
    next_claim_index_.store(0, std::memory_order_relaxed);
}

} // namespace astro::compute::scheduler
