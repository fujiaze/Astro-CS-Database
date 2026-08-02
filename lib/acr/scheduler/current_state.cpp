// lib/acr/scheduler/current_state.cpp — CurrentState 实现
// Phase F4：工作保持调度器的运行时状态跟踪。
#include "current_state.hpp"

#include <algorithm>
#include <sstream>

namespace astro::compute::scheduler {

void CurrentState::init_devices(const std::vector<std::string>& backends) {
    std::lock_guard<std::mutex> lk(mtx_);
    devices_.clear();
    devices_.reserve(backends.size());
    for (const auto& b : backends) {
        devices_.emplace_back(b);
    }
}

std::size_t CurrentState::device_count() const noexcept {
    return devices_.size();
}

std::vector<std::string> CurrentState::backends() const {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<std::string> result;
    result.reserve(devices_.size());
    for (const auto& d : devices_) result.push_back(d.backend);
    return result;
}

bool CurrentState::is_available(const std::string& backend) const {
    std::lock_guard<std::mutex> lk(mtx_);
    for (const auto& d : devices_) {
        if (d.backend == backend) return d.available.load(std::memory_order_relaxed);
    }
    return false;
}

DeviceRuntimeState* CurrentState::find_device(const std::string& backend) {
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& d : devices_) {
        if (d.backend == backend) return &d;
    }
    return nullptr;
}

const DeviceRuntimeState* CurrentState::find_device(const std::string& backend) const {
    std::lock_guard<std::mutex> lk(mtx_);
    for (const auto& d : devices_) {
        if (d.backend == backend) return &d;
    }
    return nullptr;
}

std::string CurrentState::pick_finish_shortest(const std::vector<std::string>& candidates) const {
    std::lock_guard<std::mutex> lk(mtx_);
    std::string best;
    std::uint64_t best_finish = std::numeric_limits<std::uint64_t>::max();
    for (const auto& cand : candidates) {
        for (const auto& d : devices_) {
            if (d.backend != cand) continue;
            if (!d.available.load(std::memory_order_relaxed)) continue;
            std::uint64_t ql = d.queue_load_ns.load(std::memory_order_relaxed);
            if (ql < best_finish) {
                best_finish = ql;
                best = d.backend;
            }
        }
    }
    return best;
}

void CurrentState::set_total_chunks(std::size_t n) {
    std::lock_guard<std::mutex> lk(mtx_);
    total_chunks_ = n;
}

std::size_t CurrentState::total_chunks() const noexcept {
    return total_chunks_;
}

std::size_t CurrentState::completed_chunks() const noexcept {
    std::size_t sum = 0;
    for (const auto& d : devices_) {
        sum += d.chunks_completed.load(std::memory_order_relaxed);
    }
    return sum;
}

std::size_t CurrentState::pending_chunks() const noexcept {
    return total_chunks_ > completed_chunks() ? total_chunks_ - completed_chunks() : 0;
}

bool CurrentState::all_done() const noexcept {
    return total_chunks_ > 0 && completed_chunks() >= total_chunks_;
}

double CurrentState::completion_ratio() const noexcept {
    if (total_chunks_ == 0) return 0.0;
    return static_cast<double>(completed_chunks()) / static_cast<double>(total_chunks_);
}

void CurrentState::init_coverage(std::size_t chunk_count) {
    std::lock_guard<std::mutex> lk(mtx_);
    coverage_ = CoverageBitmap(chunk_count);
    total_chunks_ = chunk_count;
}

CoverageBitmap& CurrentState::coverage() noexcept {
    return coverage_;
}

const CoverageBitmap& CurrentState::coverage() const noexcept {
    return coverage_;
}

std::size_t CurrentState::guided_next_chunk_size(std::size_t remaining,
                                                  std::size_t n_workers) const noexcept {
    if (n_workers == 0) n_workers = 1;
    // guided: next = max(1, remaining / (2 × n_workers))
    std::size_t next = remaining / (2 * n_workers);
    if (next == 0) next = 1;
    return next;
}

void CurrentState::reset() {
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& d : devices_) {
        d.queue_load_ns.store(0, std::memory_order_relaxed);
        d.last_finish_ns.store(0, std::memory_order_relaxed);
        d.available.store(true, std::memory_order_relaxed);
        d.chunks_assigned.store(0, std::memory_order_relaxed);
        d.chunks_completed.store(0, std::memory_order_relaxed);
        d.chunks_failed.store(0, std::memory_order_relaxed);
    }
    total_chunks_ = 0;
    coverage_ = CoverageBitmap{};
}

std::string CurrentState::status_json() const {
    std::lock_guard<std::mutex> lk(mtx_);
    std::ostringstream os;
    os << "{";
    os << "\"total_chunks\":" << total_chunks_;
    os << ",\"completed\":" << completed_chunks();
    os << ",\"pending\":" << (total_chunks_ > completed_chunks() ? total_chunks_ - completed_chunks() : 0);
    os << ",\"completion_ratio\":" << completion_ratio();
    os << ",\"devices\":[";
    for (std::size_t i = 0; i < devices_.size(); ++i) {
        if (i > 0) os << ",";
        const auto& d = devices_[i];
        os << "{";
        os << "\"backend\":\"" << d.backend << "\"";
        os << ",\"available\":" << (d.available.load(std::memory_order_relaxed) ? "true" : "false");
        os << ",\"queue_load_ns\":" << d.queue_load_ns.load(std::memory_order_relaxed);
        os << ",\"assigned\":" << d.chunks_assigned.load(std::memory_order_relaxed);
        os << ",\"completed\":" << d.chunks_completed.load(std::memory_order_relaxed);
        os << ",\"failed\":" << d.chunks_failed.load(std::memory_order_relaxed);
        os << "}";
    }
    os << "]}";
    return os.str();
}

} // namespace astro::compute::scheduler
