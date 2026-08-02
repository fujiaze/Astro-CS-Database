// lib/acr/utilization/actual_tracker.cpp — ActualTracker 实现
#include "actual_tracker.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <limits>
#include <mutex>
#include <sstream>
#include <vector>

namespace astro::compute::utilization {

struct ActualTracker::Impl {
    mutable std::mutex mtx;
    std::size_t capacity{1024};
    std::vector<UtilizationSample> samples;
    // 统计累积（atomic 加速）
    std::atomic<std::size_t> total_count{0};
    std::atomic<double> sum_error{0.0};
    std::atomic<double> max_error{std::numeric_limits<double>::lowest()};
    std::atomic<double> min_error{std::numeric_limits<double>::max()};
};

ActualTracker::ActualTracker() : impl_(std::make_unique<Impl>()) {}
ActualTracker::~ActualTracker() = default;

void ActualTracker::set_capacity(std::size_t cap) noexcept {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->capacity = cap;
    if (impl_->samples.size() > cap) {
        impl_->samples.erase(impl_->samples.begin(),
                              impl_->samples.end() - static_cast<std::ptrdiff_t>(cap));
    }
}

void ActualTracker::record(const UtilizationSample& sample) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->samples.push_back(sample);
    if (impl_->samples.size() > impl_->capacity) {
        impl_->samples.erase(impl_->samples.begin());
    }
    impl_->total_count.fetch_add(1, std::memory_order_relaxed);
    // 更新统计（不严格一致，因为 atomic<double> 不能 fetch_add，用 store 近似）
    double cur_sum = impl_->sum_error.load(std::memory_order_relaxed) + sample.error_ratio;
    impl_->sum_error.store(cur_sum, std::memory_order_relaxed);
    double cur_max = impl_->max_error.load(std::memory_order_relaxed);
    if (sample.error_ratio > cur_max) impl_->max_error.store(sample.error_ratio, std::memory_order_relaxed);
    double cur_min = impl_->min_error.load(std::memory_order_relaxed);
    if (sample.error_ratio < cur_min) impl_->min_error.store(sample.error_ratio, std::memory_order_relaxed);
}

std::vector<UtilizationSample> ActualTracker::recent(std::size_t n) const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (impl_->samples.size() <= n) return impl_->samples;
    return std::vector<UtilizationSample>(impl_->samples.end() - static_cast<std::ptrdiff_t>(n),
                                           impl_->samples.end());
}

std::vector<UtilizationSample> ActualTracker::all() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    return impl_->samples;
}

std::size_t ActualTracker::sample_count() const noexcept {
    return impl_->total_count.load(std::memory_order_relaxed);
}

double ActualTracker::average_error() const noexcept {
    std::size_t n = impl_->total_count.load(std::memory_order_relaxed);
    if (n == 0) return 0.0;
    return impl_->sum_error.load(std::memory_order_relaxed) / static_cast<double>(n);
}

double ActualTracker::max_error() const noexcept {
    return impl_->max_error.load(std::memory_order_relaxed);
}

double ActualTracker::min_error() const noexcept {
    return impl_->min_error.load(std::memory_order_relaxed);
}

std::string ActualTracker::status_json() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    std::ostringstream os;
    os << "{";
    os << "\"sample_count\":" << impl_->total_count.load(std::memory_order_relaxed);
    os << ",\"capacity\":" << impl_->capacity;
    os << ",\"average_error\":" << average_error();
    os << ",\"max_error\":" << impl_->max_error.load(std::memory_order_relaxed);
    os << ",\"min_error\":" << impl_->min_error.load(std::memory_order_relaxed);
    os << ",\"recent\":[";
    std::size_t show = std::min<std::size_t>(impl_->samples.size(), 10);
    for (std::size_t i = 0; i < show; ++i) {
        if (i > 0) os << ",";
        const auto& s = impl_->samples[impl_->samples.size() - show + i];
        os << "{\"timestamp_ns\":" << s.timestamp_ns;
        os << ",\"actual\":" << s.actual_ratio;
        os << ",\"target\":" << s.target_ratio;
        os << ",\"error\":" << s.error_ratio;
        os << ",\"backend\":\"" << s.backend << "\"}";
    }
    os << "]}";
    return os.str();
}

} // namespace astro::compute::utilization
