// lib/acr/scheduler/shared_work_pool.cpp — SharedWorkPool 实现（F-fix 2）
#include "shared_work_pool.hpp"

#include <algorithm>

namespace astro::compute::scheduler {

void SharedWorkPool::init(std::size_t begin, std::size_t end, std::size_t chunk_size) {
    blocks_.clear();
    next_claim_index_.store(0, std::memory_order_relaxed);

    if (begin >= end || chunk_size == 0) return;

    std::size_t id = 0;
    for (std::size_t pos = begin; pos < end; pos += chunk_size) {
        std::size_t chunk_end = std::min(pos + chunk_size, end);
        blocks_.emplace_back(id++, pos, chunk_end);
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

void SharedWorkPool::reset() {
    blocks_.clear();
    next_claim_index_.store(0, std::memory_order_relaxed);
}

} // namespace astro::compute::scheduler
